#include "cockpit/rpmsg/sensor_client.hpp"
#include "../kernel/sensor_uapi.h"
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cerrno>
#include <stdexcept>
namespace cockpit::rpmsg {
namespace {
std::uint64_t now_ms(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
std::uint64_t random_id(){std::uint64_t id=0;if(getrandom(&id,sizeof(id),0)!=sizeof(id)||!id)throw std::runtime_error("sensor getrandom failed");return id;}
class DeviceTransport final:public SensorTransport {
 int fd_{-1},wake_{-1};std::string path_;
public:
 explicit DeviceTransport(std::string path):path_(std::move(path)){wake_=eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);if(wake_<0)throw std::runtime_error("sensor eventfd");}
 ~DeviceTransport()override{close();::close(wake_);}
 bool open()override{if(fd_>=0)return true;fd_=::open(path_.c_str(),O_RDWR|O_NONBLOCK|O_CLOEXEC);return fd_>=0;}
 LinkState state()override{sensor_link_state s{};if(fd_<0||ioctl(fd_,SENSOR_IOC_STATE,&s)||s.abi!=SENSOR_ABI||s.reserved||s.removed)return {};return {s.owner_ready!=0,s.generation,s.sample_overwrites,s.control_drops,s.malformed,s.send_failures};}
 bool send(const std::vector<std::uint8_t>& wire)override{return fd_>=0&&::write(fd_,wire.data(),wire.size())==static_cast<ssize_t>(wire.size());}
 bool receive(std::vector<std::uint8_t>& wire,unsigned wait_ms)override{
  pollfd fds[2]={{fd_,POLLIN,0},{wake_,POLLIN,0}};
  int rc=::poll(fds,2,static_cast<int>(wait_ms));if(rc<0&&errno!=EINTR)throw std::runtime_error("sensor poll");
  if(fds[1].revents&POLLIN){std::uint64_t value;const auto ignored=::read(wake_,&value,sizeof(value));(void)ignored;return false;}
  if(fds[0].revents&(POLLERR|POLLHUP|POLLNVAL))throw std::runtime_error("sensor endpoint lost");
  if(!(fds[0].revents&POLLIN))return false;
  wire.resize(SV_MAX_WIRE);auto len=::read(fd_,wire.data(),wire.size());
  if(len<0&&errno==EAGAIN){wire.clear();return false;}
  if(len<0)throw std::runtime_error("sensor read");
  wire.resize(static_cast<size_t>(len));return len>0;
 }
 void wake()override{std::uint64_t one=1;const auto ignored=::write(wake_,&one,sizeof(one));(void)ignored;}
 void close()override{if(fd_>=0){::close(fd_);fd_=-1;}}
};
}
std::unique_ptr<SensorTransport> make_device_transport(const std::string& path){return std::make_unique<DeviceTransport>(path);}
SensorRuntime::SensorRuntime(std::unique_ptr<SensorTransport> transport,std::uint64_t core,SensorClient::Callback cb,std::function<std::uint64_t()> random):transport_(std::move(transport)),core_(core),random_(random?std::move(random):random_id),callback_(std::move(cb)){}
SensorRuntime::~SensorRuntime(){stop();}
bool SensorRuntime::start(){if(thread_.joinable())return true;if(!transport_||!core_)return false;try{stopping_=false;thread_=std::thread(&SensorRuntime::run,this);return true;}catch(...){client_.reset();return false;}}
void SensorRuntime::stop(){stopping_=true;if(transport_)transport_->wake();if(thread_.joinable())thread_.join();client_.reset();}
void SensorRuntime::run(){
 struct CloseGuard { SensorTransport* transport;~CloseGuard(){try{transport->close();}catch(...){}} } guard{transport_.get()};
 try {
 const auto start=now_ms();std::uint64_t retry=0;bool attached=false;
 while(!stopping_&&now_ms()-start<840000){
  auto now=now_ms();
  if(!attached){if(client_)client_->poll(now);if(now<retry){std::this_thread::sleep_for(std::chrono::milliseconds(20));continue;}
   if(!transport_->open()){retry=now+1000;continue;}
   const auto link=transport_->state();if(!link.ready){transport_->close();retry=now+1000;continue;}
   client_=std::make_unique<SensorClient>(core_,random_(),random_(),callback_);
   client_->attach(link,now);attached=true;
  }
  try {
   const auto link=transport_->state();if(!link.ready||link.generation!=client_->snapshot().generation)throw std::runtime_error("owner lost");
   client_->link_counters(link);
   auto out=client_->take_outgoing();if(!out.empty()&&!transport_->send(out))throw std::runtime_error("sensor trysend");
   std::vector<std::uint8_t> wire;if(transport_->receive(wire,50))client_->receive(wire,link.generation,now_ms());
   client_->poll(now_ms());
   if(client_->snapshot().data==vehicle::SensorDataCondition::OFFLINE)throw std::runtime_error("sensor timeout");
  }catch(...){client_->disconnect();transport_->close();attached=false;retry=now_ms()+1000;}
 }
 // Bounded best effort UNSUB; no lease renewal, no automatic claim or retry during exit.
 if(attached){client_->stop(now_ms());auto out=client_->take_outgoing();if(!out.empty()&&transport_->send(out)){
  auto end=now_ms()+250;while(now_ms()<end&&!client_->unsubscribe_confirmed()){std::vector<std::uint8_t> wire;try{if(transport_->receive(wire,20))client_->receive(wire,client_->snapshot().generation,now_ms());}catch(...){break;}}
 }}
 if(client_)client_->disconnect();
 }catch(...){
  if(client_)client_->disconnect();
  else if(callback_){vehicle::SensorState failed;failed.data=vehicle::SensorDataCondition::OFFLINE;try{callback_(failed);}catch(...){}}
 }
}
}
