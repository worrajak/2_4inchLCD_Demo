#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#include "wifi_setup.h"
#include "price_fetcher.h"
#include "display_ui.h"

// === Configuration ===
#define BACKLIGHT_PIN 27
#define UPDATE_INTERVAL_MS 60000  // Refresh prices every 60 seconds

// === Touch calibration (raw mapping, rotation 1) ===
// Calibrated from 2026-02-12_ESP32-Cheap-Yellow-Display-main
#define RAW_X_MIN 304
#define RAW_X_MAX 3928
#define RAW_Y_MIN 243
#define RAW_Y_MAX 3909

// === Global objects ===
WiFiSetup wifiSetup;
PriceFetcher fetcher;
DisplayUI ui;

unsigned long lastFetchTime = 0;
bool firstFetch = true;

// Touch helper using TFT_eSPI raw touch (matches reference project)
bool getTouch(uint16_t &sx, uint16_t &sy) {
    if (tft.getTouchRawZ() < 300) return false;  // Pressure threshold
    uint16_t rx, ry;
    tft.getTouchRaw(&rx, &ry);
    int x = map(rx, RAW_X_MIN, RAW_X_MAX, 0, 319);
    int y = map(ry, RAW_Y_MIN, RAW_Y_MAX, 0, 239);
    sx = constrain(x, 0, 319);
    sy = constrain(y, 0, 239);
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("CYD Price Tracker starting...");

    // Backlight ON
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, HIGH);

    // Init display
    tft.init();
    tft.setRotation(1);  // Landscape 320x240
    tft.fillScreen(TFT_BLACK);

    // Splash screen
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextFont(4);
    tft.setCursor(40, 60);
    tft.println("Price Tracker");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(50, 110);
    tft.println("Gold | Oil | Crypto | FX");
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextFont(1);
    tft.setCursor(100, 200);
    tft.println("ESP32 CYD 2.4\"");
    delay(2000);

    // WiFi setup
    wifiSetup.init();
    wifiSetup.runSetup();

    // Init UI
    ui.init();
    ui.drawLoading();
}

void loop() {
    unsigned long now = millis();

    // Fetch prices on interval
    if (firstFetch || (now - lastFetchTime >= UPDATE_INTERVAL_MS)) {
        Serial.println("Fetching prices...");

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected, reconnecting...");
            wifiSetup.connectWiFi(10);
        }

        if (WiFi.status() == WL_CONNECTED) {
            bool ok = fetcher.fetchAll();
            if (ok) {
                Serial.println("Prices updated successfully");
                Serial.printf("  BTC: $%.0f  SOL: $%.2f\n", fetcher.data.btc_usd, fetcher.data.sol_usd);
                Serial.printf("  Gold: $%.2f  THB/USD: %.2f\n", fetcher.data.gold_usd, fetcher.data.thb_per_usd);
                Serial.printf("  Diesel: %.2f  G95: %.2f  G91: %.2f\n", fetcher.data.diesel_b7, fetcher.data.gasohol_95, fetcher.data.gasohol_91);
                ui.drawPriceDashboard(fetcher.data);
            } else {
                Serial.println("Fetch failed: " + fetcher.data.error_msg);
                if (!firstFetch) {
                    ui.drawError(fetcher.data.error_msg.c_str());
                }
            }
        } else {
            ui.drawError("WiFi disconnected");
        }

        lastFetchTime = now;
        firstFetch = false;
    }

    // Touch: tap anywhere to force refresh
    uint16_t tx, ty;
    if (getTouch(tx, ty)) {
        // Top area: force refresh
        if (ty < 30) {
            ui.drawLoading();
            fetcher.fetchAll();
            ui.drawPriceDashboard(fetcher.data);
            lastFetchTime = millis();
        }
        delay(300);  // Debounce
    }

    // Update the "last updated" time periodically
    if (fetcher.data.valid && (now - ui.last_draw > 10000)) {
        ui.drawUpdateTime(fetcher.data.last_update);
        ui.last_draw = now;
    }

    delay(50);
}
