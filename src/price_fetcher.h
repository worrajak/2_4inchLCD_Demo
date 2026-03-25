#pragma once
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct PriceData {
    float gold_usd;      // XAU/USD per troy oz
    float gold_thb;      // Gold in THB per baht-weight (15.244g)
    float btc_usd;       // Bitcoin USD
    float sol_usd;       // Solana USD
    float thb_per_usd;   // THB/USD exchange rate
    float jpy_per_usd;   // JPY/USD exchange rate
    float cny_per_usd;   // CNY/USD exchange rate

    // 24h change percentages
    float btc_change_24h;
    float sol_change_24h;

    // Thai fuel prices (PTT, Baht/Litre)
    float diesel_b7;
    float gasohol_95;
    float gasohol_91;

    // Breaking news headlines
    String news1;
    String news2;

    bool valid;
    unsigned long last_update;
    String error_msg;

    void reset() {
        gold_usd = gold_thb = 0;
        btc_usd = sol_usd = thb_per_usd = jpy_per_usd = cny_per_usd = 0;
        btc_change_24h = sol_change_24h = 0;
        diesel_b7 = gasohol_95 = gasohol_91 = 0;
        news1 = "";
        news2 = "";
        valid = false;
        last_update = 0;
        error_msg = "";
    }
};

class PriceFetcher {
public:
    PriceData data;

    PriceFetcher() {
        data.reset();
    }

    // Fetch all prices - returns true if at least some data was fetched
    bool fetchAll() {
        bool ok = false;
        if (fetchCrypto()) ok = true;
        if (fetchExchangeRate()) ok = true;
        if (fetchGold()) ok = true;
        if (fetchThaiFuel()) ok = true;
        fetchNews();  // Best-effort, don't affect ok status

        if (ok) {
            data.valid = true;
            data.last_update = millis();
            // Calculate gold in THB per baht-weight if we have both rates
            if (data.gold_usd > 0 && data.thb_per_usd > 0) {
                // 1 baht-weight = 15.244 grams, 1 troy oz = 31.1035 grams
                data.gold_thb = data.gold_usd * data.thb_per_usd * 15.244 / 31.1035;
            }
        }
        return ok;
    }

    // Fetch BTC and SOL from CoinGecko
    bool fetchCrypto() {
        HTTPClient http;
        http.begin("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,solana&vs_currencies=usd&include_24hr_change=true");
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String payload = http.getString();
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                data.btc_usd = doc["bitcoin"]["usd"] | 0.0f;
                data.btc_change_24h = doc["bitcoin"]["usd_24h_change"] | 0.0f;
                data.sol_usd = doc["solana"]["usd"] | 0.0f;
                data.sol_change_24h = doc["solana"]["usd_24h_change"] | 0.0f;
                http.end();
                return true;
            }
        }
        data.error_msg = "Crypto API err:" + String(code);
        http.end();
        return false;
    }

    // Fetch THB/USD exchange rate
    bool fetchExchangeRate() {
        HTTPClient http;
        // Using open.er-api.com (free, no key needed)
        http.begin("https://open.er-api.com/v6/latest/USD");
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String payload = http.getString();
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                data.thb_per_usd = doc["rates"]["THB"] | 0.0f;
                data.jpy_per_usd = doc["rates"]["JPY"] | 0.0f;
                data.cny_per_usd = doc["rates"]["CNY"] | 0.0f;
                http.end();
                return true;
            }
        }
        data.error_msg = "FX API err:" + String(code);
        http.end();
        return false;
    }

    // Fetch gold price (XAU/USD)
    bool fetchGold() {
        HTTPClient http;
        // Using free metals API from CoinGecko (gold as commodity)
        // Alternative: use frankfurter or metals-api
        http.begin("https://api.coingecko.com/api/v3/simple/price?ids=tether-gold&vs_currencies=usd");
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String payload = http.getString();
            JsonDocument doc;
            if (!deserializeJson(doc, payload)) {
                // tether-gold (XAUT) tracks gold price closely
                data.gold_usd = doc["tether-gold"]["usd"] | 0.0f;
                http.end();
                return true;
            }
        }
        data.error_msg = "Gold API err:" + String(code);
        http.end();
        return false;
    }

    // Fetch Thai fuel prices from PTT via thai-oil-api
    bool fetchThaiFuel() {
        HTTPClient http;
        http.begin("https://api.chnwt.dev/thai-oil-api/latest");
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String payload = http.getString();
            // Use filter to reduce memory usage (full JSON is very large)
            JsonDocument filter;
            filter["response"]["stations"]["ptt"]["diesel_b7"] = true;
            filter["response"]["stations"]["ptt"]["gasohol_95"] = true;
            filter["response"]["stations"]["ptt"]["gasohol_91"] = true;

            JsonDocument doc;
            if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
                JsonObject ptt = doc["response"]["stations"]["ptt"];
                const char* d = ptt["diesel_b7"]["price"];
                const char* g95 = ptt["gasohol_95"]["price"];
                const char* g91 = ptt["gasohol_91"]["price"];
                if (d) data.diesel_b7 = atof(d);
                if (g95) data.gasohol_95 = atof(g95);
                if (g91) data.gasohol_91 = atof(g91);
                http.end();
                return (data.diesel_b7 > 0);
            }
        }
        data.error_msg = "Fuel API err:" + String(code);
        http.end();
        return false;
    }

    // Fetch breaking news via Google News RSS -> rss2json
    void fetchNews() {
        HTTPClient http;
        // Google News RSS search: middle east war / gulf conflict
        http.begin("https://api.rss2json.com/v1/api.json?rss_url=https%3A%2F%2Fnews.google.com%2Frss%2Fsearch%3Fq%3Dmiddle%2Beast%2Bwar%2BOR%2Bgulf%2Bconflict%26hl%3Den-US%26gl%3DUS%26ceid%3DUS%3Aen");
        http.setTimeout(10000);
        int code = http.GET();

        if (code == 200) {
            String payload = http.getString();
            // Filter to only parse items[0] and items[1] title
            JsonDocument filter;
            filter["items"][0]["title"] = true;

            JsonDocument doc;
            if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
                JsonArray items = doc["items"];
                if (items.size() > 0) {
                    const char* t0 = items[0]["title"];
                    if (t0) data.news1 = String(t0);
                }
                if (items.size() > 1) {
                    const char* t1 = items[1]["title"];
                    if (t1) data.news2 = String(t1);
                }
            }
        }
        http.end();
    }
};
