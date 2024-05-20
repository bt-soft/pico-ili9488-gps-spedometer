#ifndef __COMMONS_H__
#define __COMMONS_H__

#define __DEBUG_ON_SERIAL__

// Színsémák
#define RED2RED 0
#define GREEN2GREEN 1
#define BLUE2BLUE 2
#define BLUE2RED 3
#define GREEN2RED 4
#define RED2GREEN 5
#define RED2VIOLET 6

// Font méretek
#define FONT1_WIDTH 8
#define FONT2_WIDTH 16
#define FONT4_WIDTH 26
#define FONT6_WIDTH 48
#define FONT8_WIDTH 75

/**
 * Az ILI9488 esetén az RGB színek tipikusan 565 formátumban vannak tárolva:
 *      5 bit a vörös
 *      6 bit a zöld
 *      5 bit a kék
 */
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

// #define TFT_GREY 0x2104 // Dark grey 16-bit colour
#define TFT_GREY RGB565(50, 50, 50)
#define TFT_BORDER_COLOR RGB565(70, 70, 70)

// Speed RingMeter max érték
#define SPEED_RINGMETER_MAX_VALUE 240
#define SPEED_RINGMETER_RADIUS 140

#endif