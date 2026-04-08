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
#define CLR_TRX      0xF800   // Tron red
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

    void drawHeader(const char* title, bool wifi_ok, int page = -1, int totalPages = 0) {
        tft.fillRect(0, 0, 320, 24, CLR_HEADER);
        tft.setTextColor(TFT_WHITE, CLR_HEADER);
        tft.setTextFont(2);
        tft.setCursor(10, 4);
        tft.print(title);

        // Page indicator dots
        if (totalPages > 1) {
            int dotX = 138;
            for (int i = 0; i < totalPages; i++) {
                if (i == page) {
                    tft.fillCircle(dotX + i * 14, 12, 4, TFT_WHITE);
                } else {
                    tft.drawCircle(dotX + i * 14, 12, 3, 0x6B4D);
                }
            }
        }

        // Date+time
        struct tm t;
        if (getLocalTime(&t)) {
            char buf[20];
            snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d",
                     t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
            tft.setTextColor(TFT_YELLOW, CLR_HEADER);
            tft.setTextFont(2);
            tft.setCursor(195, 4);
            tft.print(buf);
        }

        // WiFi indicator
        uint16_t wcolor = wifi_ok ? TFT_GREEN : TFT_RED;
        tft.fillCircle(306, 12, 5, wcolor);
    }

    void drawPriceDashboard(PriceData &d, int page = 0, int totalPages = 2) {
        tft.fillScreen(CLR_BG);
        drawHeader("Prices", WiFi.status() == WL_CONNECTED, page, totalPages);

        int y = 26;
        int row_h = 37;

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

        // === CRYPTO: BTC + SOL on one line ===
        drawDivider(y);
        y += 2;
        drawCryptoRow(y, d);
        y += row_h;

        // === FX: THB  JPY  CNY per USD ===
        drawDivider(y);
        y += 2;
        drawFxRow(y, d);

        // Breaking news (2 headlines)
        drawNews(d.news1, d.news2);
    }

    // Draw news as static wrapped text (2 headlines)
    void drawNews(const String &h1, const String &h2) {
        tft.fillRect(0, NEWS_Y, 320, 240 - NEWS_Y, CLR_NEWS_BG);
        tft.drawFastHLine(0, NEWS_Y, 320, TFT_RED);

        if (h1.length() == 0) return;

        tft.setTextColor(CLR_NEWS_TXT, CLR_NEWS_BG);
        tft.setTextFont(1);  // 6x8 font, ~52 chars per line

        int maxChars = 52;
        int lineY = NEWS_Y + 4;
        int maxLines = (240 - NEWS_Y - 4) / 11;

        auto printWrapped = [&](const String &text) {
            String remain = text;
            while (remain.length() > 0 && maxLines > 0) {
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
                maxLines--;
            }
        };

        printWrapped(h1);

        if (maxLines > 0 && h2.length() > 0) {
            lineY += 2;
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
        tft.setCursor(8, y + 8);
        tft.print("FUEL");

        // 3 compact columns
        int col_x[] = {55, 145, 235};
        const char* labels[] = {"DSL", "95", "91"};  // diesel_b7 = ดีเซลธรรมดาที่ขายทุกปั๊ม
        float prices[] = {d.diesel_b7, d.gasohol_95, d.gasohol_91};

        for (int i = 0; i < 3; i++) {
            // Label (Font2)
            tft.setTextColor(CLR_LABEL, CLR_BG);
            tft.setTextFont(2);
            tft.setCursor(col_x[i], y + 1);
            tft.print(labels[i]);

            // Price (Font4, larger)
            tft.setTextColor(CLR_VALUE, CLR_BG);
            tft.setTextFont(4);
            char buf[8];
            if (prices[i] > 0) {
                snprintf(buf, sizeof(buf), "%.2f", prices[i]);
            } else {
                snprintf(buf, sizeof(buf), "---");
            }
            tft.setCursor(col_x[i], y + 14);
            tft.print(buf);
        }
    }

    // Draw crypto row: BTC + SOL + TRX in 3 compact columns
    void drawCryptoRow(int y, PriceData &d) {
        const int col_x[3]   = {8, 112, 216};
        const char* names[3] = {"BTC", "SOL", "TRX"};
        const uint16_t cols[3] = {CLR_BTC, CLR_SOL, CLR_TRX};
        const float prices[3]  = {d.btc_usd, d.sol_usd, d.trx_usd};
        const float chgs[3]    = {d.btc_change_24h, d.sol_change_24h, d.trx_change_24h};
        const int decs[3]      = {0, 2, 4};

        for (int i = 0; i < 3; i++) {
            // Symbol label
            tft.setTextColor(cols[i], CLR_BG);
            tft.setTextFont(2);
            tft.setCursor(col_x[i], y + 1);
            tft.print(names[i]);

            // 24h change %
            if (chgs[i] != 0) {
                tft.setTextColor(chgs[i] >= 0 ? CLR_UP : CLR_DOWN, CLR_BG);
                tft.setTextFont(2);
                tft.setCursor(col_x[i] + 38, y + 1);
                tft.print(formatChange(chgs[i]));
            }

            // Price (Font4)
            tft.setTextColor(CLR_VALUE, CLR_BG);
            tft.setTextFont(4);
            tft.setCursor(col_x[i], y + 14);
            tft.print(formatPrice(prices[i], "$", decs[i]));
        }
    }

    // Draw FX row: THB/USD  CNY/THB  JPY/THB (Font4 values)
    void drawFxRow(int y, PriceData &d) {
        tft.setTextColor(CLR_FX, CLR_BG);
        tft.setTextFont(2);
        tft.setCursor(8, y + 8);
        tft.print("FX");

        // Calculate cross rates
        float cny_thb = (d.thb_per_usd > 0 && d.cny_per_usd > 0)
                         ? d.thb_per_usd / d.cny_per_usd : 0;
        float jpy_thb = (d.thb_per_usd > 0 && d.jpy_per_usd > 0)
                         ? d.thb_per_usd / d.jpy_per_usd : 0;

        // 3 columns: THB/USD, CNY/THB, JPY/THB
        int col_x[] = {55, 145, 235};
        const char* labels[] = {"THB/$", "CNY/B", "JPY/B"};
        float rates[] = {d.thb_per_usd, cny_thb, jpy_thb};
        int decimals[] = {2, 2, 3};

        for (int i = 0; i < 3; i++) {
            // Label (Font2)
            tft.setTextColor(CLR_LABEL, CLR_BG);
            tft.setTextFont(2);
            tft.setCursor(col_x[i], y + 1);
            tft.print(labels[i]);

            // Rate value (Font4)
            tft.setTextColor(CLR_VALUE, CLR_BG);
            tft.setTextFont(4);
            char buf[10];
            if (rates[i] > 0) {
                snprintf(buf, sizeof(buf), "%.*f", decimals[i], rates[i]);
            } else {
                snprintf(buf, sizeof(buf), "---");
            }
            tft.setCursor(col_x[i], y + 14);
            tft.print(buf);
        }
    }

    void drawDivider(int y) {
        tft.drawFastHLine(5, y, 310, CLR_DIVIDER);
    }

    // Loading/refresh animation
    void drawLoading(int page = 0, int totalPages = 2) {
        tft.fillScreen(CLR_BG);
        const char* titles[] = {"Prices", "Weather"};
        drawHeader(page < 2 ? titles[page] : "Loading",
                   WiFi.status() == WL_CONNECTED, page, totalPages);
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
