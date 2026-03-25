#pragma once
#include <TFT_eSPI.h>
#include <time.h>
#include "weather_fetcher.h"

extern TFT_eSPI tft;

// City colors
#define CLR_WX_CM    0xFE60   // Gold (Chiang Mai)
#define CLR_WX_CR    0x07E0   // Green (Chiang Rai)
#define CLR_WX_BKK   0x07FF   // Cyan (Bangkok)
#define CLR_WX_OSK   0xF81F   // Magenta (Osaka)

// AQI colors
#define CLR_AQI_GOOD    0x07E0   // Green  0-50
#define CLR_AQI_MOD     0xFFE0   // Yellow 51-100
#define CLR_AQI_SENS    0xFDA0   // Orange 101-150
#define CLR_AQI_BAD     0xF800   // Red    151-200
#define CLR_AQI_VBAD    0x780F   // Purple 201-300
#define CLR_AQI_HAZ     0x8000   // Maroon 301+

#define CLR_QUAKE_BG  0x8000   // Dark red
#define CLR_QUAKE_TXT 0xFFE0   // Yellow

struct WeatherUI {

    uint16_t aqiColor(int aqi) {
        if (aqi <= 50)  return CLR_AQI_GOOD;
        if (aqi <= 100) return CLR_AQI_MOD;
        if (aqi <= 150) return CLR_AQI_SENS;
        if (aqi <= 200) return CLR_AQI_BAD;
        if (aqi <= 300) return CLR_AQI_VBAD;
        return CLR_AQI_HAZ;
    }

    uint16_t cityColor(int idx) {
        const uint16_t colors[] = {CLR_WX_CM, CLR_WX_CR, CLR_WX_BKK, CLR_WX_OSK};
        return (idx < 4) ? colors[idx] : TFT_WHITE;
    }

    // Draw full weather page (content below header, header drawn by main)
    void drawWeatherPage(WeatherData &d) {
        // Clear content area below header
        tft.fillRect(0, 25, 320, 215, TFT_BLACK);

        // 2x2 city grid
        // Top row: y=26-98, Bottom row: y=100-172
        // Left col: x=0-158, Right col: x=161-319
        drawCityCell(d.cities[0], 0,   0,  26);   // CM  top-left
        drawCityCell(d.cities[1], 1, 161,  26);   // CR  top-right
        drawCityCell(d.cities[2], 2,   0, 100);   // BKK bottom-left
        drawCityCell(d.cities[3], 3, 161, 100);   // OSK bottom-right

        // Grid lines
        tft.drawFastVLine(159, 26, 148, 0x4208);
        tft.drawFastVLine(160, 26, 148, 0x4208);
        tft.drawFastHLine(0, 99, 320, 0x4208);

        // Earthquake section at bottom
        drawEarthquake(d.quake);
    }

    // Draw one city cell (158x73 pixels)
    void drawCityCell(CityWeather &c, int idx, int x, int y) {
        uint16_t clr = cityColor(idx);
        char buf[16];

        // --- Line 1: City name (left) + Humidity (right) ---
        tft.setTextColor(clr, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(x + 4, y + 2);
        tft.print(c.name);

        // Humidity right-aligned in cell
        tft.setTextColor(0x5DDF, TFT_BLACK);  // Light blue
        tft.setTextFont(2);
        snprintf(buf, sizeof(buf), "%d%%", c.humidity);
        int hw = strlen(buf) * 12;
        tft.setCursor(x + 155 - hw, y + 2);
        tft.print(buf);

        // --- Line 2: Temperature (Font 4, hero number) ---
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextFont(4);
        tft.setCursor(x + 4, y + 22);
        if (c.temp >= 0 && c.temp < 100) {
            snprintf(buf, sizeof(buf), "%.1f", c.temp);
        } else {
            snprintf(buf, sizeof(buf), "%.0f", c.temp);
        }
        tft.print(buf);

        // Degree + C after temp in smaller font
        int cx_after = tft.getCursorX();
        tft.setTextFont(2);
        tft.setCursor(cx_after, y + 22);
        tft.print("o");  // degree approximation
        tft.setCursor(cx_after + 8, y + 28);
        tft.print("C");

        // --- Line 3: AQI badge + Rain probability ---
        // AQI colored badge
        uint16_t ac = aqiColor(c.aqi);
        tft.fillRoundRect(x + 4, y + 52, 56, 18, 3, ac);
        tft.setTextColor(c.aqi <= 100 ? TFT_BLACK : TFT_WHITE, ac);
        tft.setTextFont(2);
        tft.setCursor(x + 7, y + 53);
        snprintf(buf, sizeof(buf), "AQ:%d", c.aqi);
        tft.print(buf);

        // Rain probability
        uint16_t rainClr;
        if (c.rain_prob > 60)      rainClr = 0x001F;  // Blue
        else if (c.rain_prob > 30) rainClr = 0x5DDF;  // Light blue
        else                       rainClr = 0x4208;  // Grey

        tft.setTextColor(rainClr, TFT_BLACK);
        tft.setTextFont(2);
        tft.setCursor(x + 68, y + 53);
        snprintf(buf, sizeof(buf), "R:%d%%", c.rain_prob);
        tft.print(buf);

        // Rain forecast word
        tft.setTextFont(1);
        tft.setCursor(x + 120, y + 57);
        if (c.rain_prob > 70) {
            tft.setTextColor(0x001F, TFT_BLACK);
            tft.print("Rain!");
        } else if (c.rain_prob > 40) {
            tft.setTextColor(0x5DDF, TFT_BLACK);
            tft.print("Maybe");
        } else {
            tft.setTextColor(0x4208, TFT_BLACK);
            tft.print("Clear");
        }
    }

    // Earthquake section: y=174-239 (66px)
    void drawEarthquake(QuakeInfo &q) {
        int y = 174;
        tft.fillRect(0, y, 320, 66, CLR_QUAKE_BG);
        tft.drawFastHLine(0, y, 320, TFT_RED);

        // Title
        tft.setTextColor(CLR_QUAKE_TXT, CLR_QUAKE_BG);
        tft.setTextFont(2);
        tft.setCursor(4, y + 2);
        tft.print("EARTHQUAKE");

        if (!q.found) {
            tft.setTextColor(TFT_GREEN, CLR_QUAKE_BG);
            tft.setTextFont(2);
            tft.setCursor(4, y + 24);
            tft.print("No recent quakes near cities");
            return;
        }

        char buf[16];

        // Magnitude badge
        uint16_t magClr = q.magnitude >= 6.0f ? TFT_RED :
                          q.magnitude >= 5.0f ? 0xFDA0 : TFT_YELLOW;
        tft.fillRoundRect(115, y + 1, 55, 18, 3, magClr);
        tft.setTextColor(TFT_BLACK, magClr);
        tft.setTextFont(2);
        tft.setCursor(119, y + 2);
        snprintf(buf, sizeof(buf), "M%.1f", q.magnitude);
        tft.print(buf);

        // Distance from nearest city
        tft.setTextColor(TFT_WHITE, CLR_QUAKE_BG);
        tft.setTextFont(2);
        tft.setCursor(178, y + 2);
        snprintf(buf, sizeof(buf), "%.0fkm", q.dist_km);
        tft.print(buf);
        tft.setTextColor(0xC618, CLR_QUAKE_BG);
        tft.print(">");
        tft.setTextColor(TFT_WHITE, CLR_QUAKE_BG);
        tft.print(q.nearest_city);

        // Place name (Font 1, may be long)
        tft.setTextColor(TFT_WHITE, CLR_QUAKE_BG);
        tft.setTextFont(1);
        tft.setCursor(4, y + 24);
        String place = q.place;
        if ((int)place.length() > 52) place = place.substring(0, 52);
        tft.print(place);

        // Time ago + Depth
        tft.setCursor(4, y + 38);
        tft.setTextColor(0xC618, CLR_QUAKE_BG);

        struct tm t;
        if (getLocalTime(&t)) {
            time_t now_epoch = mktime(&t);
            long diff = now_epoch - (long)q.time_epoch;
            if (diff > 0) {
                if (diff < 3600)
                    snprintf(buf, sizeof(buf), "%ldm ago", diff / 60);
                else if (diff < 86400)
                    snprintf(buf, sizeof(buf), "%ldh ago", diff / 3600);
                else
                    snprintf(buf, sizeof(buf), "%ldd ago", diff / 86400);
                tft.print(buf);
            }
        }

        snprintf(buf, sizeof(buf), " | Depth:%.0fkm", q.depth_km);
        tft.print(buf);
    }

    void drawLoading() {
        tft.fillRect(0, 25, 320, 215, TFT_BLACK);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextFont(4);
        tft.setCursor(30, 100);
        tft.print("Loading WX...");
    }
};
