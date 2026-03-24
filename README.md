# CYD Price Tracker

Real-time price dashboard on ESP32 CYD (Cheap Yellow Display) 2.4" ILI9341 with XPT2046 touch screen.

## Features

- **Gold** - XAU/USD spot price + THB per baht-weight (Font4 large)
- **Thai Fuel Prices (PTT)** - Diesel B7, Gasohol 95, Gasohol 91 (Baht/Litre)
- **Bitcoin (BTC)** - USD price with 24h change %
- **Solana (SOL)** - USD price with 24h change %
- **THB/USD** - Exchange rate
- **Breaking News** - Middle East / Gulf conflict headlines (auto word-wrap, up to 5 lines)
- **Thai Clock** - NTP-synced date & time (UTC+7) on header bar

Auto-refresh every 60 seconds. Touch header bar to force refresh.

## Display Layout

```
+--------------------------------------+
| Price Tracker  24/03 14:35  [wifi]   |  Header + NTP clock
|--------------------------------------|
| GOLD   $3,045.20    42,350 THB/bt    |
| FUEL   B7 29.94  95 35.05  91 32.68 |
| BTC    $87,432              +2.3%    |
| SOL    $142.50              -1.2%    |
| THB    34.25              per USD    |
|======================================|
| Iran warns of retaliation after US   |  Breaking news
| carrier group enters Persian Gulf... |  (word-wrapped)
+--------------------------------------+
```

## Hardware

- ESP32-2432S028 (CYD 2.4" with ILI9341 + XPT2046 touch)
- Display: 320x240 landscape, HSPI
- Touch: Raw Z pressure + raw XY mapping (calibrated)

## APIs Used (all free, no API key)

| Data | API |
|------|-----|
| BTC, SOL, Gold (XAUT) | [CoinGecko](https://api.coingecko.com) |
| THB/USD exchange rate | [open.er-api.com](https://open.er-api.com) |
| Thai fuel prices (PTT) | [thai-oil-api](https://api.chnwt.dev/thai-oil-api/latest) |
| Breaking news | [Google News RSS](https://news.google.com) via [rss2json](https://rss2json.com) |
| Thai time (NTP) | pool.ntp.org, time.nist.gov |

## WiFi Setup

On first boot (or if saved credentials fail), the device:
1. Scans nearby WiFi networks
2. Shows a list on screen - tap to select
3. On-screen keyboard for password entry
4. Saves credentials to NVS flash for next boot

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
  main.cpp          - Setup, loop, touch handling, NTP sync
  wifi_setup.h      - WiFi scan, select, on-screen keyboard
  price_fetcher.h   - HTTP fetch from all APIs + news
  display_ui.h      - Dashboard layout, news display
platformio.ini      - Build config with TFT_eSPI flags
```

## License

MIT
