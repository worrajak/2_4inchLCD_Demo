#pragma once
#include <TFT_eSPI.h>
#include <time.h>
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

#define CLR_NEWS_BG  0x8000   // Dark red
#define CLR_NEWS_TXT TFT_YELLOW
#define NEWS_Y       180       // News area start

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

        // Thai date+time after title
        struct tm t;
        if (getLocalTime(&t)) {
            char buf[20];
            snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d",
                     t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
            tft.setTextColor(TFT_YELLOW, CLR_HEADER);
            tft.setTextFont(2);
            tft.setCursor(170, 4);
            tft.print(buf);
        }

        // WiFi indicator
        uint16_t wcolor = wifi_ok ? TFT_GREEN : TFT_RED;
        tft.fillCircle(300, 12, 5, wcolor);
    }

    void drawPriceDashboard(PriceData &d) {
        tft.fillScreen(CLR_BG);
        drawHeader("Price Tracker", WiFi.status() == WL_CONNECTED);

        int y = 28;
        int row_h = 30;

        // === GOLD ===  $3,045.20  42,350 THB/bt
        drawDivider(y);
        y += 2;
        drawCompactRow(y, "GOLD", CLR_GOLD,
                       formatPrice(d.gold_usd, "$", 2),
                       d.gold_thb > 0 ? formatPriceTHB(d.gold_thb) : "",
                       CLR_LABEL);
        y += row_h;

        // === FUEL (Diesel, G95, G91) ===
        drawDivider(y);
        y += 2;
        drawFuelRow(y, d);
        y += row_h;

        // === BTC ===  $87,432  +2.3%
        drawDivider(y);
        y += 2;
        drawCompactRow(y, "BTC", CLR_BTC,
                       formatPrice(d.btc_usd, "$", 0),
                       formatChange(d.btc_change_24h),
                       d.btc_change_24h >= 0 ? CLR_UP : CLR_DOWN);
        y += row_h;

        // === SOL ===  $142.50  -1.2%
        drawDivider(y);
        y += 2;
        drawCompactRow(y, "SOL", CLR_SOL,
                       formatPrice(d.sol_usd, "$", 2),
                       formatChange(d.sol_change_24h),
                       d.sol_change_24h >= 0 ? CLR_UP : CLR_DOWN);
        y += row_h;

        // === THB/USD ===
        drawDivider(y);
        y += 2;
        drawCompactRow(y, "THB", CLR_FX,
                       d.thb_per_usd > 0 ? String(d.thb_per_usd, 2) : "---",
                       "per USD", CLR_LABEL);

        // Breaking news (static, word-wrapped)
        drawNews(d.news1, d.news2);
    }

    // Draw news as static wrapped text
    void drawNews(const String &h1, const String &h2) {
        tft.fillRect(0, NEWS_Y, 320, 240 - NEWS_Y, CLR_NEWS_BG);
        tft.drawFastHLine(0, NEWS_Y, 320, TFT_RED);

        if (h1.length() == 0) return;

        tft.setTextColor(CLR_NEWS_TXT, CLR_NEWS_BG);
        tft.setTextFont(1);  // 6x8 font, ~52 chars per line

        int maxChars = 52;
        int lineY = NEWS_Y + 4;
        int maxLines = (240 - NEWS_Y - 4) / 11;  // lines that fit

        // Word-wrap helper: prints one String, returns lines used
        auto printWrapped = [&](const String &text) -> int {
            int used = 0;
            String remain = text;
            while (remain.length() > 0 && used < maxLines) {
                String line;
                if ((int)remain.length() <= maxChars) {
                    line = remain;
                    remain = "";
                } else {
                    int cut = remain.lastIndexOf(' ', maxChars);
                    if (cut <= 0) cut = maxChars;
                    line = remain.substring(0, cut);
                    remain = remain.substring(cut + 1);
                }
                tft.setCursor(4, lineY);
                tft.print(line);
                lineY += 11;
                used++;
                maxLines--;
            }
            return used;
        };

        printWrapped(h1);

        // Print headline 2 if space remains
        if (maxLines > 0 && h2.length() > 0) {
            lineY += 2;  // small gap between headlines
            printWrapped(h2);
        }
    }

    // Single-line compact row: LABEL  PRICE  sub_text
    void drawCompactRow(int y, const char* label, uint16_t label_color,
                        String price, String sub, uint16_t sub_color) {
        // Label (Font2, small)
        tft.setTextColor(label_color, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(8, y + 6);
        tft.print(label);

        // Price (Font4, large)
        tft.setTextColor(CLR_VALUE, CLR_BG);
        tft.setTextFont(4);
        tft.setCursor(55, y + 1);
        tft.print(price);

        // Sub text (Font2, right side)
        if (sub.length() > 0) {
            tft.setTextColor(sub_color, CLR_BG);
            tft.setTextFont(2);
            tft.setCursor(215, y + 6);
            tft.print(sub);
        }
    }

    // Draw Thai fuel prices: FUEL  B7 29.94  95 35.05  91 32.68
    void drawFuelRow(int y, PriceData &d) {
        tft.setTextColor(CLR_FUEL, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(8, y + 6);
        tft.print("FUEL");

        // 3 compact columns
        int col_x[] = {55, 145, 235};
        const char* labels[] = {"B7", "95", "91"};
        float prices[] = {d.diesel_b7, d.gasohol_95, d.gasohol_91};

        for (int i = 0; i < 3; i++) {
            // Label
            tft.setTextColor(CLR_LABEL, CLR_BG);
            tft.setTextFont(1);
            tft.setCursor(col_x[i], y + 1);
            tft.print(labels[i]);

            // Price (Font2, drawn twice offset 1px = faux bold)
            tft.setTextColor(CLR_VALUE, CLR_BG);
            tft.setTextFont(2);
            char buf[8];
            if (prices[i] > 0) {
                snprintf(buf, sizeof(buf), "%.2f", prices[i]);
            } else {
                snprintf(buf, sizeof(buf), "---");
            }
            tft.setCursor(col_x[i], y + 12);
            tft.print(buf);
            tft.setCursor(col_x[i] + 1, y + 12);
            tft.print(buf);
        }
    }

    void drawDivider(int y) {
        tft.drawFastHLine(5, y, 310, CLR_DIVIDER);
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

    String formatChange(float pct) {
        if (pct == 0) return "";
        char buf[12];
        snprintf(buf, sizeof(buf), "%s%.1f%%", pct >= 0 ? "+" : "", pct);
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
