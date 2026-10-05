#include "../../apps/rpmsg_srv/kernel/rk3576_sensor.c"
int main(void){
 struct rpmsg_device device={.dst=0x3005};struct file file={.f_flags=O_NONBLOCK};char buffer[256]={0};loff_t offset=0;
 assert(sensor_module_init()==0);
 wrong_dt=true;assert(sensor_probe(&device)==-EINVAL);wrong_dt=false;
 wrong_pins=true;assert(sensor_probe(&device)==-EINVAL);wrong_pins=false;
 i2c_bound=true;assert(sensor_probe(&device)==-EBUSY);i2c_bound=false;
 fake_mux=9;assert(sensor_probe(&device)==-EINVAL);fake_mux=10;
 assert(sensor_probe(&device)==0);struct sensor_peer*p=dev_get_drvdata(&device.dev);
 file.private_data=&p->misc;assert(sensor_open(NULL,&file)==0);
 struct file second={.private_data=&p->misc};assert(sensor_open(NULL,&second)==-EBUSY);
 buffer[6]=1;buffer[7]=4;
 in_callback=true;sensor_callback(&device,buffer,116,NULL,device.dst);sensor_callback(&device,buffer,116,NULL,device.dst);in_callback=false;
 assert(p->state.sample_overwrites==1);assert(sensor_read(&file,buffer,50,&offset)==-EMSGSIZE);
 assert(sensor_read(&file,buffer,256,&offset)==116);assert(sensor_read(&file,buffer,256,&offset)==-EAGAIN);
 buffer[7]=1;for(unsigned i=0;i<10;i++)sensor_callback(&device,buffer,116,NULL,device.dst);assert(p->count==8&&p->state.control_drops==2);
 copy_fault=true;assert(sensor_read(&file,buffer,256,&offset)==-EFAULT&&p->count==8);copy_fault=false;
 struct sensor_link_state state;assert(!sensor_ioctl(&file,SENSOR_IOC_STATE,(unsigned long)&state)&&state.owner_ready&&state.generation);
 tx_result=-EAGAIN;assert(sensor_write(&file,buffer,116,&offset)==-EAGAIN&&p->state.send_failures==1);
 // A callback acquired its reference before remove; late callback final put only schedules work.
 kref_get(&p->ref);sensor_remove(&device);assert(p->removed);
 assert(sensor_read(&file,buffer,256,&offset)==-ENODEV);assert(sensor_poll(&file,NULL)&EPOLLHUP);
 assert(!sensor_close(NULL,&file));assert(clock_releases==0);
 in_callback=true;kref_put(&p->ref,peer_free);assert(clock_releases==0);in_callback=false;
 flush_workqueue(release_queue);assert(clock_releases==2);assert(allocated==freed);
 sensor_module_exit();puts("SENSOR_KERNEL_ACTUAL_FACTORY_PASS");return 0;
}
