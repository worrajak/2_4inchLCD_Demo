# CYD Price Tracker & Weather Dashboard

Real-time price dashboard + weather/earthquake monitor + Islamic prayer times on ESP32 CYD (Cheap Yellow Display) 2.4" ILI9341 with XPT2046 touch screen.

**3-page swipeable interface** - tap header to switch pages.

## Page 1: Price Tracker

- **Gold** - XAU/USD spot price + THB per baht-weight (Font4 large)
- **Thai Fuel Prices (PTT)** - Diesel, Gasohol 95, Gasohol 91 (Baht/Litre)
- **Bitcoin (BTC)** - USD price with 24h change %
- **Solana (SOL)** - USD price with 24h change %
- **FX Rates** - THB/USD, CNY/THB, JPY/THB
- **Breaking News** - Middle East / Gulf conflict headlines (auto word-wrap)
- **Thai Clock** - NTP-synced date & time (UTC+7) on header bar

Auto-refresh every 60 seconds.

## Page 2: Weather & Earthquake

Real-time weather for 4 cities in a 2x2 grid layout:

| City | Color | Data |
|------|-------|------|
| Chiang Mai | Gold | Temp, Humidity, AQI, Rain forecast |
| Chiang Rai | Green | Temp, Humidity, AQI, Rain forecast |
| Bangkok | Cyan | Temp, Humidity, AQI, Rain forecast |
| Osaka | Magenta | Temp, Humidity, AQI, Rain forecast |

- **Temperature** - Large Font4 hero number (°C)
- **Humidity** - Percentage with blue tint
- **AQI** - Color-coded badge (Green/Yellow/Orange/Red/Purple)
- **Rain Forecast** - Max probability in next 12 hours + text (Rain!/Maybe/Clear)
- **Earthquake Alert** - Filters M4.0+ quakes by impact radius to show only those affecting the 4 cities. Displays magnitude badge, distance, location, time ago, and depth.

Auto-refresh every 5 minutes.

## Page 3: Islamic Prayer Times

Daily salah schedule for one of 4 Thai cities (Chiang Mai, Chiang Rai, Bangkok, Songkhla):

- **5 prayer times** - Fajr, Dhuhr, Asr, Maghrib, Isha (Font4 large)
- **Next prayer** highlighted in green based on current local time
- **Hijri date** - day, month, year AH
- **Sunrise / Sunset** times at the bottom
- **City selector** - tap right side of header to cycle through 4 cities (saved to NVS)

Auto-refresh once per day (or when city changes). Uses [Aladhan API](https://aladhan.com/prayer-times-api) (method 2 = ISNA).

## Display Layout

```
Page 1 - Prices:
+--------------------------------------+
| Prices  (o)(.)  25/03 14:35  [wifi]  |  Header + page dots + clock
|--------------------------------------|
| GOLD   $3,045.20    42,350 THB/bt    |
| FUEL   DSL 50.54 95 43.95 91 43.58  |
| BTC +2.3%  $87,432  SOL -1.2% $142  |
| FX  THB/$ 34.25  CNY/B 4.82 JPY/B   |
|======================================|
| Iran warns of retaliation after...   |  Breaking news
+--------------------------------------+

Page 2 - Weather:
+--------------------------------------+
| Weather (.)( o)  25/03 14:35 [wifi]  |  Header + page dots
|--------------------------------------|
| Chiang Mai   75% | Chiang Rai   80% |
| 32.5 oC          | 30.2 oC          |
| [AQI:89] R:40%   | [AQI:65] R:55%   |
|-------------------+------------------|
| Bangkok      60% | Osaka        50% |
| 35.1 oC          | 22.3 oC          |
| [AQI:120] R:20%  | [AQI:45] R:10%   |
|======================================|
| EARTHQUAKE  [M5.2]  200km>CM        |  Filtered by impact
| 85km NW of Mandalay, Myanmar        |  radius to 4 cities
| 2h ago | Depth: 10km                |
+--------------------------------------+
```

## Touch Controls

| Action | Area |
|--------|------|
| Switch page | Tap left side of header (x < 170) |
| Force refresh / Cycle city (Prayer page) | Tap middle-right of header (170 <= x < 285) |
| Cycle backlight brightness | Tap WiFi dot area (x > 285) |

## Hardware

- ESP32-2432S028 (CYD 2.4" with ILI9341 + XPT2046 touch)
- Display: 320x240 landscape, HSPI
- Touch: Raw Z pressure + raw XY mapping (calibrated)

## APIs Used (all free, no API key needed)

| Data | API |
|------|-----|
| BTC, SOL, Gold (XAUT) | [CoinGecko](https://api.coingecko.com) |
| THB/USD, JPY, CNY exchange rate | [open.er-api.com](https://open.er-api.com) |
| Thai fuel prices (PTT) | [thai-oil-api](https://api.chnwt.dev/thai-oil-api/latest) |
| Breaking news | [Google News RSS](https://news.google.com) via [rss2json](https://rss2json.com) |
| Weather (temp, humidity, rain) | [Open-Meteo](https://api.open-meteo.com) |
| Air Quality Index (AQI) | [Open-Meteo Air Quality](https://air-quality-api.open-meteo.com) |
| Earthquake data | [USGS Earthquake API](https://earthquake.usgs.gov) |
| Islamic prayer times + Hijri date | [Aladhan API](https://api.aladhan.com) |
| Thai time (NTP) | pool.ntp.org, time.nist.gov |

## WiFi Setup

On first boot (or if saved credentials fail):
1. Scans nearby WiFi networks
2. Shows list with signal strength + **EAP** badge for enterprise networks
3. For WPA2-Enterprise: on-screen keyboard for username, then password
   - Uses PEAP / MSCHAPv2 (compatible with eduroam, AD-based corporate WiFi)
   - Shows disconnect reason code on failure for diagnostics
4. For WPA2-Personal: on-screen keyboard for password (or "No Password")
5. Saves credentials to NVS flash for next boot

## Build

PlatformIO project for ESP32.

```bash
pio run -t upload
pio device monitor
```

## Pin Configuration

| Function | Pin |
|----------|-----|
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT SCLK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT BL | 27 |
| Touch CS | 33 |

## Project Structure

```
src/
  main.cpp            - Setup, loop, page switching, touch, NTP
  wifi_setup.h        - WiFi scan, WPA2-Enterprise, on-screen keyboard
  price_fetcher.h     - HTTP fetch: gold, crypto, fuel, FX, news
  display_ui.h        - Price dashboard layout, shared header
  weather_fetcher.h   - HTTP fetch: weather, AQI, earthquake
  weather_ui.h        - Weather page layout, 2x2 grid, quake section
  prayer_fetcher.h    - HTTP fetch: Islamic prayer times + Hijri date (Aladhan)
  prayer_ui.h         - Prayer page layout with next-prayer highlight
platformio.ini        - Build config with TFT_eSPI flags
```

## Earthquake Filtering Logic

Only shows earthquakes that could be **felt** at one of the 4 monitored cities:

| Magnitude | Impact Radius |
|-----------|--------------|
| M4.0-4.4 | 80 km |
| M4.5-4.9 | 150 km |
| M5.0-5.9 | 300 km |
| M6.0-6.9 | 800 km |
| M7.0-7.9 | 1,500 km |
| M8.0+ | 3,000 km |

Distance calculated using Haversine formula. Most impactful quake (highest magnitude/distance score) is displayed.

## License

MIT
