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

### 1. Initialization

On power-up, the system:
- Connects to Wi-Fi
- Initializes the ILI9488 LCD via SPI
- Starts the NEO-6M GPS module via Serial
- Configures 4 button pins as inputs
- Displays the main menu screen

```cpp
#define BUTTON_FORWARD 14
#define BUTTON_BACK    27
#define BUTTON_UP      12
#define BUTTON_DOWN    13
```

### 2. Main Menu & Button Navigation

The main menu offers 4 options. Users navigate using physical buttons:

| Button | GPIO | Function |
|---|---|---|
| **Forward** (Enter) | GPIO 14 | Select / confirm a menu option |
| **Back** | GPIO 27 | Return to the main menu |
| **Up** | GPIO 12 | Scroll up / zoom in / previous page |
| **Down** | GPIO 13 | Scroll down / zoom out / next page |

```
Main Menu
├── KMB ETA          → Nearby KMB bus stop arrival times
├── GMB ETA          → Nearby Green Minibus arrival times
├── View Map         → Google Static Map with position & facilities
└── Weather Info     → Warnings, reports & forecasts
```

### 3. GPS Data Acquisition

The ESP32 reads NMEA data from the NEO-6M GPS module over a hardware serial connection. Once a satellite fix is obtained (typically 1–2 min in open areas), the latitude and longitude are used to:

- Request nearby bus stops, facilities, and weather from the backend
- Render the current position on a Google Static Map
- Send coordinates to the server via POST request every 1 minute for safety tracking

### 4. Fetching & Displaying Data

Each menu option triggers HTTP GET requests to the Express.js backend:

```
User selects "KMB ETA"
        │
        ▼
ESP32 sends GET /transport/kmbStops/nearest?lat=...&long=...
        │
        ▼
Server queries MongoDB + real-time KMB API
        │
        ▼
Returns formatted string response
        │
        ▼
ESP32 parses response and renders on LCD with pagination
```

**Key implementation details:**

- **Auto-refresh:** Map refreshes every **5 seconds**, bus ETAs refresh every **30 seconds**
- **Pagination:** When data exceeds one screen, Up/Down buttons scroll through pages
- **Word-wrapping:** Custom logic wraps long text (especially weather reports) to fit the LCD width, avoiding mid-word line breaks
- **JSON formatting:** Complex JSON responses are pre-formatted on the server side to reduce parsing load on the ESP32

### 5. Safety Monitoring

The ESP32 periodically sends its GPS coordinates to the server:

```
ESP32  ───POST /location/track───►  Server
       { time, location: [lng, lat] }
```

The server compares incoming coordinates using the **Haversine formula**. If the hiker stays within 100 meters for over **4 hours**, the server triggers an email alert via Nodemailer to the predefined emergency contact, containing:
- Last known coordinates
- A direct Google Maps link to the location

After the initial alert, follow-up emails are sent every hour.

### 6. Map Display & Zoom

The map feature requests a Google Static Maps URL from the backend, which includes markers for:
- 📍 Current hiker position
- 🚌 Nearby KMB bus stops
- 🚐 Nearby GMB bus stops
- 💧 Water filling stations
- 🔥 BBQ areas

The Up/Down buttons control zoom level, and the map image auto-refreshes every 5 seconds as the GPS position updates.

---

## Pin Connections

```
ESP32           ILI9488 LCD (SPI)
─────           ─────────────────
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
GPIO 14 ─────── Forward (Enter)
GPIO 27 ─────── Back
GPIO 12 ─────── Up
GPIO 13 ─────── Down
```

> **Note:** Verify pin assignments against your actual wiring. The LCD SPI and GPS serial pins above are based on common ESP32 configurations — check the `#define` statements at the top of `finalcode.ino` for exact values.

---

## API Endpoints Used

The hardware client communicates with the backend server via HTTP:

| Endpoint | Method | Description |
|---|---|---|
| `/weather/weather_forecast` | GET | Weather forecast text |
| `/weather/warning_info` | GET | Current weather warnings |
| `/weather/weather_report?lat=&long=` | GET | Weather report from nearest station |
| `/transport/kmbStops/nearest?lat=&long=` | GET | Nearby KMB bus ETAs |
| `/transport/gmbStops/nearest?lat=&long=` | GET | Nearby GMB bus ETAs |
| `/map?lat=&long=` | GET | Google Static Map URL with facility markers |
| `/location/track` | POST | Send GPS coordinates for safety tracking |

For the full API documentation, see the [server repository README](https://github.com/Cinnoline/pkpd-server#for-users).

---

## Prerequisites & Setup

### Requirements

- **Arduino IDE** — [Download](https://www.arduino.cc/en/software)
- **ESP32 Board Manager** — Add `https://dl.espressif.com/dl/package_esp32_index.json` to Arduino IDE Board Manager URLs
- **VCP Driver** — Install the USB-to-UART driver for your OS if needed

### Required Libraries

Install via Arduino IDE Library Manager:

| Library | Purpose |
|---|---|
| `TFT_eSPI` | ILI9488 LCD display driver ([GitHub](https://github.com/Bodmer/TFT_eSPI)) |
| `TinyGPSPlus` | GPS NMEA sentence parsing |
| `ArduinoJson` | JSON parsing for API responses |
| `WiFi` (built-in) | Wi-Fi connectivity |
| `HTTPClient` (built-in) | HTTP requests to backend |

### TFT_eSPI Configuration

Edit `User_Setup.h` in the TFT_eSPI library folder to match the ILI9488:

```cpp
#define ILI9488_DRIVER
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
```

### Upload Steps

1. **Clone the repository**
   ```bash
   git clone https://github.com/bryantang2449/PKPD_HardwareCode.git
   ```

2. **Open `finalcode.ino`** in Arduino IDE

3. **Configure Wi-Fi** — Update the SSID and password in the code

4. **Configure server URL** — Update the backend URL to point to your running instance of [pkpd-server](https://github.com/Cinnoline/pkpd-server)

5. **Select board** — `ESP32 Dev Module` under Tools → Board

6. **Upload** — Connect ESP32 via USB and upload

7. **Use outdoors** — GPS requires clear sky view. First fix takes 1–2 minutes.

---

## Challenges & Lessons Learned

- **GPS Module Quality** — 2 out of 3 purchased NEO-6M modules were defective; always test hardware components early
- **Display Formatting Without Graphics Library** — All UI rendering was done manually without LVGL or similar, requiring custom word-wrapping and pagination logic
- **JSON Parsing on ESP32** — Complex responses were reformatted server-side to reduce client parsing overhead
- **Color Calibration** — ILI9488 initially displayed inverted RGB values, requiring color correction
- **Git Security** — Credentials were accidentally committed; we had to rewrite commit history using `git filter-repo`

---

## Team & My Contribution

This was a 4-person group project for CCIT4080. My contributions (Tang Chun Leung / Bryan):

- **Hardware programming** — All ESP32 firmware: menu system, display rendering, GPS integration, API communication, auto-refresh, pagination
- **UI design** — Button-driven navigation, screen layouts, word-wrapping logic
- **Debugging** — Hardware troubleshooting, display calibration, end-to-end integration testing

---

## Acknowledgements

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- [DATA.GOV.HK](https://data.gov.hk/en/) and [Common Spatial Data Infrastructure](https://portal.csdi.gov.hk/) for open data APIs
- [Google Maps Static API](https://developers.google.com/maps/documentation/maps-static) for map rendering
- Map marker icon authors: [bsd](https://www.flaticon.com/authors/bsd), [meaicon](https://www.flaticon.com/authors/meaicon), [Freepik](https://www.freepik.com/), [Icongeek26](https://www.flaticon.com/authors/icongeek26)
