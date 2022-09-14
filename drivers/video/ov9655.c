/*
 * Copyright (c) 2022, listenai
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ovti_ov9655

#include <zephyr.h>
#include <device.h>

#include <drivers/video.h>
#include <drivers/i2c.h>
#include <drivers/gpio.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ov9655);

/** 
  * @brief  OV9655 ID  
  */  
#define  OV9655_ID    0x96

/* OV9655 Registers definition */
#define OV9655_GAIN_CTRL                       0x00U
#define OV9655_AWB_BLUE_GAIN_CTRL              0x01U
#define OV9655_AWB_RED_GAIN_CTRL               0x02U
#define OV9655_VERTIVCAL_FRAME_CTRL            0x03U
#define OV9655_COMMON_CTRL1                    0x04U
#define OV9655_UB_AVERAGE_LEVEL                0x05U
#define OV9655_YGB_AVERAGE_LEVEL               0x06U
#define OV9655_GR_AVERAGE_LEVEL                0x07U
#define OV9655_VR_AVERAGE_LEVEL                0x08U
#define OV9655_COMMON_CTRL2                    0x09U   
#define OV9655_PID_NUMBER_HIGH                 0x0AU
#define OV9655_PID_NUMBER_LOW                  0x0BU
#define OV9655_COMMON_CTRL3                    0x0CU
#define OV9655_COMMON_CTRL4                    0x0DU
#define OV9655_COMMON_CTRL5                    0x0EU
#define OV9655_COMMON_CTRL6                    0x0FU
#define OV9655_EXPOSURE_VALUE                  0x10U
#define OV9655_INTERNAL_CLOCK                  0x11U
#define OV9655_COMMON_CTRL7                    0x12U
#define OV9655_COMMON_CTRL8                    0x13U
#define OV9655_COMMON_CTRL9                    0x14U
#define OV9655_COMMON_CTRL10                   0x15U
#define OV9655_REG16_CTRL                      0x16U
#define OV9655_HORIZONTAL_FRAME_START          0x17U
#define OV9655_HORIZONTAL_FRAME_STOP           0x18U
#define OV9655_VERTICAL_FRAME_START            0x19U
#define OV9655_VERTICAL_FRAME_STOP             0x1AU
#define OV9655_PSHFT                           0x1BU
#define OV9655_MANUFACTURER_ID_HIGH            0x1CU
#define OV9655_MANUFACTURER_ID_LOW             0x1DU
#define OV9655_MIRROR_VFLIP                    0x1EU    
#define OV9655_LAEC                            0x1FU /* Reserved */
#define OV9655_BOS                             0x20U /* B Channel ADBLC Result: Offset adjustment sign  */
#define OV9655_GBOS                            0x21U /* GB Channel ADBLC Result: Offset adjustment sign */
#define OV9655_GROS                            0x22U /* GR Channel ADBLC Result: Offset adjustment sign */
#define OV9655_ROS                             0x23U /* R Channel ADBLC Result: Offset adjustment sign  */
#define OV9655_AEW                             0x24U /* AGC/AEC - Stable Operating Region (Upper Limit) */
#define OV9655_AEB                             0x25U /* AGC/AEC - Stable Operating Region (Lower Limit) */
#define OV9655_VPT                             0x26U /* AGC/AEC Fast Mode Operating Region              */
#define OV9655_BBIAS                           0x27U /* B Channel Signal Output Bias                    */
#define OV9655_GBBIAS                          0x28U /* GB Channel Signal Output Bias                   */
#define OV9655_PREGAIN                         0x29U /* RGB Channel Pre-gain                            */
#define OV9655_EXHCH                           0x2AU /* Dummy Pixel Insert MSB                          */                                                  
#define OV9655_EXHCL                           0x2BU /* Dummy Pixel Insert LSB                          */
#define OV9655_RBIAS                           0x2CU /* R Channel Signal Output Bias                    */
#define OV9655_ADVFL                           0x2DU /* LSB of insert dummy lines in vertical direction */
#define OV9655_ADVFH                           0x2EU /* MSB of insert dummy lines in vertical direction */
#define OV9655_YG_AVERAGE_VALUE                0x2FU /* Y/G Channel Average Value                       */
#define OV9655_HSYNC_RISING_EDGE_DELAY         0x30U 
#define OV9655_HSYNC_FALLING_EDGE_DELAY        0x31U
#define OV9655_HORIZONTAL_FRAME_CTRL           0x32U
#define OV9655_CHLF_CTRL                       0x33U /* ADC Reference Control Array Current Control     */
#define OV9655_AREF1                           0x34U /* Array Reference Control                         */
#define OV9655_AREF2                           0x35U /* Array Reference Control                         */
#define OV9655_AREF3                           0x36U /* Array Reference Control                         */                                                
#define OV9655_ADC1                            0x37U /* ADC Control 1                                   */ 
#define OV9655_ADC2                            0x38U /* ADC Reference Control                           */
#define OV9655_AREF4                           0x39U /* Analog Reference Control                        */
#define OV9655_TSLB                            0x3AU /* Line Buffer Test Option */
#define OV9655_COMMON_CTRL11                   0x3BU
#define OV9655_COMMON_CTRL12                   0x3CU
#define OV9655_COMMON_CTRL13                   0x3DU                                                 
#define OV9655_COMMON_CTRL14                   0x3EU
#define OV9655_EDGE                            0x3FU
#define OV9655_COMMON_CTRL15                   0x40U
#define OV9655_COMMON_CTRL16                   0x41U
#define OV9655_COMMON_CTRL17                   0x42U
#define OV9655_MATRIX_COEFFICIENT_1            0x4FU
#define OV9655_MATRIX_COEFFICIENT_2            0x50U
#define OV9655_MATRIX_COEFFICIENT_3            0x51U                                                 
#define OV9655_MATRIX_COEFFICIENT_4            0x52U
#define OV9655_MATRIX_COEFFICIENT_5            0x53U
#define OV9655_MATRIX_COEFFICIENT_6            0x54U
#define OV9655_BRIGHTNESS_ADJUSTMENT           0x55U
#define OV9655_CONTRAST_COEFFICIENT_1          0x56U
#define OV9655_CONTRAST_COEFFICIENT_2          0x57U
#define OV9655_MATRIX_COEFFICIENT_SIGN         0x58U
#define OV9655_AWB_CTRL_OPTION_1               0x59U
#define OV9655_AWB_CTRL_OPTION_2               0x5AU
#define OV9655_AWB_CTRL_OPTION_3               0x5BU
#define OV9655_AWB_CTRL_OPTION_4               0x5CU
#define OV9655_AWB_CTRL_OPTION_5               0x5DU
#define OV9655_AWB_CTRL_OPTION_6               0x5EU
#define OV9655_AWB_BLUE_GAIN_LIMIT             0x5FU
#define OV9655_AWB_RED_GAIN_LIMIT              0x60U
#define OV9655_AWB_GREEN_GAIN_LIMIT            0x61U
#define OV9655_LENS_CORRECTION_OPTION_1        0x62U
#define OV9655_LENS_CORRECTION_OPTION_2        0x63U
#define OV9655_LENS_CORRECTION_OPTION_3        0x64U
#define OV9655_LENS_CORRECTION_OPTION_4        0x65U
#define OV9655_LENS_CORRECTION_CTRL            0x66U
#define OV9655_MANU                            0x67U /* Manual U Value                            */
#define OV9655_MANV                            0x68U /* Manual V Value                            */
#define OV9655_BD50MAX                         0x6AU /* 50 Hz Banding Filter Maximum Step Setting */
#define OV9655_DBLV                            0x6BU /* Band Gap Reference Adjustment             */
#define OV9655_DNSTH                           0x70U /* De-noise Function Threshold Adjustment    */
#define OV9655_POIDX                           0x72U /* Pixel Output Index                        */
#define OV9655_PCKDV                           0x73U /* Pixel Clock Output Selection              */
#define OV9655_XINDX                           0x74U /* Horizontal Scaling Down Coefficients      */
#define OV9655_YINDX                           0x75U /* Vertical Scaling Down Coefficients        */
#define OV9655_SLOP                            0x7AU /* Gamma Curve Highest Segment Slope         */
#define OV9655_GAMMA_1                         0x7BU /* Gamma Curve 1st Segment Input End Point 0x010 Output Value */
#define OV9655_GAMMA_2                         0x7CU /* Gamma Curve 2nd Segment Input End Point 0x020 Output Value */
#define OV9655_GAMMA_3                         0x7DU /* Gamma Curve 3rd Segment Input End Point 0x040 Output Value */
#define OV9655_GAMMA_4                         0x7EU /* Gamma Curve 4th Segment Input End Point 0x080 Output Value */
#define OV9655_GAMMA_5                         0x7FU /* Gamma Curve 5th Segment Input End Point 0x0A0 Output Value */
#define OV9655_GAMMA_6                         0x80U /* Gamma Curve 6th Segment Input End Point 0x0C0 Output Value */     
#define OV9655_GAMMA_7                         0x81U /* Gamma Curve 7th Segment Input End Point 0x0E0 Output Value */   
#define OV9655_GAMMA_8                         0x82U /* Gamma Curve 8th Segment Input End Point 0x100 Output Value */
#define OV9655_GAMMA_9                         0x83U /* Gamma Curve 9th Segment Input End Point 0x120 Output Value */
#define OV9655_GAMMA_10                        0x84U /* Gamma Curve 10th Segment Input End Point 0x140 Output Value */
#define OV9655_GAMMA_11                        0x85U /* Gamma Curve 11th Segment Input End Point 0x180 Output Value */
#define OV9655_GAMMA_12                        0x86U /* Gamma Curve 12th Segment Input End Point 0x1C0 Output Value */
#define OV9655_GAMMA_13                        0x87U /* Gamma Curve 13th Segment Input End Point 0x240 Output Value */
#define OV9655_GAMMA_14                        0x88U /* Gamma Curve 14th Segment Input End Point 0x2C0 Output Value */
#define OV9655_GAMMA_15                        0x89U /* Gamma Curve 15th Segment Input End Point 0x340 Output Value */
#define OV9655_COMMON_CTRL18                   0x8BU              
#define OV9655_COMMON_CTRL19                   0x8CU              
#define OV9655_COMMON_CTRL20                   0x8DU              
#define OV9655_DMLNL                           0x92U /* Frame Dummy Line LSBs */
#define OV9655_DMLNH                           0x93U /* Frame Dummy Line MSBs */
#define OV9655_LENS_CORRECTION_OPTION_6        0x9DU
#define OV9655_LENS_CORRECTION_OPTION_7        0x9EU
#define OV9655_AECH                            0xA1U /* Exposure Value - AEC MSB 5 bits   */                  
#define OV9655_BD50                            0xA2U /* 1/100s Exposure Setting for 50 Hz Banding Filter */
#define OV9655_BD60                            0xA3U /* 1/120s Exposure Setting for 60 Hz Banding Filter */
#define OV9655_COMMON_CTRL21                   0xA4U
#define OV9655_GREEN                           0xA6U /* AWB Green Component Gain Setting */
#define OV9655_VZST                            0xA7U /* VGA Zoom Mode Vertical Start Line */
#define OV9655_REFA8                           0xA8U /* Analog Reference Control */
#define OV9655_REFA9                           0xA9U /* Analog Reference Control */              
#define OV9655_BLC_1                           0xACU /* Black Level Control 1 */              
#define OV9655_BLC_2                           0xADU /* Black Level Control 2 */              
#define OV9655_BLC_3                           0xAEU /* Black Level Control 3 */
#define OV9655_BLC_4                           0xAFU /* Black Level Control 4 */
#define OV9655_BLC_5                           0xB0U /* Black Level Control 5 */
#define OV9655_BLC_6                           0xB1U /* Black Level Control 6 */
#define OV9655_BLC_7                           0xB2U /* Black Level Control 7 */
#define OV9655_BLC_8                           0xB3U /* Black Level Control 8 */
#define OV9655_CTRLB4                          0xB4U
#define OV9655_FRSTL                           0xB7U /* One-Pin Frame Exposure Reset Time Control Low 8 Bits */
#define OV9655_FRSTH                           0xB8U /* One-Pin Frame Exposure Reset Time Control High 8 Bits */
#define OV9655_ADBOFF                          0xBCU /* ADC B channel offset setting */
#define OV9655_ADROFF                          0xBDU /* ADC R channel offset setting */
#define OV9655_ADGbOFF                         0xBEU /* ADC Gb channel offset setting */                                                  
#define OV9655_ADGrOFF                         0xBFU /* ADC Gr channel offset setting */
#define OV9655_OV9655_COMMON_CTRL23            0xC4U
#define OV9655_BD60MAX                         0xC5U /* 60 Hz Banding Filter Maximum Step Setting */
#define OV9655_OV9655_COMMON_CTRL24            0x07U

struct ov9655_data {
    const struct i2c_dt_spec i2c_bus;
    const struct gpio_dt_spec reset_gpios;
	const struct gpio_dt_spec pwdn_gpios;
	struct video_format fmt;
};

struct ov9655_reg {
	uint8_t addr;
	uint8_t value;
};

/* Initialization sequence for QVGA resolution (320x240) in YUV format */
static const struct ov9655_reg OV9655_QVGA[] = {
	{ 0x12, 0x80 },{ 0x00, 0x00 },{ 0x01, 0x80 },{ 0x02, 0x80 },{ 0x03, 0x02 },{ 0x04, 0x03 },{ 0x0e, 0x61 },
	{ 0x0f, 0x42 },{ 0x11, 0x01 },{ 0x12, 0x62 },{ 0x13, 0xe7 },{ 0x14, 0x3a },{ 0x15, 0x00 },{ 0x16, 0x24 },{ 0x17, 0x18 },
	{ 0x18, 0x04 },{ 0x19, 0x01 },{ 0x1a, 0x81 },{ 0x1e, 0x04 },{ 0x24, 0x3c },{ 0x25, 0x36 },{ 0x26, 0x72 },
	{ 0x27, 0x08 },{ 0x28, 0x08 },{ 0x29, 0x15 },{ 0x2a, 0x00 },{ 0x2b, 0x00 },{ 0x2c, 0x08 },{ 0x32, 0x24 },
	{ 0x33, 0x00 },{ 0x34, 0x3f },{ 0x35, 0x00 },{ 0x36, 0x3a },{ 0x38, 0x72 },{ 0x39, 0x57 },{ 0x3a, 0x08},
	{ 0x3b, 0x04 },{ 0x3d, 0x99 },{ 0x3e, 0x0e },{ 0x3f, 0xc1 },{ 0x40, 0xc0 },{ 0x41, 0x01 },{ 0x42, 0xc0 },
	{ 0x43, 0x0a },{ 0x44, 0xf0 },{ 0x45, 0x46 },{ 0x46, 0x62 },{ 0x47, 0x2a },{ 0x48, 0x3c },{ 0x4a, 0xfc },
	{ 0x4b, 0xfc },{ 0x4c, 0x7f },{ 0x4d, 0x7f },{ 0x4e, 0x7f },{ 0x52, 0x28 },{ 0x53, 0x88 },{ 0x54, 0xb0 },
	{ 0x4f, 0x98 },{ 0x50, 0x98 },{ 0x51, 0x00 },{ 0x58, 0x1a },{ 0x59, 0x85 },{ 0x5a, 0xa9 },{ 0x5b, 0x64 },
	{ 0x5c, 0x84 },{ 0x5d, 0x53 },{ 0x5e, 0x0e },{ 0x5f, 0xf0 },{ 0x60, 0xf0 },{ 0x61, 0xf0 },{ 0x62, 0x00 },
	{ 0x63, 0x00 },{ 0x64, 0x02 },{ 0x65, 0x20 },{ 0x66, 0x00 },{ 0x69, 0x0a },{ 0x6b, 0x5a },{ 0x6c, 0x04 },
	{ 0x6d, 0x55 },{ 0x6e, 0x00 },{ 0x6f, 0x9d },{ 0x70, 0x21 },{ 0x71, 0x78 },{ 0x72, 0x11 },{ 0x73, 0x01 },
	{ 0x74, 0x10 },{ 0x75, 0x10 },{ 0x76, 0x01 },{ 0x77, 0x02 },{ 0x7a, 0x12 },{ 0x7b, 0x08 },{ 0x7c, 0x15 },
	{ 0x7d, 0x24 },{ 0x7e, 0x45 },{ 0x7f, 0x55 },{ 0x80, 0x6a },{ 0x81, 0x78 },{ 0x82, 0x87 },{ 0x83, 0x96 },
	{ 0x84, 0xa3 },{ 0x85, 0xb4 },{ 0x86, 0xc3 },{ 0x87, 0xd6 },{ 0x88, 0xe6 },{ 0x89, 0xf2 },{ 0x8a, 0x24 },
	{ 0x8c, 0x80 },{ 0x90, 0x7d },{ 0x91, 0x7b },{ 0x9d, 0x02 },{ 0x9e, 0x02 },{ 0x9f, 0x7a },{ 0xa0, 0x79 },
	{ 0xa1, 0x40 },{ 0xa4, 0x50 },{ 0xa5, 0x68 },{ 0xa6, 0x4a },{ 0xa8, 0xc1 },{ 0xa9, 0xef },{ 0xaa, 0x92 },
	{ 0xab, 0x04 },{ 0xac, 0x80 },{ 0xad, 0x80 },{ 0xae, 0x80 },{ 0xaf, 0x80 },{ 0xb2, 0xf2 },{ 0xb3, 0x20 },
	{ 0xb4, 0x20 },{ 0xb5, 0x00 },{ 0xb6, 0xaf },{ 0xbb, 0xae },{ 0xbc, 0x7f },{ 0xbd, 0x7f },{ 0xbe, 0x7f },
	{ 0xbf, 0x7f },{ 0xc0, 0xaa },{ 0xc1, 0xc0 },{ 0xc2, 0x01 },{ 0xc3, 0x4e },{ 0xc6, 0x05 },{ 0xc7, 0x81 },
	{ 0xc9, 0xe0 },{ 0xca, 0xe8 },{ 0xcb, 0xf0 },{ 0xcc, 0xd8 },{ 0xcd, 0x93 },{ 0xcd, 0x93 },{ 0xFF, 0xFF }
};


  /* Initialization sequence for VGA resolution (640x480) in YUV(VYUY) format*/
static const struct ov9655_reg OV9655_VGA[]=
{
    {0x00, 0x00},
    {0x01, 0x80},
    {0x02, 0x80},
    {0xb5, 0x00},
    {0x35, 0x00},
    {0xa8, 0xc1},
    {0x3a, 0xc8},
    {0x3d, 0x99},
    {0x77, 0x02},
    {0x13, 0xe7},
    {0x26, 0x72},
    {0x27, 0x08},
    {0x28, 0x08},
    {0x2c, 0x08},
    {0xab, 0x04},
    {0x6e, 0x00},
    {0x6d, 0x55},
    {0x00, 0x11},
    {0x10, 0x7b},
    {0xbb, 0xae},
    {0x11, 0x03},
    {0x72, 0x00},
    {0x3e, 0x0c},
    {0x74, 0x3a},
    {0x76, 0x01},
    {0x75, 0x35},
    {0x73, 0x00},
    {0xc7, 0x80},
    {0x62, 0x00},
    {0x63, 0x00},
    {0x64, 0x02},
    {0x65, 0x20},
    {0x66, 0x01},
    {0xc3, 0x4e},
    {0x33, 0x00},
    {0xa4, 0x50},
    {0xaa, 0x92},
    {0xc2, 0x01},
    {0xc1, 0xc8},
    {0x1e, 0x04},
    {0xa9, 0xef},
    {0x0e, 0x61},
    {0x39, 0x57},
    {0x0f, 0x48},
    {0x24, 0x3c},
    {0x25, 0x36},
    {0x12, 0x62},
    {OV9655_HORIZONTAL_FRAME_CTRL, 0xff},
    {OV9655_HORIZONTAL_FRAME_START, 0x16},
    {OV9655_HORIZONTAL_FRAME_STOP, 0x02},
    {OV9655_VERTIVCAL_FRAME_CTRL, 0x12},
    {OV9655_VERTICAL_FRAME_START, 0x01},
    {OV9655_VERTICAL_FRAME_STOP, 0x3d},
    {0x36, 0xfa},
    {0x69, 0x0a},
    {0x8c, 0x8d},
    {0xc0, 0xaa},
    {0x40, 0xc0},
    {0x43, 0x14},
    {0x44, 0xf0},
    {0x45, 0x46},
    {0x46, 0x62},
    {0x47, 0x2a},
    {0x48, 0x3c},
    {0x59, 0x85},
    {0x5a, 0xa9},
    {0x5b, 0x64},
    {0x5c, 0x84},
    {0x5d, 0x53},
    {0x5e, 0x0e},
    {0x6c, 0x0c},
    {0xc6, 0x85},
    {0xcb, 0xf0},
    {0xcc, 0xd8},
    {0x71, 0x78},
    {0xa5, 0x68},
    {0x6f, 0x9e},
    {0x42, 0xc0},
    {0x3f, 0x82},
    {0x8a, 0x23},
    {0x14, 0x3a},
    {0x3b, 0xcc},
    {0x34, 0x3d},
    {0x41, 0x40},
    {0xc9, 0xe0},
    {0xca, 0xe8},
    {0xcd, 0x93},
    {0x7a, 0x20},
    {0x7b, 0x1c},
    {0x7c, 0x28},
    {0x7d, 0x3c},
    {0x7e, 0x5a},
    {0x7f, 0x68},
    {0x80, 0x76},
    {0x81, 0x80},
    {0x82, 0x88},
    {0x83, 0x8f},
    {0x84, 0x96},
    {0x85, 0xa3},
    {0x86, 0xaf},
    {0x87, 0xc4},
    {0x88, 0xd7},
    {0x89, 0xe8},
    {0x4f, 0x98},
    {0x50, 0x98},
    {0x51, 0x00},
    {0x52, 0x28},
    {0x53, 0x70},
    {0x54, 0x98},
    {0x58, 0x1a},
    {0x6b, 0x5a},
    {0x90, 0x92},
    {0x91, 0x92},
    {0x9f, 0x90},
    {0xa0, 0x90},
    {0x16, 0x24},
    {0x2a, 0x00},
    {0x2b, 0x00},
    {0xac, 0x80},
    {0xad, 0x80},
    {0xae, 0x80},
    {0xaf, 0x80},
    {0xb2, 0xf2},
    {0xb3, 0x20},
    {0xb4, 0x20},
    {0xb6, 0xaf},
    {0x29, 0x15},
    {0x9d, 0x02},
    {0x9e, 0x02},
    {0x9e, 0x02},
    {0x04, 0x03},
    {0x05, 0x2e},
    {0x06, 0x2e},
    {0x07, 0x2e},
    {0x08, 0x2e},
    {0x2f, 0x2e},
    {0x4a, 0xe9},
    {0x4b, 0xdd},
    {0x4c, 0xdd},
    {0x4d, 0xdd},
    {0x4e, 0xdd},
    {0x70, 0x06},
    {0xa6, 0x40},
    {0xbc, 0x02},
    {0xbd, 0x01},
    {0xbe, 0x02},
    {0xbf, 0x01},
};


static int ov9655_write_reg(const struct device *dev, uint8_t reg_addr,
			    uint8_t value)
{
	struct ov9655_data *drv_data = dev->data;
	struct i2c_msg msgs;
	uint8_t tx_buf[2];

	tx_buf[0] = reg_addr;
	tx_buf[1] = value;

	msgs.buf = tx_buf;
	msgs.len = 2;
	msgs.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer(drv_data->i2c_bus.bus, &msgs, 1, drv_data->i2c_bus.addr);
}

static int ov9655_read_reg(const struct device *dev, uint8_t reg_addr,
			   uint8_t *value)
{
	struct ov9655_data *drv_data = dev->data;
	struct i2c_msg msgs[2];

	msgs[0].buf = (uint8_t *)&reg_addr;
	msgs[0].len = 1;
	/*
	 * When using I2C to read the registers of the SCCB device,
	 * a stop bit is required after writing the register address
	 */
	msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	msgs[1].buf = (uint8_t *)value;
	msgs[1].len = 1;
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_STOP | I2C_MSG_RESTART;

	return i2c_transfer(drv_data->i2c_bus.bus, msgs, 2, drv_data->i2c_bus.addr);
}

int ov9655_modify_reg(const struct device *dev,
		      uint8_t reg_addr,
		      uint8_t clear_mask,
		      uint8_t value)
{
	int ret;
	uint8_t set_value;

	ret = ov9655_read_reg(dev, reg_addr, &set_value);

	if (ret == 0) {
		set_value = (set_value & (~clear_mask)) |
				(set_value & clear_mask);
		ret = ov9655_write_reg(dev, reg_addr, set_value);
	}


	return ret;
}

static int ov9655_write_all(const struct device *dev,
			    const struct ov9655_reg *regs,
			    uint16_t reg_num)
{
	uint16_t i = 0;
	int err;

	for (i = 0; i < reg_num; i++) {
		err = ov9655_write_reg(dev, regs[i].addr, regs[i].value);
		k_sleep(K_MSEC(1));
		if (err) {
			return err;
		}
	}

	return 0;
}

static int ov9655_set_fmt(const struct device *dev,
			  enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	int ret = 0;

	if (fmt->pixelformat == VIDEO_PIX_FMT_VYUY && fmt->height == 240 &&
	    fmt->width == 320) {
		/* Configure Sensor */
		ret = ov9655_write_all(dev, OV9655_QVGA,
					ARRAY_SIZE(OV9655_QVGA));
		if (ret) {
			LOG_ERR("Unable to write ov9655 QVGA config");
			return ret;
		}		

		return 0;
	}

	if (fmt->pixelformat == VIDEO_PIX_FMT_VYUY && fmt->height == 480 &&
	    fmt->width == 640) {	

		ret = ov9655_write_all(dev, OV9655_VGA,
					ARRAY_SIZE(OV9655_VGA));
		if (ret) {
			LOG_ERR("Unable to write ov9655 VGA config");
			return ret;
		}	

		return 0;
	}
	
	LOG_ERR("video format unsupported \n");
    return -ENOTSUP;
}

static int ov9655_get_fmt(const struct device *dev,
			  enum video_endpoint_id ep,
			  struct video_format *fmt)
{
	return 0;
}

static int ov9655_stream_start(const struct device *dev)
{
	return 0;
}

static int ov9655_stream_stop(const struct device *dev)
{
	return 0;
}

static const struct video_format_cap fmts[] = {
	{
		.pixelformat = VIDEO_PIX_FMT_VYUY,
		.width_min = 320,
		.width_max = 320,
		.height_min = 240,
		.height_max = 240,
		.width_step = 0,
		.height_step = 0,
	},
	{
		.pixelformat = VIDEO_PIX_FMT_VYUY,
		.width_min = 640,
		.width_max = 640,
		.height_min = 480,
		.height_max = 480,
		.width_step = 0,
		.height_step = 0,
	},
	{ 0 }
};

static int ov9655_get_caps(const struct device *dev,
			   enum video_endpoint_id ep,
			   struct video_caps *caps)
{
	caps->format_caps = fmts;
	return 0;
}

#if CONFIG_VIDEO_OV9655_MANUAL_UV
static int ov9655_set_fixed_UV_value(const struct device *dev)
{
	uint8_t value = 0;
	ov9655_read_reg(dev, OV9655_TSLB, &value);
	ov9655_write_reg(dev, OV9655_TSLB, value | (1 << 4));
	ov9655_write_reg(dev, 0x67, CONFIG_VIDEO_OV9655_MANUAL_U_VALUE);
	ov9655_write_reg(dev, 0x68, CONFIG_VIDEO_OV9655_MANUAL_V_VALUE);

	//wait stable
	k_sleep(K_MSEC(1));
	return 0;
}
#endif

static int ov9655_init(const struct device *dev)
{
	struct ov9655_data *drv_data = dev->data;
	int ret = 0;
	uint8_t pid;

	if (i2c_configure(drv_data->i2c_bus.bus,
			I2C_SPEED_SET(I2C_SPEED_STANDARD) | I2C_MODE_MASTER)) {
		LOG_ERR("I2C config failed!");
		return -EIO;
	}

	//check chip id
	ret = ov9655_read_reg(dev, OV9655_PID_NUMBER_HIGH, &pid);
	if (ret) {
		LOG_ERR("Unable to read ov9655 pid");
		return -ENODEV;
	}

	if(pid != OV9655_ID){
		LOG_ERR("ov9655 read PID err 0x%x", pid);
		return -ENODEV;		
	}

	/* Prepare the camera to be configured by resetting all its registers */
	ov9655_write_reg(dev, OV9655_COMMON_CTRL7, 0x80);
	k_sleep(K_MSEC(10));

#if CONFIG_VIDEO_OV9655_MANUAL_UV
	ov9655_set_fixed_UV_value(dev);
#endif

	return 0;
}

static int ov9655_set_ctrl(const struct device *dev,
				unsigned int cid, void *value)
{
	int ret = 0;

	switch (cid) {	
	/*
		该sensor只有在mclk有输入时钟的情况下，i2c才能正常通信，所以并不能在内核设备初始化流程中进行sensor初始化。
		目前是通过set_ctrl来提供sensor初始化的api.
	*/
	case 0:
		ret = ov9655_init(dev);
		break;
	default:
		return -ENOTSUP;
	}

	return ret;
}

static const struct video_driver_api ov9655_driver_api = {
	.set_format = ov9655_set_fmt,
	.get_format = ov9655_get_fmt,
	.get_caps = ov9655_get_caps,
	.stream_start = ov9655_stream_start,
	.stream_stop = ov9655_stream_stop,
	.set_ctrl = ov9655_set_ctrl,
};

static int ov9655_init_0(const struct device *dev)
{
	struct ov9655_data *drv_data = dev->data;
	int ret = 0;

	if(drv_data->pwdn_gpios.port){
		gpio_pin_configure_dt(&drv_data->pwdn_gpios, GPIO_OUTPUT_ACTIVE);
		gpio_pin_set_dt(&drv_data->pwdn_gpios, 0);
	}

	if (drv_data->reset_gpios.port) {
        gpio_pin_configure_dt(&drv_data->reset_gpios, GPIO_OUTPUT_ACTIVE);
        gpio_pin_set_dt(&drv_data->reset_gpios, 0);
		k_sleep(K_MSEC(3));
        gpio_pin_set_dt(&drv_data->reset_gpios, 1);
		k_sleep(K_MSEC(1));
	}

	return ret;
}

static struct ov9655_data ov9655_data_0 =
{
    .i2c_bus = I2C_DT_SPEC_GET(DT_DRV_INST(0)),

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), reset_gpios)
    .reset_gpios = GPIO_DT_SPEC_GET(DT_DRV_INST(0), reset_gpios),
#endif

#if DT_NODE_HAS_PROP(DT_DRV_INST(0), pwdn_gpios)
	.pwdn_gpios = GPIO_DT_SPEC_GET(DT_DRV_INST(0), pwdn_gpios),
#endif
};

DEVICE_DT_INST_DEFINE(0, &ov9655_init_0, NULL,
		    &ov9655_data_0, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &ov9655_driver_api);
