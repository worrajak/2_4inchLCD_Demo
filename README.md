# CYD Price Tracker

Real-time price dashboard on ESP32 CYD (Cheap Yellow Display) 2.4" ILI9341 with XPT2046 touch screen.

## Features

- **Gold** - XAU/USD spot price + THB per baht-weight
- **Thai Fuel Prices (PTT)** - Diesel B7, Gasohol 95, Gasohol 91 (Baht/Litre)
- **Bitcoin (BTC)** - USD price with 24h change %
- **Solana (SOL)** - USD price with 24h change %
- **THB/USD** - Exchange rate

Auto-refresh every 60 seconds. Touch header bar to force refresh.

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
  main.cpp          - Setup, loop, touch handling
  wifi_setup.h      - WiFi scan, select, on-screen keyboard
  price_fetcher.h   - HTTP fetch from all APIs
  display_ui.h      - Dashboard layout and drawing
platformio.ini      - Build config with TFT_eSPI flags
```

## License

MIT
