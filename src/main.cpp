#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();

#include "wifi_setup.h"
#include "price_fetcher.h"
#include "display_ui.h"
#include "weather_fetcher.h"
#include "weather_ui.h"

// === Configuration ===
#define BACKLIGHT_PIN 27
#define PRICE_INTERVAL_MS   60000   // Refresh prices every 60s
#define WEATHER_INTERVAL_MS 300000  // Refresh weather every 5min
#define TOTAL_PAGES 2

// === Touch calibration (raw mapping, rotation 1) ===
#define RAW_X_MIN 304
#define RAW_X_MAX 3928
#define RAW_Y_MIN 243
#define RAW_Y_MAX 3909

// === Global objects ===
WiFiSetup wifiSetup;
PriceFetcher fetcher;
DisplayUI ui;
WeatherFetcher wxFetcher;
WeatherUI wxUI;

int currentPage = 0;           // 0=Prices, 1=Weather
unsigned long lastPriceFetch = 0;
unsigned long lastWeatherFetch = 0;
bool priceReady = false;
bool weatherReady = false;
bool needRedraw = true;

// Touch helper
bool getTouch(uint16_t &sx, uint16_t &sy) {
    if (tft.getTouchRawZ() < 300) return false;
    uint16_t rx, ry;
    tft.getTouchRaw(&rx, &ry);
    int x = map(rx, RAW_X_MIN, RAW_X_MAX, 0, 319);
    int y = map(ry, RAW_Y_MIN, RAW_Y_MAX, 0, 239);
    sx = constrain(x, 0, 319);
    sy = constrain(y, 0, 239);
    return true;
}

void drawCurrentPage() {
    bool wifiOk = WiFi.status() == WL_CONNECTED;

    if (currentPage == 0) {
        if (priceReady) {
            ui.drawPriceDashboard(fetcher.data, 0, TOTAL_PAGES);
        } else {
            ui.drawLoading(0, TOTAL_PAGES);
        }
    } else {
        // Draw header from DisplayUI (shared style)
        tft.fillScreen(TFT_BLACK);
        ui.drawHeader("Weather", wifiOk, 1, TOTAL_PAGES);
        if (weatherReady) {
            wxUI.drawWeatherPage(wxFetcher.data);
        } else {
            wxUI.drawLoading();
        }
    }
    needRedraw = false;
}

void setup() {
    Serial.begin(115200);
    Serial.println("CYD Price Tracker starting...");

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // Splash screen
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextFont(4);
    tft.setCursor(30, 50);
    tft.println("Price Tracker");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(30, 100);
    tft.println("Gold | Oil | Crypto | FX");
    tft.setCursor(30, 125);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Weather | AQI | Earthquake");
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(100, 200);
    tft.println("ESP32 CYD 2.4\"");
    delay(2000);

    // WiFi setup
    wifiSetup.init();
    wifiSetup.runSetup();

    // NTP sync (UTC+7 Thailand)
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Syncing NTP...");
    struct tm t;
    int retry = 0;
    while (!getLocalTime(&t) && retry < 10) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    Serial.println(retry < 10 ? " OK" : " Failed");

    // Init modules
    ui.init();
    wxFetcher.init();
    ui.drawLoading(0, TOTAL_PAGES);
}

void loop() {
    unsigned long now = millis();

    // === Fetch price data on interval ===
    if (!priceReady || (now - lastPriceFetch >= PRICE_INTERVAL_MS)) {
        if (WiFi.status() != WL_CONNECTED) {
            wifiSetup.connectWiFi(10);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Fetching prices...");
            if (fetcher.fetchAll()) {
                Serial.println("Prices OK");
                priceReady = true;
                if (currentPage == 0) needRedraw = true;
            } else {
                Serial.println("Price fetch failed: " + fetcher.data.error_msg);
            }
        }
        lastPriceFetch = now;
    }

    // === Fetch weather data on interval ===
    if (!weatherReady || (now - lastWeatherFetch >= WEATHER_INTERVAL_MS)) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Fetching weather...");
            if (wxFetcher.fetchAll()) {
                Serial.println("Weather OK");
                weatherReady = true;
                if (currentPage == 1) needRedraw = true;
            } else {
                Serial.println("Weather fetch failed: " + wxFetcher.data.error_msg);
            }
        }
        lastWeatherFetch = now;
    }

    // === Redraw if needed ===
    if (needRedraw) {
        drawCurrentPage();
    }

    // === Touch input ===
    uint16_t tx, ty;
    if (getTouch(tx, ty)) {
        if (ty < 26) {
            // Header area: tap left half = switch page, right half = refresh
            if (tx < 170) {
                // Switch page
                currentPage = (currentPage + 1) % TOTAL_PAGES;
                needRedraw = true;
                Serial.printf("Switched to page %d\n", currentPage);
            } else {
                // Force refresh current page
                Serial.println("Force refresh");
                if (currentPage == 0) {
                    ui.drawLoading(0, TOTAL_PAGES);
                    fetcher.fetchAll();
                    priceReady = fetcher.data.valid;
                    lastPriceFetch = millis();
                } else {
                    tft.fillScreen(TFT_BLACK);
                    ui.drawHeader("Weather", true, 1, TOTAL_PAGES);
                    wxUI.drawLoading();
                    wxFetcher.fetchAll();
                    weatherReady = wxFetcher.data.valid;
                    lastWeatherFetch = millis();
                }
                needRedraw = true;
            }
        }
        delay(300);  // Debounce
    }

    // === Update header clock every 30 seconds ===
    static unsigned long lastClockUpdate = 0;
    if (now - lastClockUpdate > 30000) {
        bool wifiOk = WiFi.status() == WL_CONNECTED;
        const char* titles[] = {"Prices", "Weather"};
        ui.drawHeader(titles[currentPage], wifiOk, currentPage, TOTAL_PAGES);
        lastClockUpdate = now;
    }

    delay(50);
}
