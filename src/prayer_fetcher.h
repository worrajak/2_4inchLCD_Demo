#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

// Cities for prayer times (4 Thai provinces)
struct PrayerCity {
    const char* name;
    float lat;
    float lon;
};

static const PrayerCity PRAYER_CITIES[] = {
    {"Chiang Mai", 18.7883f,  98.9853f},
    {"Chiang Rai", 19.9105f,  99.8406f},
    {"Bangkok",    13.7563f, 100.5018f},
    {"Songkhla",    7.1896f, 100.5945f},
};
static const int PRAYER_CITY_COUNT = sizeof(PRAYER_CITIES) / sizeof(PRAYER_CITIES[0]);

struct PrayerData {
    String fajr;
    String sunrise;
    String dhuhr;
    String asr;
    String maghrib;
    String isha;
    String sunset;
    int    hijri_day;
    String hijri_month;   // English name e.g. "Ramadan"
    int    hijri_year;
    int    fetched_yday;  // tm_yday on which data was fetched (-1 = none)
    int    city_idx;
    bool   valid;
    String error_msg;
};

struct PrayerFetcher {
    PrayerData data;
    Preferences prefs;

    void init() {
        data.fetched_yday = -1;
        data.valid = false;
        // Load saved city preference
        prefs.begin("prayer", true);
        data.city_idx = prefs.getInt("city", 0);
        prefs.end();
        if (data.city_idx < 0 || data.city_idx >= PRAYER_CITY_COUNT) data.city_idx = 0;
    }

    void setCity(int idx) {
        if (idx < 0 || idx >= PRAYER_CITY_COUNT) return;
        data.city_idx = idx;
        data.valid = false;
        data.fetched_yday = -1;  // force refetch
        prefs.begin("prayer", false);
        prefs.putInt("city", idx);
        prefs.end();
    }

    void cycleCity() {
        setCity((data.city_idx + 1) % PRAYER_CITY_COUNT);
    }

    const PrayerCity& currentCity() const {
        return PRAYER_CITIES[data.city_idx];
    }

    // Strip " (+07)" timezone suffix Aladhan adds, keep only "HH:MM"
    static String cleanTime(const char* s) {
        if (!s) return String("--:--");
        String r(s);
        int sp = r.indexOf(' ');
        if (sp > 0) r = r.substring(0, sp);
        return r;
    }

    // Returns true if data is fresh enough (same yday) for current city
    bool needsFetch() {
        if (!data.valid) return true;
        struct tm t;
        if (!getLocalTime(&t)) return false;  // no NTP yet, don't spam
        return t.tm_yday != data.fetched_yday;
    }

    bool fetch() {
        struct tm t;
        if (!getLocalTime(&t)) {
            data.error_msg = "No NTP time";
            return false;
        }

        const PrayerCity& c = currentCity();
        char url[200];
        snprintf(url, sizeof(url),
            "https://api.aladhan.com/v1/timings/%02d-%02d-%04d?latitude=%.4f&longitude=%.4f&method=2",
            t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, c.lat, c.lon);

        HTTPClient http;
        http.begin(url);
        http.setTimeout(10000);
        int code = http.GET();
        if (code != 200) {
            data.error_msg = "Prayer API err:" + String(code);
            http.end();
            return false;
        }

        String payload = http.getString();
        JsonDocument filter;
        JsonObject ft = filter["data"]["timings"].to<JsonObject>();
        ft["Fajr"] = true;
        ft["Sunrise"] = true;
        ft["Dhuhr"] = true;
        ft["Asr"] = true;
        ft["Maghrib"] = true;
        ft["Isha"] = true;
        ft["Sunset"] = true;
        filter["data"]["date"]["hijri"]["day"] = true;
        filter["data"]["date"]["hijri"]["year"] = true;
        filter["data"]["date"]["hijri"]["month"]["en"] = true;

        JsonDocument doc;
        DeserializationError e = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        http.end();
        if (e) {
            data.error_msg = String("JSON err:") + e.c_str();
            return false;
        }

        JsonObject ti = doc["data"]["timings"];
        data.fajr    = cleanTime(ti["Fajr"]    | (const char*)nullptr);
        data.sunrise = cleanTime(ti["Sunrise"] | (const char*)nullptr);
        data.dhuhr   = cleanTime(ti["Dhuhr"]   | (const char*)nullptr);
        data.asr     = cleanTime(ti["Asr"]     | (const char*)nullptr);
        data.maghrib = cleanTime(ti["Maghrib"] | (const char*)nullptr);
        data.isha    = cleanTime(ti["Isha"]    | (const char*)nullptr);
        data.sunset  = cleanTime(ti["Sunset"]  | (const char*)nullptr);

        JsonObject hj = doc["data"]["date"]["hijri"];
        const char* hday = hj["day"]   | "";
        const char* hyr  = hj["year"]  | "";
        const char* hmon = hj["month"]["en"] | "";
        data.hijri_day   = atoi(hday);
        data.hijri_year  = atoi(hyr);
        data.hijri_month = String(hmon);

        data.fetched_yday = t.tm_yday;
        data.valid = true;
        data.error_msg = "";
        return true;
    }
};
