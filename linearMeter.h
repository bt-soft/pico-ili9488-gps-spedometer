#ifndef __LINEARMETER_H__
#define __LINEARMETER_H__

#include "commons.h"

#define TFT_BORDER_COLOR RGB565(70, 70, 70)

/***************************************************************************************
** Function name:           rainbowColor
** Description:             Return a 16 bit rainbow colour
***************************************************************************************/
// If 'spectrum' is in the range 0-159 it is converted to a spectrum colour
// from 0 = red through to 127 = blue to 159 = violet
// Extending the range to 0-191 adds a further violet to red band

uint16_t rainbowColor(uint8_t spectrum) {
    spectrum = spectrum % 192;

    uint8_t red = 0;   // Red is the top 5 bits of a 16 bit colour spectrum
    uint8_t green = 0; // Green is the middle 6 bits, but only top 5 bits used here
    uint8_t blue = 0;  // Blue is the bottom 5 bits

    uint8_t sector = spectrum >> 5;
    uint8_t amplit = spectrum & 0x1F;

    switch (sector) {
    case 0:
        red = 0x1F;
        green = amplit; // Green ramps up
        blue = 0;
        break;
    case 1:
        red = 0x1F - amplit; // Red ramps down
        green = 0x1F;
        blue = 0;
        break;
    case 2:
        red = 0;
        green = 0x1F;
        blue = amplit; // Blue ramps up
        break;
    case 3:
        red = 0;
        green = 0x1F - amplit; // Green ramps down
        blue = 0x1F;
        break;
    case 4:
        red = amplit; // Red ramps up
        green = 0;
        blue = 0x1F;
        break;
    case 5:
        red = 0x1F;
        green = 0;
        blue = 0x1F - amplit; // Blue ramps down
        break;
    }

    return red << 11 | green << 6 | blue;
}
// #########################################################################
//  Draw the linear meter
// #########################################################################
// val =  reading to show (range is 0 to n)
// x, y = position of top left corner
// w, h = width and height of a single bar
// g    = pixel gap to next bar (can be 0)
// n    = number of segments
// s    = colour scheme
void linearMeter(TFT_eSPI *tft, int val, int x, int y, int w, int h, int g, int n, byte s) {

    // Variable to save "value" text colour from scheme and set default
    int colour = TFT_BLUE;

    // Draw n colour blocks
    for (int b = 1; b <= n; b++) {
        if (val > 0 && b <= val) { // Fill in coloured blocks
            switch (s) {
            case 0:
                colour = TFT_RED;
                break; // Fixed colour
            case 1:
                colour = TFT_GREEN;
                break; // Fixed colour
            case 2:
                colour = TFT_BLUE;
                break; // Fixed colour
            case 3:
                colour = rainbowColor(map(b, 0, n, 127, 0));
                break; // Blue to red
            case 4:
                colour = rainbowColor(map(b, 0, n, 63, 0));
                break; // Green to red
            case 5:
                colour = rainbowColor(map(b, 0, n, 0, 63));
                break; // Red to green
            case 6:
                colour = rainbowColor(map(b, 0, n, 0, 159));
                break; // Rainbow (red to violet)
            }
            tft->fillRect(x + b * (w + g), y, w, h, colour);
        } else { // Fill in blank segments

            tft->fillRect(x + b * (w + g), y, w, h, TFT_DARKGREY);
        }
    }
}

/**
 *
 */
void verticalLinearMeter(TFT_eSPI *tft, float val, float minVal, float maxVal, int x, int y, int w, int h, int g, int n, byte s, boolean mirrored = false) {

    int barVal = map(val, minVal, maxVal, 1, n);

    int colour = TFT_DARKGREY;

    for (int b = 1; b <= n; b++) {

        colour = TFT_DARKGREY;

        if (b <= barVal) { // Fill in coloured blocks
            switch (s) {
            case 0:
                colour = TFT_RED;
                break; // Fixed colour
            case 1:
                colour = TFT_GREEN;
                break; // Fixed colour
            case 2:
                colour = TFT_BLUE;
                break; // Fixed colour
            case 3:
                colour = rainbowColor(map(b, 0, n, 127, 0));
                break; // Blue to red
            case 4:
                colour = rainbowColor(map(b, 0, n, 63, 0));
                break; // Green to red
            case 5:
                colour = rainbowColor(map(b, 0, n, 0, 63));
                break; // Red to green
            case 6:
                colour = rainbowColor(map(b, 0, n, 0, 159));
                break; // Rainbow (red to violet)
            }
        }
        tft->fillRect(x, y - b * (h + g), w, h, colour);             // Bar
        tft->fillRect(x + w + 5, y - b * (h + g), 20, h, TFT_BLACK); // korábbi érték törlése

        // Koordináta érték megjelenítése
        char buffer[10]; // Karaktertömb a szöveg tárolásához

        // A két szélsőérték megjelenítése
        if (b == 1 || b == n) {
            sprintf(buffer, "%.1f", b == 1 ? minVal : maxVal); // Szám értékének formázása és karaktertömbbe írása
        } else if (b <= barVal && b + 1 > barVal) {
            sprintf(buffer, "%.1f", val); // Szám értékének formázása és karaktertömbbe írása
        } else {
            memset(buffer, '\0', sizeof(buffer));
        }

        int16_t textWidth = tft->textWidth(buffer, 1); // Szöveg szélességének kiszámítása
        tft->setTextPadding(textWidth);                // Szélesség beállítása a paddinghez
        tft->setTextColor(TFT_WHITE, TFT_BLACK);       // Fehér szöveg fekete háttérrel
        // tft->drawString(buffer, x + (w / 2) - (textWidth / 2), y - (b * (h + g) / 2), 1); // Szöveg rajzolása bele a bar közepébe
        tft->drawString(buffer, x + w + 15, y - (b * (h + g)) + h / 2, 1); // Szöveg rajzolása
    }
}

// #########################################################################
//  Draw a linear meter on the screen
// #########################################################################
void plotLinear(TFT_eSPI *tft, char *label, int x, int y) {
#define LINEAR_METER_WIDTH 36

    tft->drawRect(x, y, LINEAR_METER_WIDTH, 155, TFT_BORDER_COLOR); // keret

    tft->fillRect(x + 2, y + 19, LINEAR_METER_WIDTH - 3, 155 - 38, TFT_WHITE);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawCentreString(label, x + LINEAR_METER_WIDTH / 2, y + 2, 2);

    for (int i = 0; i < 110; i += 10) {
        tft->drawFastHLine(x + 20, y + 27 + i, 6, TFT_BLACK);
    }

    for (int i = 0; i < 110; i += 50) {
        tft->drawFastHLine(x + 20, y + 27 + i, 9, TFT_BLACK);
    }

    tft->fillTriangle(x + 3, y + 127, x + 3 + 16, y + 127, x + 3, y + 127 - 5, TFT_RED);
    tft->fillTriangle(x + 3, y + 127, x + 3 + 16, y + 127, x + 3, y + 127 + 5, TFT_RED);

    tft->drawCentreString("---", x + LINEAR_METER_WIDTH / 2, y + 155 - 18, 2);
}

#endif