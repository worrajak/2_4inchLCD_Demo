#pragma once
#include <TFT_eSPI.h>
#include "prayer_fetcher.h"

extern TFT_eSPI tft;

#define CLR_PRAYER_BG     TFT_BLACK
#define CLR_PRAYER_LABEL  0xC618   // light grey
#define CLR_PRAYER_TIME   TFT_WHITE
#define CLR_PRAYER_CITY   TFT_CYAN
#define CLR_PRAYER_HIJRI  0xFE60   // gold
#define CLR_PRAYER_SUN    0xFDA0   // orange
#define CLR_PRAYER_NEXT   TFT_GREEN

struct PrayerUI {
    // Compute "next prayer" index based on current local time.
    // Returns 0..4 for Fajr/Dhuhr/Asr/Maghrib/Isha, or -1 if unknown.
    int nextPrayerIdx(const PrayerData& d) {
        struct tm t;
        if (!getLocalTime(&t)) return -1;
        int now_min = t.tm_hour * 60 + t.tm_min;
        const String* times[5] = {&d.fajr, &d.dhuhr, &d.asr, &d.maghrib, &d.isha};
        for (int i = 0; i < 5; i++) {
            if (times[i]->length() < 5) continue;
            int hh = times[i]->substring(0, 2).toInt();
            int mm = times[i]->substring(3, 5).toInt();
            if (hh * 60 + mm > now_min) return i;
        }
        return 0;  // all passed -> next is tomorrow's Fajr
    }

    void drawLoading() {
        tft.fillRect(0, 26, 320, 214, CLR_PRAYER_BG);
        tft.setTextColor(TFT_CYAN, CLR_PRAYER_BG);
        tft.setTextFont(2);
        tft.setCursor(90, 110);
        tft.print("Loading prayer times...");
    }

    void drawError(const String& msg) {
        tft.fillRect(0, 26, 320, 214, CLR_PRAYER_BG);
        tft.setTextColor(TFT_RED, CLR_PRAYER_BG);
        tft.setTextFont(2);
        tft.setCursor(20, 110);
        tft.print("Prayer error: ");
        tft.print(msg);
    }

    // Draws one prayer row: label left, time right
    void drawRow(int y, const char* label, const String& time, bool highlight) {
        uint16_t bg = highlight ? 0x0320 : CLR_PRAYER_BG;  // dark green tint
        tft.fillRect(4, y, 312, 26, bg);
        if (highlight) tft.drawRect(4, y, 312, 26, CLR_PRAYER_NEXT);

        tft.setTextColor(highlight ? CLR_PRAYER_NEXT : CLR_PRAYER_LABEL, bg);
        tft.setTextFont(4);
        tft.setCursor(14, y + 2);
        tft.print(label);

        tft.setTextColor(CLR_PRAYER_TIME, bg);
        tft.setTextFont(4);
        // Right-align time
        int tw = tft.textWidth(time);
        tft.setCursor(305 - tw, y + 2);
        tft.print(time);
    }

    void drawPrayerPage(const PrayerData& d) {
        tft.fillRect(0, 26, 320, 214, CLR_PRAYER_BG);

        // Top: City name + Hijri date
        tft.setTextFont(2);
        tft.setTextColor(CLR_PRAYER_CITY, CLR_PRAYER_BG);
        tft.setCursor(8, 28);
        tft.print(PRAYER_CITIES[d.city_idx].name);
        tft.print(", TH");

        // Hijri date right-aligned
        if (d.hijri_year > 0) {
            char hb[40];
            snprintf(hb, sizeof(hb), "%d %s %d AH",
                     d.hijri_day, d.hijri_month.c_str(), d.hijri_year);
            tft.setTextColor(CLR_PRAYER_HIJRI, CLR_PRAYER_BG);
            int hw = tft.textWidth(hb);
            tft.setCursor(312 - hw, 28);
            tft.print(hb);
        }

        // Divider
        tft.drawFastHLine(0, 46, 320, 0x4208);

        int next = nextPrayerIdx(d);

        // 5 prayer rows (26px each, starts y=50)
        const char* labels[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
        const String* times[5] = {&d.fajr, &d.dhuhr, &d.asr, &d.maghrib, &d.isha};
        for (int i = 0; i < 5; i++) {
            drawRow(50 + i * 28, labels[i], *times[i], i == next);
        }

        // Bottom: Sunrise / Sunset row
        tft.drawFastHLine(0, 192, 320, 0x4208);
        tft.setTextFont(2);
        tft.setTextColor(CLR_PRAYER_SUN, CLR_PRAYER_BG);
        tft.setCursor(8, 198);
        tft.print("Sunrise ");
        tft.setTextColor(TFT_WHITE, CLR_PRAYER_BG);
        tft.print(d.sunrise);

        tft.setTextColor(CLR_PRAYER_SUN, CLR_PRAYER_BG);
        tft.setCursor(170, 198);
        tft.print("Sunset ");
        tft.setTextColor(TFT_WHITE, CLR_PRAYER_BG);
        tft.print(d.sunset);

        // Footer hint
        tft.setTextFont(1);
        tft.setTextColor(0x6B4D, CLR_PRAYER_BG);
        tft.setCursor(8, 222);
        tft.print("Tap header right to change city");
    }
};
