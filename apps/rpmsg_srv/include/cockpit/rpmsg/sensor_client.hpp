#pragma once
#include "cockpit/vehicle/state.hpp"
#include "cockpit/protocol/sensor_v1.h"
#include <functional>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
namespace cockpit::rpmsg {
struct LinkState { bool ready{false}; std::uint64_t generation{0}, overwrites{0}, drops{0}, malformed{0}, send_failures{0}; };
class SensorTransport {
public:
 virtual ~SensorTransport()=default;
 virtual bool open()=0;
 virtual LinkState state()=0;
 virtual bool send(const std::vector<std::uint8_t>&)=0;
 virtual bool receive(std::vector<std::uint8_t>&, unsigned wait_ms)=0;
 virtual void wake()=0;
 virtual void close()=0;
};
class SensorClient {
public:
 using Callback=std::function<void(const vehicle::SensorState&)>;
 SensorClient(std::uint64_t core_epoch, std::uint64_t session, std::uint64_t nonce, Callback callback);
 void attach(LinkState link, std::uint64_t now);
 void receive(const std::vector<std::uint8_t>& wire, std::uint64_t generation, std::uint64_t now);
 void poll(std::uint64_t now);
 void link_counters(LinkState link);
 void disconnect();
 void stop(std::uint64_t now);
 std::vector<std::uint8_t> take_outgoing();
 const vehicle::SensorState& snapshot() const { return state_; }
 bool unsubscribe_confirmed() const { return unsubscribe_confirmed_; }
private:
 enum class Phase { OFFLINE, QUERY, BIND, SUBSCRIBE, ACTIVE, STOPPED };
 void command(std::uint16_t type,std::uint64_t now);
 void publish();
 void fail();
 std::uint64_t core_,session_,nonce_,request_{0},pending_{0},generation_{0},deadline_{0},renew_{0},link_rx_{0},query_at_{0};
 std::uint16_t pending_type_{0};
 Phase phase_{Phase::OFFLINE};
 vehicle::SensorState state_;
 Callback callback_;
 std::vector<std::uint8_t> outgoing_;
 bool unsubscribe_confirmed_{false};
};
std::unique_ptr<SensorTransport> make_device_transport(const std::string& path);
class SensorRuntime {
public:
 SensorRuntime(std::unique_ptr<SensorTransport>,std::uint64_t core_epoch,SensorClient::Callback,
               std::function<std::uint64_t()> random = {});
 ~SensorRuntime();
 bool start();
 void stop();
private:
 void run();
 std::unique_ptr<SensorTransport> transport_;
 std::unique_ptr<SensorClient> client_;
 std::uint64_t core_;
 std::function<std::uint64_t()> random_;
 SensorClient::Callback callback_;
 std::atomic<bool> stopping_{false};
 std::thread thread_;
};
}
