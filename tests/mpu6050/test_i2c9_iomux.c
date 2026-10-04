#include "fake_i2c9_platform.h"
#define GPIO_BANK0 0
#define GPIO_BANK1 1
#define GPIO_BANK2 2
#define GPIO_PIN_D5 32
#define GPIO_PIN_C4 16
#define GPIO_PIN_B2 4
#define GPIO_PIN_B3 8
#define GPIO_PIN_B4 16
#define GPIO_PIN_B5 32
#define GPIO_PIN_D0 1
#define GPIO_PIN_D1 2
#define PIN_CONFIG_MUX_FUNC0 0
#define PIN_CONFIG_MUX_FUNC10 10
#define PIN_CONFIG_MUX_FUNC11 11
#define PIN_CONFIG_MUX_FUNC12 12
static int mux_calls;
static void HAL_PINCTRL_SetIOMUX(int b,int p,int m){(void)b;(void)p;(void)m;mux_calls++;}
static void i2c7_m1_iomux_config(void){mux7++;}
#include "board_iomux.c"
int main(void){rt_hw_iomux_config();assert(mux7==0&&mux_calls==0);puts("PASS actual board iomux: enabling I2C9 does not mux I2C7 or any sensor pin");return 0;}
