#ifndef WS2812_H_
#define WS2812_H_
//--------------------------------------------------
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
//--------------------------------------------------
#define DELAY_LEN 48
#define LED_COUNT 144
#define ARRAY_LEN DELAY_LEN + LED_COUNT*24
#define HIGH 65
#define LOW 26
//--------------------------------------------------
#define BitIsSet(reg, bit) ((reg & (1<<bit)) != 0)
//--------------------------------------------------
void ws2812_init(void);
void ws2812_pixel_rgb_to_buf_dma(uint8_t Rpixel , uint8_t Gpixel, uint8_t Bpixel, uint16_t posX);
//--------------------------------------------------
#endif /* WS2812_H_ */
