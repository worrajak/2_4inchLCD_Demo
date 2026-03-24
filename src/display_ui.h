#pragma once
#include <TFT_eSPI.h>
#include "price_fetcher.h"

extern TFT_eSPI tft;

// Colors
#define CLR_BG       TFT_BLACK
#define CLR_HEADER   0x1082   // Dark blue
#define CLR_GOLD     0xFE60   // Gold color
#define CLR_FUEL     0x2D8F   // Teal/petrol
#define CLR_BTC      0xFDA0   // Orange
#define CLR_SOL      0x9F1F   // Purple/violet
#define CLR_FX       0x07FF   // Cyan
#define CLR_UP       TFT_GREEN
#define CLR_DOWN     TFT_RED
#define CLR_LABEL    0xC618   // Light grey
#define CLR_VALUE    TFT_WHITE
#define CLR_DIVIDER  0x4208   // Dark grey

struct DisplayUI {
    unsigned long last_draw;
    int anim_frame;

    void init() {
        last_draw = 0;
        anim_frame = 0;
    }

    void drawHeader(const char* title, bool wifi_ok) {
        tft.fillRect(0, 0, 320, 24, CLR_HEADER);
        tft.setTextColor(TFT_WHITE, CLR_HEADER);
        tft.setTextFont(2);
        tft.setCursor(10, 4);
        tft.print(title);

        // WiFi indicator
        uint16_t wcolor = wifi_ok ? TFT_GREEN : TFT_RED;
        tft.fillCircle(300, 12, 5, wcolor);

        // Time since update
        tft.setTextFont(1);
        tft.setTextColor(0xBDF7, CLR_HEADER);
        tft.setCursor(250, 8);
    }

    void drawPriceDashboard(PriceData &d) {
        tft.fillScreen(CLR_BG);
        drawHeader("Price Tracker", WiFi.status() == WL_CONNECTED);

        int y = 28;
        int row_h = 36;

        // === GOLD ===
        drawDivider(y);
        y += 2;
        drawPriceRow(y, "GOLD", CLR_GOLD,
                     formatPrice(d.gold_usd, "$", 2),
                     d.gold_thb > 0 ? formatPriceTHB(d.gold_thb) : "---",
                     0, false);
        y += row_h;

        // === FUEL (Diesel, G95, G91) ===
        drawDivider(y);
        y += 2;
        drawFuelRow(y, d);
        y += row_h;

        // === BTC ===
        drawDivider(y);
        y += 2;
        drawPriceRow(y, "BTC", CLR_BTC,
                     formatPrice(d.btc_usd, "$", 0),
                     "",
                     d.btc_change_24h, true);
        y += row_h;

        // === SOL ===
        drawDivider(y);
        y += 2;
        drawPriceRow(y, "SOL", CLR_SOL,
                     formatPrice(d.sol_usd, "$", 2),
                     "",
                     d.sol_change_24h, true);
        y += row_h;

        // === THB/USD ===
        drawDivider(y);
        y += 2;
        drawFXRow(y, d.thb_per_usd);

        // Update timestamp
        drawUpdateTime(d.last_update);
    }

    void drawPriceRow(int y, const char* label, uint16_t label_color,
                      String price, String sub, float change_pct, bool show_change) {
        // Label
        tft.setTextColor(label_color, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(8, y + 2);
        tft.print(label);

        // Price (large)
        tft.setTextColor(CLR_VALUE, CLR_BG);
        tft.setTextFont(4);
        tft.setCursor(70, y);
        tft.print(price);

        // Sub text or change percentage
        if (show_change && change_pct != 0) {
            uint16_t chg_color = change_pct >= 0 ? CLR_UP : CLR_DOWN;
            tft.setTextColor(chg_color, CLR_BG);
            tft.setTextFont(2);
            int tx = 70;
            tft.setCursor(tx, y + 24);
            tft.printf("%s%.1f%%", change_pct >= 0 ? "+" : "", change_pct);
        } else if (sub.length() > 0) {
            tft.setTextColor(CLR_LABEL, CLR_BG);
            tft.setTextFont(2);
            tft.setCursor(70, y + 24);
            tft.print(sub);
        }
    }

    // Draw Thai fuel prices: Diesel B7 | Gasohol 95 | Gasohol 91
    void drawFuelRow(int y, PriceData &d) {
        tft.setTextColor(CLR_FUEL, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(8, y + 2);
        tft.print("FUEL");

        tft.setTextFont(1);
        tft.setCursor(8, y + 20);
        tft.setTextColor(CLR_LABEL, CLR_BG);
        tft.print("PTT B/L");

        // 3 columns: Diesel | G95 | G91
        int col_x[] = {70, 160, 250};
        const char* labels[] = {"Diesel", "G-95", "G-91"};
        float prices[] = {d.diesel_b7, d.gasohol_95, d.gasohol_91};

        for (int i = 0; i < 3; i++) {
            // Label
            tft.setTextColor(CLR_LABEL, CLR_BG);
            tft.setTextFont(1);
            tft.setCursor(col_x[i], y + 2);
            tft.print(labels[i]);
            // Price
            tft.setTextColor(CLR_VALUE, CLR_BG);
            tft.setTextFont(2);
            tft.setCursor(col_x[i], y + 14);
            if (prices[i] > 0) {
                tft.printf("%.2f", prices[i]);
            } else {
                tft.print("---");
            }
        }
    }

    void drawFXRow(int y, float rate) {
        tft.setTextColor(CLR_FX, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(8, y + 2);
        tft.print("THB");

        tft.setTextColor(CLR_VALUE, CLR_BG);
        tft.setTextFont(4);
        tft.setCursor(70, y);
        if (rate > 0) {
            tft.printf("%.2f", rate);
        } else {
            tft.print("---");
        }

        tft.setTextColor(CLR_LABEL, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(200, y + 8);
        tft.print("per USD");
    }

    void drawDivider(int y) {
        tft.drawFastHLine(5, y, 310, CLR_DIVIDER);
    }

    void drawUpdateTime(unsigned long last_ms) {
        tft.setTextColor(0x7BEF, CLR_BG);
        tft.setTextFont(1);
        tft.setCursor(220, 232);
        if (last_ms > 0) {
            int sec = (millis() - last_ms) / 1000;
            if (sec < 60) {
                tft.printf("Updated %ds ago", sec);
            } else {
                tft.printf("Updated %dm ago", sec / 60);
            }
        } else {
            tft.print("No data yet");
        }
    }

    // Loading/refresh animation
    void drawLoading() {
        tft.fillScreen(CLR_BG);
        drawHeader("Price Tracker", WiFi.status() == WL_CONNECTED);
        tft.setTextColor(TFT_CYAN, CLR_BG);
        tft.setTextFont(4);
        tft.setCursor(60, 100);
        tft.print("Loading...");
    }

    // Error display
    void drawError(const char* msg) {
        tft.fillRect(0, 220, 320, 20, CLR_BG);
        tft.setTextColor(TFT_RED, CLR_BG);
        tft.setTextFont(1);
        tft.setCursor(10, 225);
        tft.print(msg);
    }

    // Format helpers
    String formatPrice(float val, const char* prefix, int decimals) {
        if (val <= 0) return "---";
        char buf[20];
        if (val >= 10000) {
            // Format with comma: e.g., $67,432
            int whole = (int)val;
            if (decimals == 0) {
                snprintf(buf, sizeof(buf), "%s%d,%03d",
                         prefix, whole / 1000, whole % 1000);
            } else {
                snprintf(buf, sizeof(buf), "%s%d,%03d",
                         prefix, whole / 1000, whole % 1000);
            }
        } else {
            snprintf(buf, sizeof(buf), "%s%.*f", prefix, decimals, val);
        }
        return String(buf);
    }

    String formatPriceTHB(float val) {
        char buf[24];
        if (val >= 10000) {
            int whole = (int)val;
            snprintf(buf, sizeof(buf), "%d,%03d THB/bt", whole / 1000, whole % 1000);
        } else {
            snprintf(buf, sizeof(buf), "%.0f THB/bt", val);
        }
        return String(buf);
    }
};
