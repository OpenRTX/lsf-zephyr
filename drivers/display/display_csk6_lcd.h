#ifndef __DISPLAY_CSK6_LCD_H__
#define __DISPLAY_CSK6_LCD_H__

#include "csk6_cm33/include/spi.h"

/**
 * @brief initialze the display csk6 lcd
 *
 * @param name spi device name, just support 'SPI_0' and 'SPI_1'
 * @param dma_ch dma channel number
 *
 * @return void* display handler
 */
void *display_csk6_lcd_init(const char *name, uint8_t dma_ch);

/**
 * @brief send lcd display datas
 * 
 * @param handler display handler
 * @param buff pointer to display buffer
 * @param h display area height
 * @param w display area width
 * @param rotate >= 1 means rotate 90 degress
 * @return int32_t 
 */
int32_t display_csk6_lcd_display_datas_send(void *handler, const uint8_t *buff, uint32_t h, uint32_t w,
					    uint8_t rotate);

/**
 * @brief  send lcd cmd datas
 * 
 * @param spi display handler
 * @param datas pointer to cmd buffer
 * @param len number of bytes to send
 * @return int32_t 
 */
int32_t display_csk6_lcd_cmd_datas_send(void *handler, uint8_t *datas, uint32_t len);

#endif
