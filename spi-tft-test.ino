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

#define AD_RESOLUTION 12
volatile float vBatterry = 0.0f;
volatile float temperature = 0.0f;
int maxSpeed = 0;

// GPS Mutex
auto_init_mutex(_gpsMutex);

//---------------------------------------------------------------------------------------------------------------------------------------------------------
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

bool traffiAlarmActive = false;

/**
 *
 */
void processTraffiAlarm() {

    static long lastAlarm = millis();

    // Aktív az alarm?
    if (traffiAlarmActive) {

        // De még nem járt le az időzítése?
        if (!timeHasPassed(lastAlarm, ALARM_TIME_MS)) {
            return;
        }

        // Aktív az alarm, és le is járt az időzítése -> töröljük a viewport-ot és az alarm flag-et is
        tft.resetViewport();
        tft.fillRect(100, 20, 280, 60, TFT_BLACK); // töröljük a viewport területét
        traffiAlarmActive = false;
        lastAlarm = millis();
        return;

    } else {
        // Nem aktív az alarm, de meghívtak -> Bekapcsoljuk az alarm flag-et és a viewport-ot is
        tft.setViewport(100, 20, 280, 60, true);
        tft.frameViewport(TFT_RED, 3);             // 3 pixel keret a viewport-on belül
        tft.fillRect(100, 20, 280, 60, TFT_BLACK); // töröljük a viewport területét
        traffiAlarmActive = true;
    }

    // Alarm aktualizálása
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_RED);
    tft.drawString("TRAFFIPAX ALARM!!!", 150, 30, 2);
}

/**
 * Fejléc feliratok
 */
void displayHeaderText() {

    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

#define HEADER_TEXT_Y 6
    tft.drawString("Time/Date", 250, HEADER_TEXT_Y, 2);
    tft.drawString("Altitude", 455, HEADER_TEXT_Y, 2);
    tft.drawString("Hdop", 75, 100, 2);
    tft.drawString("Max Speed", 400, 100, 2);
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

    char buf[11];

    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    // Műholdak száma
    short sats = gps.satellites.isValid() && gps.satellites.age() < GPS_DATA_MAX_AGE ? gps.satellites.value() : 0;
    ringMeter(&tft, sats,
              SATS_RINGETER_MIN, // min
              SATS_RINGETER_MAX, // max
              0,                 // xpos
              8,                 // ypos
              40,                // radius
              230,               // angle
              true,              // coloredValue
              "Sats",            // Text
              RED2GREEN);

    // Magasság
    int alt = gps.satellites.isValid() && gps.altitude.age() < GPS_DATA_MAX_AGE ? gps.altitude.meters() : 0;
    sprintf(buf, "%4d", alt);
    tft.setTextPadding(14 * 4);
    tft.drawString(buf, 450, 30, 4);

    if (!traffiAlarmActive) {
        // Idő
        if (gps.time.isValid() && gps.time.age() < GPS_DATA_MAX_AGE) {
            int hours = gps.time.hour();
            int mins = gps.time.minute();
            dls.correctTime(mins, hours, gps.date.day(), gps.date.month(), gps.date.year());
            sprintf(buf, "%02d:%02d:%02d", hours, mins, gps.time.second());
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextPadding(tft.textWidth(buf, 6));
            tft.drawString(buf, 250, 45, 6);
        }

        // Dátum
        if (gps.date.isValid() && gps.date.age() < GPS_DATA_MAX_AGE) {
            sprintf(buf, "%04d-%02d-%02d", gps.date.year(), gps.date.month(), gps.date.day());
            tft.setTextSize(1);
            tft.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
            tft.setTextPadding(tft.textWidth(buf, 2));
            tft.drawString(buf, 250, 70, 2);
        }
    }

    // Hdop
    double hdop = gps.satellites.isValid() && gps.hdop.age() < GPS_DATA_MAX_AGE ? gps.hdop.hdop() : 0;
    sprintf(buf, "%.2f", hdop);
    tft.setTextPadding(5 * 14);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(buf, 75, 120, 4);

    // Sebesség
    int speedValue = gps.speed.isValid() && gps.speed.age() < GPS_DATA_MAX_AGE && gps.speed.kmph() >= 4 ? gps.speed.kmph() : 0;
    ringMeter(&tft,
              speedValue,                                 // val
              0,                                          // minValue
              SPEED_RINGMETER_MAX_VALUE,                  // maxValue
              (tft.width() / 2 - SPEED_RINGMETER_RADIUS), // x
              100,                                        // y
              SPEED_RINGMETER_RADIUS,                     // radius
              230,                                        // angle
              true,                                       // coloredValue
              "km/h",                                     // felirat
              GREEN2RED);                                 // scheme

    // Max Speed
    maxSpeed = MAX(maxSpeed, speedValue);
    sprintf(buf, "%3d", maxSpeed);
    tft.setTextPadding(3 * 14);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(buf, 400, 120, 4);

#define VERTICAL_BARS_Y 290
    // Vertical Line bar - Batterry
    verticalLinearMeter(&tft,
                        "Batt [V]",        // category
                        vBatterry,         // val
                        BATT_BARMETER_MIN, // minVal
                        BATT_BARMETER_MAX, // maxVal
                        0,                 // x
                        VERTICAL_BARS_Y,   // bottom-left-y
                        30,                // bar-w
                        10,                // bar-h
                        2,                 // gap
                        10,                // n
                        BLUE2RED);         // color

    // Vertical Line bar - temperature
    verticalLinearMeter(&tft,
                        "Temp [C]",        // category
                        temperature,       // val
                        TEMP_BARMETER_MIN, // minVal
                        TEMP_BARMETER_MAX, // maxVal
                        tft.width() - 30,  // x = maxX - bar-w
                        VERTICAL_BARS_Y,   // bottom-left-y
                        30,                // bar-w
                        10,                // bar-h
                        2,                 // gap
                        10,                // n
                        BLUE2RED,          // color
                        true);             // bal oldalt legyenek az értékek

    // // Traffipax alarm aktív?
    // if (traffiAlarmActive) {
    //     processTraffiAlarm();
    // }

    // // Test alarm 30mp-enként
    // static long testTrafiAlarm = millis();
    // if (timeHasPassed(testTrafiAlarm, 30000)) {
    //     processTraffiAlarm();
    //     testTrafiAlarm = millis();
    // }
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
        temperature = readtemperature();
        lastReadSensors = millis();
    }
}

/**
 * GPS adatok kiolvasása
 */
void readGPS() {

    // Kiolvassuk és dekódoljuk az összes GPS adatot
    while (Serial2.available() > 0) {
        char c = Serial2.read();

        // LED villogtatása, ha van érvéynes bejövő GPS mondat
        if (gps.encode(c)) {
            digitalWrite(LED_BUILTIN, HIGH);
            delayMicroseconds(500);
            digitalWrite(LED_BUILTIN, LOW);
        }
    }
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
float readtemperature() {
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

    // DS18B20 Hőmérsékletmérő szenzor, felbontásban követjük a belső AD-t
    ds18B20.begin();
    ds18B20.setResolution(AD_RESOLUTION);
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