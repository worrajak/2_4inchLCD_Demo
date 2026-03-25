#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

#define WX_NUM_CITIES 4

struct CityWeather {
    const char* name;
    const char* short_name;
    float lat;
    float lon;
    float temp;
    int humidity;
    int aqi;
    int rain_prob;
};

struct QuakeInfo {
    bool found;
    float magnitude;
    float depth_km;
    String place;
    float dist_km;
    String nearest_city;
    unsigned long time_epoch;
};

struct WeatherData {
    CityWeather cities[WX_NUM_CITIES];
    QuakeInfo quake;
    bool valid;
    String error_msg;
    unsigned long last_update;
};

class WeatherFetcher {
public:
    WeatherData data;

    void init() {
        data.cities[0] = {"Chiang Mai", "CM",  18.79f, 98.98f,  0,0,0,0};
        data.cities[1] = {"Chiang Rai", "CR",  19.91f, 99.83f,  0,0,0,0};
        data.cities[2] = {"Bangkok",    "BKK", 13.76f, 100.50f, 0,0,0,0};
        data.cities[3] = {"Osaka",      "OSK", 34.69f, 135.50f, 0,0,0,0};
        data.quake.found = false;
        data.valid = false;
        data.last_update = 0;
    }

    bool fetchAll() {
        data.valid = false;
        bool w = fetchWeather();
        bool a = fetchAQI();
        fetchEarthquake();
        data.valid = w;
        data.last_update = millis();
        Serial.printf("Weather: %s  AQI: %s\n", w ? "OK" : "FAIL", a ? "OK" : "FAIL");
        for (int i = 0; i < WX_NUM_CITIES; i++) {
            Serial.printf("  %s: %.1fC H:%d%% AQI:%d Rain:%d%%\n",
                data.cities[i].short_name, data.cities[i].temp,
                data.cities[i].humidity, data.cities[i].aqi, data.cities[i].rain_prob);
        }
        if (data.quake.found) {
            Serial.printf("  Quake: M%.1f %s %.0fkm from %s\n",
                data.quake.magnitude, data.quake.place.c_str(),
                data.quake.dist_km, data.quake.nearest_city.c_str());
        }
        return data.valid;
    }

private:
    float haversineKm(float lat1, float lon1, float lat2, float lon2) {
        float dLat = radians(lat2 - lat1);
        float dLon = radians(lon2 - lon1);
        float a = sin(dLat / 2.0f) * sin(dLat / 2.0f) +
                  cos(radians(lat1)) * cos(radians(lat2)) *
                  sin(dLon / 2.0f) * sin(dLon / 2.0f);
        return 6371.0f * 2.0f * atan2(sqrtf(a), sqrtf(1.0f - a));
    }

    float getImpactRadius(float mag) {
        if (mag >= 8.0f) return 3000;
        if (mag >= 7.0f) return 1500;
        if (mag >= 6.0f) return 800;
        if (mag >= 5.0f) return 300;
        if (mag >= 4.5f) return 150;
        return 80;
    }

    bool fetchWeather() {
        HTTPClient http;
        http.begin("https://api.open-meteo.com/v1/forecast?"
                   "latitude=18.79,19.91,13.76,34.69"
                   "&longitude=98.98,99.83,100.50,135.50"
                   "&current=temperature_2m,relative_humidity_2m"
                   "&hourly=precipitation_probability"
                   "&forecast_hours=12&timezone=auto");
        http.setTimeout(10000);
        int code = http.GET();
        if (code != 200) {
            data.error_msg = "Weather HTTP " + String(code);
            http.end();
            return false;
        }

        String payload = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, payload)) {
            data.error_msg = "Weather JSON error";
            return false;
        }

        // Multi-location returns a JSON array
        JsonArray arr = doc.as<JsonArray>();
        for (int i = 0; i < WX_NUM_CITIES && i < (int)arr.size(); i++) {
            JsonObject city = arr[i];
            data.cities[i].temp = city["current"]["temperature_2m"] | 0.0f;
            data.cities[i].humidity = city["current"]["relative_humidity_2m"] | 0;

            // Max rain probability in next 12 hours
            JsonArray probs = city["hourly"]["precipitation_probability"];
            int maxP = 0;
            for (int j = 0; j < (int)probs.size() && j < 12; j++) {
                int p = probs[j] | 0;
                if (p > maxP) maxP = p;
            }
            data.cities[i].rain_prob = maxP;
        }
        return true;
    }

    bool fetchAQI() {
        HTTPClient http;
        http.begin("https://air-quality-api.open-meteo.com/v1/air-quality?"
                   "latitude=18.79,19.91,13.76,34.69"
                   "&longitude=98.98,99.83,100.50,135.50"
                   "&current=us_aqi");
        http.setTimeout(10000);
        int code = http.GET();
        if (code != 200) { http.end(); return false; }

        String payload = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, payload)) return false;

        JsonArray arr = doc.as<JsonArray>();
        for (int i = 0; i < WX_NUM_CITIES && i < (int)arr.size(); i++) {
            data.cities[i].aqi = arr[i]["current"]["us_aqi"] | 0;
        }
        return true;
    }

    bool fetchEarthquake() {
        data.quake.found = false;
        HTTPClient http;
        http.begin("https://earthquake.usgs.gov/fdsnws/event/1/query?"
                   "format=geojson&minmagnitude=4.0&limit=20&orderby=time");
        http.setTimeout(10000);
        int code = http.GET();
        if (code != 200) { http.end(); return false; }

        String payload = http.getString();
        http.end();

        // Use filter to reduce memory usage
        JsonDocument filter;
        filter["features"][0]["properties"]["mag"] = true;
        filter["features"][0]["properties"]["place"] = true;
        filter["features"][0]["properties"]["time"] = true;
        filter["features"][0]["geometry"]["coordinates"] = true;

        JsonDocument doc;
        if (deserializeJson(doc, payload, DeserializationOption::Filter(filter)))
            return false;

        JsonArray features = doc["features"];
        float bestScore = 0;

        for (JsonObject f : features) {
            float mag   = f["properties"]["mag"] | 0.0f;
            float qLon  = f["geometry"]["coordinates"][0] | 0.0f;
            float qLat  = f["geometry"]["coordinates"][1] | 0.0f;
            float depth = f["geometry"]["coordinates"][2] | 0.0f;
            const char* place = f["properties"]["place"] | "Unknown";
            unsigned long qTime = f["properties"]["time"] | 0UL;

            float impactR = getImpactRadius(mag);

            for (int i = 0; i < WX_NUM_CITIES; i++) {
                float dist = haversineKm(
                    data.cities[i].lat, data.cities[i].lon, qLat, qLon);
                if (dist < impactR) {
                    float score = mag * 100.0f / (dist + 1.0f);
                    if (score > bestScore) {
                        bestScore = score;
                        data.quake.found = true;
                        data.quake.magnitude = mag;
                        data.quake.depth_km = depth;
                        data.quake.place = String(place);
                        data.quake.dist_km = dist;
                        data.quake.nearest_city = data.cities[i].short_name;
                        data.quake.time_epoch = qTime / 1000;
                    }
                }
            }
        }
        return true;
    }
};
