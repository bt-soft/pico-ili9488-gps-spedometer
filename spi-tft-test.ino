#include <Arduino.h>
#include <Streaming.h>
#include <stdio.h>

#include <TinyGPS++.h>
TinyGPSPlus gps;

#include "DayLightSaving.h"
DaylightSaving dls;

#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library with default width and height

#include "commons.h"
#include "linearMeter.h"
#include "ringMeter.h"

// Hőmérés
#define PIN_TEMP_SENSOR 17        /* ATmega328P PIN:4, D10 a DS18B20 bemenete */
#define DS18B20_TEMP_SENSOR_NDX 0 /* Dallas DS18B20 hõmérõ szenzor indexe */
#include <OneWire.h>
#define REQUIRESALARMS false /* nem kell a DallasTemperature ALARM supportja */
#include <DallasTemperature.h>
DallasTemperature ds18B20(new OneWire(PIN_TEMP_SENSOR));

#define AD_RESOLUTION 10

volatile float vBatterry;
volatile float cpuTemperature;

// GPS Mutex
auto_init_mutex(_gpsMutex);

/**
 * Eltelt már annyi idő?
 */
bool timeHasPassed(long fromWhen, int howLong) {
    return millis() - fromWhen >= howLong;
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------
//
//      CORE - 0
//
//---------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------

#define MAX_SPEED 240
#define SPEED_RADIUS 150
#define SPEED_RINGMETER_X (tft.width() / 2 - SPEED_RADIUS)
#define SPEED_RINGMETER_Y 100
#define SPEED_RINGMETER_ANGLE 230

void displaytest() {

    linearMeter(&tft, cpuTemperature,
                10 /* x */, 10 /* y */,
                3 /*w-bar*/, 8 /*h-bar*/,
                3 /*gap*/,
                50 /*segments*/,
                BLUE2RED);

    // Draw a large meter
    static int value = 0;
    static int ramp = 5;

    value += (ramp);
    if (value > MAX_SPEED) {
        ramp = -5;
        delay(1000);
    }
    if (value < 0) {
        ramp = 5;
    }

    ringMeter(&tft, value,
              0 /*min*/, MAX_SPEED /*max*/,
              SPEED_RINGMETER_X /*xpos*/, SPEED_RINGMETER_Y /*ypos*/,
              SPEED_RADIUS /*radius*/, SPEED_RINGMETER_ANGLE /*angle*/,
              true /*coloredValue*/, " km/h", GREEN2RED);
}

/**
 * Fejléc feliratok
 */
void displayHeaderText() {

    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

#define HEADER_TEXT_Y 8
    tft.drawString("Batt", 40, HEADER_TEXT_Y, 1);
    tft.drawString("Sats", 140, HEADER_TEXT_Y, 1);
    tft.drawString("Alt", 200, HEADER_TEXT_Y, 1);
    tft.drawString("Temp", 290, HEADER_TEXT_Y, 1);
    tft.drawString("Time", 430, HEADER_TEXT_Y, 1);

    tft.drawString("Hdop", 445, 280, 1);
}

/**
 * Értékek kiírása
 */
void displayValues() {

    // Lockolunk egyet
    CoreMutex m(&_gpsMutex);

    // Ha nem sikerül a lock, akkor nem megyünk tovább
    if (!m) {
#ifdef __DEBUG_ON_SERIAL__
        Serial << "displayValues: _gpsMutex aktív" << endl;
#endif
        return;
    }

    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    char buf[11];

#define FIRST_LINE_VALUES_Y 55

    // Akku Feszültég
    memset(buf, '\0', sizeof(buf));
    dtostrf(vBatterry, 2, 1, buf);
    tft.setTextPadding(14 * 4);
    tft.drawString(buf, 45, FIRST_LINE_VALUES_Y, 6);

    // Műholdak száma
    short sats = gps.satellites.isValid() && gps.satellites.age() < 3000 ? gps.satellites.value() : 0;
    memset(buf, '\0', sizeof(buf));
    dtostrf(sats, 2, 0, buf);
    tft.setTextPadding(14 * 2);
    tft.drawString(buf, 130, FIRST_LINE_VALUES_Y, 4);

    // Magasság
    int alt = gps.satellites.isValid() && gps.altitude.age() < 3000 ? gps.altitude.meters() : 0;
    memset(buf, '\0', sizeof(buf));
    dtostrf(alt, 4, 0, buf);
    tft.setTextPadding(14 * 4);
    tft.drawString(buf, 190, FIRST_LINE_VALUES_Y, 4);

    // Hőmérséklet
    memset(buf, '\0', sizeof(buf));
    dtostrf(cpuTemperature, 2, 1, buf);
    tft.setTextPadding(14 * 5);
    tft.drawString(buf, 280, FIRST_LINE_VALUES_Y, 4);

    // Dátum
    if (gps.date.isValid() && gps.date.age() < 3000) {
        memset(buf, '\0', sizeof(buf));
        dtostrf(cpuTemperature, 2, 1, buf);
        sprintf(buf, "%04d-%02d-%02d", gps.date.year(), gps.date.month(), gps.date.day());
        tft.setTextPadding(14 * 10);
        tft.drawString(buf, 40, 315, 2);
    }

    // Idő
    if (gps.time.isValid() && gps.time.age() < 3000) {
        int hours = gps.time.hour();
        int mins = gps.time.minute();
        dls.correctTime(mins, hours, gps.date.day(), gps.date.month(), gps.date.year());
        // sprintf(buf, "%02d:%02d:%02d", hours, mins, gps.time.second());
        // tft.setTextPadding(14 * 10);
        sprintf(buf, "%02d:%02d", hours, mins);
        tft.setTextPadding(14 * 8);
        tft.drawString(buf, 420, FIRST_LINE_VALUES_Y, 6);
    }

    // Hdop
    double hdop = gps.satellites.isValid() && gps.hdop.age() < 3000 ? gps.hdop.hdop() : 0;
    memset(buf, '\0', sizeof(buf));
    dtostrf(hdop, 3, 2, buf);
    tft.setTextPadding(14 * 6);
    tft.drawString(buf, 445, 308, 4);

    // Sebesség
    int speedValue = gps.speed.isValid() && gps.speed.age() < 3000 && gps.speed.kmph() > 1 ? gps.speed.kmph() : 0;
    ringMeter(&tft, speedValue,
              0 /*min*/, MAX_SPEED /*max*/,
              SPEED_RINGMETER_X /*xpos*/, SPEED_RINGMETER_Y /*ypos*/,
              SPEED_RADIUS /*radius*/, SPEED_RINGMETER_ANGLE /*angle*/,
              true /*coloredValue*/, " km/h", GREEN2RED);

    // Vertical Line bar - Batterry
    verticalLinearMeter(&tft,
                        vBatterry, // val
                        3,         // minVal
                        6,         // maxVal
                        0,         // x
                        270,       // y
                        30,        // bar-w
                        10,        // bar-h
                        2,         // gap
                        15,        // n
                        BLUE2RED); // color

    // Vertical Line bar - cpuTemperature
    verticalLinearMeter(&tft,
                        cpuTemperature,   // val
                        -15,              // minVal
                        +50,              // maxVal
                        tft.width() - 30, // x = maxX - bar-w
                        270,              // y
                        30,               // bar-w
                        10,               // bar-h
                        2,                // gap
                        15,               // n
                        BLUE2RED,         // color
                        true);            // bal oldalt legyenek az értékek
}

/**
 * Core-0 Setup
 */
void setup(void) {
#ifdef __DEBUG_ON_SERIAL__
    Serial.begin(115200);
#endif

    // TFT
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // plotLinear(&tft, "A0", 0, 160);

    displayHeaderText();
}

/**
 * Core-0 Loop
 */
void loop() {

    // Értékek kiírása
    static long lastDisplay = millis() - 1000;
    if (timeHasPassed(lastDisplay, 1000)) {
        displayValues();
        lastDisplay = millis();
    }
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------
//
//      CORE - 1
//
//---------------------------------------------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------------------------------------------

/**
 *
 */
void readSensorValues() {

    // Lockolunk egyet
    CoreMutex m(&_gpsMutex);

    // Ha nem sikerül a lock, akkor nem megyünk tovább
    if (!m) {
#ifdef __DEBUG_ON_SERIAL__
        Serial << "readGPS: _gpsMutex aktív" << endl;
#endif
        return;
    }

    // GPS adatok olvasása
    readGPS();

    static long lastReadSensors = millis() - 1000;
    if (timeHasPassed(lastReadSensors, 1000)) {
        vBatterry = readBatterry();
        cpuTemperature = readCpuTemperature();
        lastReadSensors = millis();
    }
}

/**
 * GPS adatok kiolvasása
 */
void readGPS() {

    // Kiolvassuk és dekódoljuk az összes GPS adatot
    while (Serial2.available() > 0) {
        gps.encode(Serial2.read());
    }

    gpsDataReceivedLED();
}

/**
 * LED villogtatása, ha van GPS bejövő mondat
 */
void gpsDataReceivedLED() {
#ifdef __DEBUG_ON_SERIAL__
    static bool ledState = false;
    static long stateChanged = millis();

    // Ha eltelt már 10msec és a LED aktív, akkor kikapcsoljuk a LED-et
    if (timeHasPassed(stateChanged, 10) && ledState) {
        ledState = false;
        digitalWrite(LED_BUILTIN, ledState);
        return;
    }

    if (timeHasPassed(stateChanged, 1000)) {
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState);
        stateChanged = millis();
    }
#endif
}

/**
 * Akkumulátor feszültség mérése
 */
float readBatterry() {
#define V_REFERENCE 3.3f
#define R_ATTENNUATOR ((22.0f + 4.69f) / 4.69f)
#define CONVERSION_FACTOR (1 << AD_RESOLUTION)

    // ADC érték átalakítása feszültséggé
    float voltageOut = (analogRead(A3) * V_REFERENCE) / CONVERSION_FACTOR;
    voltageOut -= 0.02; // csalunk egyet, ez a precíz feszültség az osztón
    // Serial << "Vout: " << voltageOut << endl;

    // Eredeti feszültség számítása a feszültségosztó alapján
    return voltageOut * R_ATTENNUATOR;
}

/**
 * CPU beépített hőmérséklet mérő olvasása
 */
float readCpuTemperature() {
    ds18B20.requestTemperaturesByIndex(DS18B20_TEMP_SENSOR_NDX);
    return ds18B20.getTempCByIndex(DS18B20_TEMP_SENSOR_NDX);
}

/**
 * Core-1 Setup
 */
void setup1(void) {
    // GPS Serial
    Serial2.begin(9600);

    // initialize digital pin LED_BUILTIN as an output.
    pinMode(LED_BUILTIN, OUTPUT);

    // AD felbontás beállítása
    analogReadResolution(AD_RESOLUTION);

    // DS18B20 Hőmérsékletmérő szenzor
    ds18B20.begin();
    ds18B20.setResolution(12);
    ds18B20.setWaitForConversion(false);
}

/**
 * Core-1 - GPS process
 */
void loop1(void) {

    // Van GPS adat?
    if (Serial2.available()) {
        readSensorValues();
    }
}