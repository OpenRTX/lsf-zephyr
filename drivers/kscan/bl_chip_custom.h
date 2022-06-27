#ifndef BL_CHIP_CUSTOM_H
#define BL_CHIP_CUSTOM_H
#define     TS_CHIP          BL6XX3
#define     CTP_SLAVE_ADDR		(0x2c)  // 7bit
//#define     CTP_SLAVE_ADDR		(0x2c<<1)
#define     BTL_CHECK_CHIPID

#define     CTP_USE_HW_I2C
// #define     CTP_USE_SW_I2C
// #define     GPIO_EINT
// #define     RESET_PIN_WAKEUP
//#define     INT_PIN_WAKEUP
//#define     NEED_CONFIG_EINT_RESUME
//#define     SWAP_XY
#define     BL_UPDATE_FIRMWARE_ENABLE
//#define   BL_POWER_CONTROL_SUPPORT
#define     TPD_RES_X        240
#define     TPD_RES_Y        240
#define     BL_DEBUG
//#define     BL_DEBUG_SUPPORT
#endif
