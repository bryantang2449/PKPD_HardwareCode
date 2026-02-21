# Solo Hiker SmartSync Insight System — Hardware Client

This component runs on an ESP32 microcontroller and provides solo hikers with real-time GPS navigation, weather information, and public transport ETAs on a portable LCD display.

> **Related repository:** [pkpd-server](https://github.com/Cinnoline/pkpd-server) — the Express.js backend that powers the data APIs.

## Features

- **Real-time Map Display** — Fetches and renders Google Static Maps based on GPS coordinates, with zoom in/out controls. Auto-refreshes every 5 seconds.
- **Bus ETA (KMB & GMB)** — Displays estimated time of arrival for nearby KMB and Green Minibus stops within 3 km. Auto-refreshes every 30 seconds with pagination for multiple routes.
- **Weather Information** — Shows current weather warnings, weather reports, and forecasts across two navigable pages with proper word-wrapping.
- **GPS Tracking** — Reads real-time coordinates from the NEO-6M GPS module and sends location data to the server every minute for safety monitoring.
- **Emergency Alert System** — The server detects if the hiker has been stationary for over 4 hours and automatically sends email alerts to emergency contacts with a Google Maps link.
- **Button-driven Navigation** — 4-button UI (Up, Down, Enter, Back) for menu selection, page scrolling, and screen navigation — designed for simplicity and outdoor usability.

## System Architecture

```
┌─────────────┐     HTTP requests     ┌──────────────────┐     API calls     ┌─────────────┐
│  ESP32 +    │ ──────────────────►   │  Express.js      │ ───────────────►  │ DATA.GOV.HK │
│  ILI9488    │ ◄──────────────────   │  Server          │ ◄───────────────  │ CSDI        │
│  LCD        │     JSON responses    │  (MongoDB)       │     Open data     │ HK Obs.     │
│  + NEO-6M   │                       │  (DigitalOcean)  │                   └─────────────┘
│  GPS        │                       └──────────────────┘
└─────────────┘                              │
      4 Buttons                              │ Email alert (Nodemailer)
      (Up/Down/Enter/Back)                   ▼
                                      Emergency Contact
```

## Hardware Components

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32-DevKitC-V4 (WROOM-32E) | Main processing unit, Wi-Fi connectivity |
| Display | 4.0" TFT LCD (ILI9488 driver) | Information display via SPI bus |
| GPS Module | NEO-6M V2 | Real-time positioning |
| Buttons | 4× tactile buttons | Menu navigation (Up, Down, Enter, Back) |
| Power | USB power bank | Portable power supply |
| Other | Cables, resistors, PCB, pin headers | Circuit connections |

## Menu Structure

```
Main Menu
├── KMB ETA          → Nearby KMB bus stop ETAs (paginated, auto-refresh 30s)
├── GMB ETA          → Nearby GMB bus stop ETAs (paginated, auto-refresh 30s)
├── View Map         → Google Static Map with current position & nearby facilities
│                      (zoom in/out, auto-refresh 5s)
└── Weather Info     → Page 1: Warnings + Weather Report
                       Page 2: Weather Forecast
```

## Prerequisites

- **Arduino IDE** — [Download](https://www.arduino.cc/en/software)
- **ESP32 Board Manager** — Add `https://dl.espressif.com/dl/package_esp32_index.json` to Arduino IDE Board Manager URLs
- **VCP Driver** — Install the USB-to-UART driver for your OS if needed

### Required Libraries

Install the following libraries via Arduino IDE Library Manager or manually:

- `TFT_eSPI` — LCD display driver ([GitHub](https://github.com/Bodmer/TFT_eSPI))
- `TinyGPSPlus` — GPS NMEA parsing
- `ArduinoJson` — JSON parsing for API responses
- `WiFi` (built-in) — Wi-Fi connectivity
- `HTTPClient` (built-in) — HTTP requests to the backend server

### TFT_eSPI Configuration

You need to configure `TFT_eSPI` for the ILI9488 display. Edit the `User_Setup.h` file in the library folder:

```cpp
#define ILI9488_DRIVER
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
```

> **Note:** Pin assignments may vary depending on your wiring. Check your circuit connections.

## Setup & Usage

1. **Clone the repository**
   ```bash
   git clone https://github.com/bryantang2449/PKPD_HardwareCode.git
   ```

2. **Open `finalcode.ino`** in Arduino IDE

3. **Configure Wi-Fi credentials** — Update the SSID and password in the code to connect to your network

4. **Configure server URL** — Update the backend server URL to point to your running instance of [pkpd-server](https://github.com/Cinnoline/pkpd-server)

5. **Select board** — Choose `ESP32 Dev Module` in Arduino IDE under Tools → Board

6. **Upload** — Connect the ESP32 via USB and upload the code

7. **Use outdoors** — GPS module requires a clear view of the sky. First fix typically takes 1–2 minutes in open areas.

## Pin Connections

```
ESP32           ILI9488 LCD
─────           ───────────
3V3  ────────── VCC
GND  ────────── GND
GPIO 23 ─────── MOSI (SDA)
GPIO 19 ─────── MISO
GPIO 18 ─────── SCLK (SCL)
GPIO 15 ─────── CS
GPIO 2  ─────── DC (RS)
GPIO 4  ─────── RST

ESP32           NEO-6M GPS
─────           ──────────
3V3  ────────── VCC
GND  ────────── GND
GPIO 16 ─────── TX
GPIO 17 ─────── RX

ESP32           Buttons
─────           ───────
GPIO 32 ─────── Up
GPIO 33 ─────── Down
GPIO 25 ─────── Enter
GPIO 26 ─────── Back
```

> **Note:** Pin assignments shown above are for reference. Please verify against your actual wiring and adjust the code if needed.

## API Endpoints Used

The hardware client communicates with the backend server via HTTP GET/POST requests:

| Endpoint | Method | Description |
|---|---|---|
| `/weather/weather_forecast` | GET | Weather forecast text |
| `/weather/warning_info` | GET | Current weather warnings |
| `/transport/kmbStops/nearest?lat=&long=` | GET | Nearby KMB bus ETAs |
| `/transport/gmbStops/nearest?lat=&long=` | GET | Nearby GMB bus ETAs |
| `/map?lat=&long=` | GET | Google Static Map URL with facility markers |
| `/location/track` | POST | Send GPS coordinates for safety tracking |

For the full API documentation, see the [server repository README](https://github.com/Cinnoline/pkpd-server#for-users).

## Challenges & Lessons Learned

- **GPS Module Quality** — 2 out of 3 purchased NEO-6M modules were defective; thorough testing of hardware components is critical
- **Display Formatting Without Graphics Library** — All UI rendering was done manually without a graphics library (e.g., LVGL), requiring custom word-wrapping and pagination logic
- **JSON Parsing on ESP32** — Complex JSON responses were reformatted on the server side to reduce client-side parsing overhead
- **Color Calibration** — The ILI9488 required color correction as initial RGB values displayed inverted colors

## Team & My Contribution

This was a 4-person group project. My contributions (Tang Chun Leung / Bryan):

- **Hardware programming** — All ESP32 firmware including menu system, display rendering, GPS integration, and API communication
- **UI design** — Button-driven navigation, pagination, auto-refresh logic, and word-wrapping
- **Debugging** — Hardware troubleshooting, display calibration, and integration testing

## License

This project was developed as part of the CCIT4080 course at HKUSPACE Community College (2024–2025).

## Acknowledgements

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- [DATA.GOV.HK](https://data.gov.hk/en/) and [Common Spatial Data Infrastructure](https://portal.csdi.gov.hk/) for open data APIs
- [Google Maps Static API](https://developers.google.com/maps/documentation/maps-static) for map rendering
- Map marker icon authors: [bsd](https://www.flaticon.com/authors/bsd), [meaicon](https://www.flaticon.com/authors/meaicon), [Freepik](https://www.freepik.com/), [Icongeek26](https://www.flaticon.com/authors/icongeek26)
