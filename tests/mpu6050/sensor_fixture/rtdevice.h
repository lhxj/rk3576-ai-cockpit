#define RT_I2C_WR 0
#define RT_I2C_RD 1
struct rt_i2c_bus_device { int value; };
struct rt_i2c_msg {rt_uint16_t addr,flags,len;rt_uint8_t *buf;};
struct rt_i2c_bus_device *rt_i2c_bus_device_find(const char*);
rt_size_t rt_i2c_transfer(struct rt_i2c_bus_device*,struct rt_i2c_msg*,rt_uint32_t);
