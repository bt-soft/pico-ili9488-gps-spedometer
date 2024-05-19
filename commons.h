#ifndef __COMMONS_H__
#define __COMMONS_H__

#define __DEBUG_ON_SERIAL__

/**
 * Az ILI9488 esetén az RGB színek tipikusan 565 formátumban vannak tárolva:
 *      5 bit a vörös
 *      6 bit a zöld
 *      5 bit a kék
 */
#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#endif