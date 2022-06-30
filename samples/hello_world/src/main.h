/*
 * main.h
 *
 *  Created on: Nov. 6, 2020
 *      Author: bauldeng
 */

#ifndef __AUD_IN_AUD_OUT_MAIN_H
#define __AUD_IN_AUD_OUT_MAIN_H

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "venus_cp.h"
#include "IOMuxManager.h"
#include "venus_log.h"
#include "Driver_I2S.h"
#include "Driver_ADC_PDM.h"
#include "Driver_DAC.h"
#include "Driver_GPIO.h"

//----------------------------------------------------------------------------
#define IOMUX_V4    4
#define IOMUX_V5    5
#define IOMUX_VER   IOMUX_V5 // IOMUX_V4


#if (IOMUX_VER == IOMUX_V4)

// I2S2 Pin
#define I2S2_MCLK   5 // GPIOB, 5, 1
#define I2S2_BCK    6 // GPIOB, 6, 1
#define I2S2_LRCK   7 // GPIOB, 7, 1
#define I2S2_DOUT   8 // GPIOB, 8, 1
#define I2S2_DIN    9 // GPIOB, 9, 1

// I2S2 Pin function select
#define I2S2_FUNC   1 // function 1

// I2S2 Pin group
#define I2S2_GROUP  CSK_IOMUX_PAD_B

// I2S1 Pin
#define I2S1_MCLK   0 // GPIOB, 0, 2
#define I2S1_BCK    6 // GPIOB, 6, 2
#define I2S1_LRCK   7 // GPIOB, 7, 2
#define I2S1_DOUT   8 // GPIOB, 8, 2
#define I2S1_DIN    9 // GPIOB, 9, 2

// I2S1 Pin function select
#define I2S1_FUNC   2 // function 2

// I2S1 Pin group
#define I2S1_GROUP  CSK_IOMUX_PAD_B

// I2S0 Pin
#define I2S0_MCLK   I2S1_MCLK
#define I2S0_BCK    1 // GPIOB, 1, 1
#define I2S0_LRCK   2 // GPIOB, 2, 1
#define I2S0_DIN    3 // GPIOB, 3, 1

// I2S0 Pin function select
#define I2S0_FUNC   1 // function 1 (except MCLK)

// I2S0 Pin group
#define I2S0_GROUP  CSK_IOMUX_PAD_B

// I2C0 Pin
#define I2C0_SCL    17 // GPIOA, 17, 6
#define I2C0_SDA    18 // GPIOA, 18, 6

// I2C0 Pin function select
#define I2C0_FUNC   6 // function 6

// I2C0 Pin group
#define I2C0_GROUP  CSK_IOMUX_PAD_A

// I2C1 Pin
#define I2C1_SCL    17 // GPIOA, 17, 7
#define I2C1_SDA    18 // GPIOA, 18, 7

// I2C1 Pin function select
#define I2C1_FUNC   7 // function 7

// I2C1 Pin group
#define I2C1_GROUP  CSK_IOMUX_PAD_A

#elif (IOMUX_VER == IOMUX_V5)

//-----------------------------------
// I2S2 Pin
#define I2S2_MCLK   15 // GPIOA, 15, 15
#define I2S2_BCK    16 // GPIOA, 16, 15
#define I2S2_LRCK   17 // GPIOA, 17, 15
#define I2S2_DIN    18 // GPIOA, 18, 15
#define I2S2_DOUT   9 // GPIOA, 9, 15

//// I2S2 Pin
//#define I2S2_MCLK   5 // GPIOA, 5, 15
//#define I2S2_BCK    6 // GPIOA, 6, 15
//#define I2S2_LRCK   7 // GPIOA, 7, 15
//#define I2S2_DIN    8 // GPIOA, 8, 15
//#define I2S2_DOUT   9 // GPIOA, 9, 15

//// I2S2 Pin
//#define I2S2_MCLK   15 // GPIOA, 15, 15
//#define I2S2_BCK    16 // GPIOA, 16, 15
//#define I2S2_LRCK   17 // GPIOA, 17, 15
//#define I2S2_DIN    18 // GPIOA, 18, 15
//#define I2S2_DOUT   19 // GPIOA, 19, 15

// I2S2 Pin function select
#define I2S2_FUNC   15 // function 15

// I2S2 Pin group
#define I2S2_GROUP  CSK_IOMUX_PAD_A

//-----------------------------------
// I2S1 Pin
#define I2S1_MCLK   6  // GPIOA, 6, 14
#define I2S1_BCK    16 // GPIOA, 16, 14
#define I2S1_LRCK   17 // GPIOA, 17, 14
#define I2S1_DIN    18 // GPIOA, 18, 14
#define I2S1_DOUT   15 // GPIOA, 15, 14

// I2S1 Pin function select
#define I2S1_FUNC   14 // function 14

// I2S1 Pin group
#define I2S1_GROUP  CSK_IOMUX_PAD_A

//// I2S1 Pin
//#define I2S1_MCLK   6 // GPIOA, 6, 14
//#define I2S1_BCK    7 // GPIOA, 7, 14
//#define I2S1_LRCK   8 // GPIOA, 8, 14
//#define I2S1_DIN    4 // GPIOA, 4, 14
//#define I2S1_DOUT   5 // GPIOA, 5, 14
//
//// I2S1 Pin function select
//#define I2S1_FUNC   14 // function 14
//
//// I2S1 Pin group
//#define I2S1_GROUP  CSK_IOMUX_PAD_A

//// I2S1 Pin
//#define I2S1_MCLK   0 // GPIOB, 0, 14
//#define I2S1_BCK    8 // GPIOB, 8, 14
//#define I2S1_LRCK   7 // GPIOB, 7, 14
//#define I2S1_DIN    6 // GPIOB, 6, 14
//#define I2S1_DOUT   1 // GPIOB, 1, 14
//
//// I2S1 Pin function select
//#define I2S1_FUNC   14 // function 14
//
//// I2S1 Pin group
//#define I2S1_GROUP  CSK_IOMUX_PAD_B

//-----------------------------------
// I2S0 Pin

//#define I2S0_MCLK   6  // GPIOA, 6, 14
//#define I2S0_MCLK_GROUP     CSK_IOMUX_PAD_A
//#define I2S0_MCLK_FUNC      14 // function 14 (same as I2S1 MCLK)
//
//#define I2S0_BCK    2 // GPIOB, 2, 15
//#define I2S0_LRCK   1 // GPIOB, 1, 15
//#define I2S0_DIN    0 // GPIOB, 0, 15


#define I2S0_MCLK   0  // GPIOB, 0, 15
#define I2S0_MCLK_GROUP     CSK_IOMUX_PAD_B
#define I2S0_MCLK_FUNC      14 // function 14 (same as I2S1 MCLK)

#define I2S0_BCK    8 // GPIOB, 8, 15
#define I2S0_LRCK   7 // GPIOB, 7, 15
#define I2S0_DIN    6 // GPIOB, 6, 15


// I2S0 Pin function select
#define I2S0_FUNC       15 // function 15 (except MCLK)

// I2S0 Pin group
#define I2S0_GROUP  CSK_IOMUX_PAD_B


//NOTE: I2C0 and I2C1 cannot coexist in the following pin configuration!!
//-----------------------------------
// I2C0 Pin

#define I2C0_SCL    4 // GPIOA, 4, 8
#define I2C0_SDA    5 // GPIOA, 5, 8

//#define I2C0_SCL    6 // GPIOA, 6, 8
//#define I2C0_SDA    7 // GPIOA, 7, 8

// I2C0 Pin function select
#define I2C0_FUNC   8 // function 8

// I2C0 Pin group
#define I2C0_GROUP  CSK_IOMUX_PAD_A

//-----------------------------------
// I2C1 Pin

#define I2C1_SCL    7 // GPIOA, 7, 9
#define I2C1_SDA    6 // GPIOA, 6, 9

//#define I2C1_SCL    5 // GPIOA, 5, 9
//#define I2C1_SDA    4 // GPIOA, 4, 9

// I2C1 Pin function select
#define I2C1_FUNC   9 // function 9

// I2C1 Pin group
#define I2C1_GROUP  CSK_IOMUX_PAD_A

//-----------------------------------
// DMIC Pin

//#define DMIC01_CLK    8     // GPIOA, 8, 17
//#define DMIC01_DAT    13    // GPIOA, 13, 17
#define DMIC01_CLK    16    // GPIOA, 16, 17
#define DMIC01_DAT    17    // GPIOA, 17, 17

//#define DMIC23_CLK    18    // GPIOA, 18, 17
#define DMIC23_CLK    2     // GPIOA, 2, 17
#define DMIC23_DAT    7     // GPIOA, 7, 17

// DMIC01/DMIC23 Pin function select
#define DMIC_FUNC   17      // function 17

// DMIC01/DMIC23 Pin group
#define DMIC_GROUP  CSK_IOMUX_PAD_A

#endif // V5


//----------------------------------------------------------------------------
//I2C device operations
void * get_i2c_dev();
bool init_i2c_dev(void *i2c_dev);
bool i2c_write(void *i2c_dev, uint32_t addr, const uint8_t* data, uint32_t num, uint32_t max_wait_ms);
bool i2c_read(void *i2c_dev, uint32_t addr, uint8_t* data, uint32_t num, uint32_t max_wait_ms);

//I2S/ADC_PDM/DAC functions
bool trigger_i2s_in();
bool trigger_i2s_out();
bool trigger_adc_dmic_in();
bool trigger_dac_out();

bool i2s_oneshot_play();
bool i2s_oneshot_record();
bool dmic_oneshot_record();
bool adc_oneshot_record();
bool dac_oneshot_play();

//I2S/ADC_PDM/DAC callback functions
void cb_i2s_in_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_out_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_inout_event(uint32_t event_info, uint32_t usr_param);
void cb_adc_dmic_in_event(uint32_t event_info, uint32_t usr_param);
void cb_dac_out_event(uint32_t event_info, uint32_t usr_param);

void cb_i2s_oneshot_play_event(uint32_t event_info, uint32_t usr_param);
void cb_i2s_oneshot_record_event(uint32_t event_info, uint32_t usr_param);
void cb_dmic_oneshot_record_event(uint32_t event_info, uint32_t usr_param);
void cb_adc_oneshot_record_event(uint32_t event_info, uint32_t usr_param);
void cb_dac_oneshot_play_event(uint32_t event_info, uint32_t usr_param);


// init I2S IN device (also init ES7210 codec if mic_in_bmp != 0)
void * init_i2s_es7210_in(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t mic_in_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);
// init I2S IN device (also init WM8960 codec if mic_in_bmp != 0)
void * init_i2s_wm8960_in(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t mic_in_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);
// init I2S OUT device (also init WM8960 codec if spk_out_bmp != 0)
void * init_i2s_wm8960_out(bool i2s_master, uint16_t i2s_ch_bmp, uint16_t spk_out_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);

// init I2S IN/OUT device (WM8960 codec)
void * init_i2s_wm8960_inout(bool i2s_master, uint16_t i2s_ch_bmp, uint8_t use_tdm, uint8_t ch_bits,
                        I2S_PROTOCOL i2s_prt, uint32_t sampling_freq, CSK_I2S_SignalEvent_t cb_event);

// init DMIC IN device group (support samp_bits = 24 only)
void * init_dmic_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event);

// init ADC IN device group (support samp_bits = 24 only)
void * init_adc_in(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_ADC_PDM_SignalEvent_t cb_event);

// init DAC OUT device group (support samp_bits = 24 only)
void * init_dac_out(uint8_t dev_bmp, uint8_t use_16bits, uint32_t sampling_freq, CSK_DAC_SignalEvent_t cb_event);

#endif /* __AUD_IN_AUD_OUT_MAIN_H */
