# Solo Hiker SmartSync Insight System — Hardware Client

This component runs on an ESP32 microcontroller and provides solo hikers with real-time GPS navigation, weather information, and public transport ETAs on a portable LCD display.

> **Related repository:** [pkpd-server](https://github.com/Cinnoline/pkpd-server) — the Express.js backend that powers the data APIs.

---

## Features

- **Real-time Map Display** — Fetches and renders Google Static Maps based on GPS coordinates, with zoom in/out controls. Auto-refreshes every 5 seconds.
- **Bus ETA (KMB & GMB)** — Displays estimated time of arrival for nearby KMB and Green Minibus stops within 3 km. Auto-refreshes every 30 seconds with pagination for multiple routes.
- **Weather Information** — Shows current weather warnings, weather reports, and forecasts across two navigable pages with proper word-wrapping.
- **GPS Tracking** — Reads real-time coordinates from the NEO-6M GPS module and sends location data to the server every minute for safety monitoring.
- **Emergency Alert System** — The server detects if the hiker has been stationary for over 4 hours and automatically sends email alerts to emergency contacts with a Google Maps link.
- **Button-driven Navigation** — 4-button UI for menu selection, page scrolling, and screen navigation — designed for simplicity and outdoor usability.

---

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

---

## Hardware Components

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32-DevKitC-V4 (WROOM-32E) | Main processing unit, Wi-Fi connectivity |
| Display | 4.0" TFT LCD (ILI9488 driver) | Information display via SPI bus |
| GPS Module | NEO-6M V2 | Real-time positioning |
| Buttons | 4× tactile buttons | Menu navigation (Forward, Back, Up, Down) |
| Power | USB power bank | Portable power supply |
| Other | Cables, resistors, PCB, pin headers | Circuit connections |

---

## Required Libraries

Install via Arduino IDE Library Manager:

| Library | Purpose |
|---|---|
| `TFT_eSPI` | ILI9488 LCD display driver ([GitHub](https://github.com/Bodmer/TFT_eSPI)) |
| `TinyGPSPlus` | GPS NMEA sentence parsing |
| `ArduinoJson` | JSON parsing for API responses |
| `WiFi` (built-in) | Wi-Fi connectivity |
| `HTTPClient` (built-in) | HTTP requests to backend |

---

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
      (Forward/Back/Up/Down)                 ▼
                                      Emergency Contact
```

---

## How the Code Works

### Button & Pin Definitions

Four physical buttons are mapped to GPIO pins for all user interaction:

```cpp
#define BUTTON_FORWARD 14   // Enter / select menu option
#define BUTTON_BACK    27   // Return to main menu
#define BUTTON_UP      12   // Scroll up / zoom in / previous page
#define BUTTON_DOWN    13   // Scroll down / zoom out / next page
```

### Startup Flow

On power-up, the ESP32 performs the following initialization sequence:

```cpp
void setup() {
    Serial.begin(115200);

    // 1. Initialize LCD display via SPI
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // 2. Initialize GPS module via hardware serial
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    // 3. Configure button pins
    pinMode(BUTTON_FORWARD, INPUT_PULLUP);
    pinMode(BUTTON_BACK, INPUT_PULLUP);
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);

    // 4. Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }

    // 5. Display main menu
    drawMainMenu();
}
```

### Main Menu Navigation

The menu system uses a state-based approach. The `loop()` reads button inputs and switches between screens:

```cpp
void loop() {
    // Read GPS data continuously
    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }

    // Handle button presses based on current screen
    if (digitalRead(BUTTON_FORWARD) == LOW) {
        switch (currentMenuItem) {
            case 0: showKmbEta();     break;
            case 1: showGmbEta();     break;
            case 2: showMap();        break;
            case 3: showWeather();    break;
        }
    }

    if (digitalRead(BUTTON_BACK) == LOW) {
        drawMainMenu();
    }

    if (digitalRead(BUTTON_UP) == LOW) {
        handleUpButton();
    }

    if (digitalRead(BUTTON_DOWN) == LOW) {
        handleDownButton(); 
    }
}
```

### GPS Data Reading

The NEO-6M module outputs NMEA sentences over serial. `TinyGPSPlus` parses them into usable coordinates:

```cpp
#include <TinyGPS++.h>

TinyGPSPlus gps;
HardwareSerial gpsSerial(1);  // Use UART1

// In loop — continuously feed GPS data
while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
}

// Once a valid fix is obtained
if (gps.location.isValid()) {
    float latitude  = gps.location.lat();  // e.g. 22.3849
    float longitude = gps.location.lng();  // e.g. 114.1438
}
```

### HTTP Requests to Backend

The ESP32 uses `HTTPClient` to fetch data from the Express.js server. Example — fetching nearby KMB bus ETAs:

```cpp
#include <HTTPClient.h>

String fetchData(String endpoint) {
    HTTPClient http;
    String url = serverBaseUrl + endpoint;
    http.begin(url);

    int httpCode = http.GET();
    String payload = "";

    if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
    }

    http.end();
    return payload;
}

// Usage — pass current GPS coordinates
String kmbData = fetchData(
    "/transport/kmbStops/nearest?lat=" + String(latitude, 6) +
    "&long=" + String(longitude, 6)
);
```

The request-response flow:

```
User selects "KMB ETA"
        │
        ▼
ESP32 sends GET /transport/kmbStops/nearest?lat=22.3849&long=114.1438
        │
        ▼
Server queries MongoDB for nearby stops (Haversine formula, 3km radius)
  + fetches real-time ETA from KMB open data API
        │
        ▼
Returns pre-formatted string response
        │
        ▼
ESP32 renders text on LCD with pagination
```

### Sending GPS Location (POST)

Every 1 minute, the ESP32 sends its current position to the server for safety tracking:

```cpp
void sendLocationToServer() {
    HTTPClient http;
    http.begin(serverBaseUrl + "/location/track");
    http.addHeader("Content-Type", "application/json");

    // Build JSON body
    String body = "{\"time\":" + String(gps.time.value()) +
                  ",\"location\":[" + String(longitude, 6) + "," +
                  String(latitude, 6) + "]}";

    int httpCode = http.POST(body);
    http.end();
}
```

The server compares this with previous coordinates. If the hiker stays within **100 meters** for over **4 hours**, an alert email is sent automatically. After the first alert, follow-up emails are sent every hour.

### Map Display & Zoom

The map feature fetches a Google Static Maps URL from the backend, which includes markers for nearby facilities:

```cpp
void showMap() {
    String mapUrl = fetchData(
        "/map?lat=" + String(latitude, 6) +
        "&long=" + String(longitude, 6)
    );

    // Download the map image and display on LCD
    downloadAndDisplayImage(mapUrl);
}
```

- **Up button** → increments zoom level
- **Down button** → decrements zoom level
- Map **auto-refreshes every 5 seconds** as GPS position updates

The map displays markers for: current hiker position, nearby KMB stops, GMB stops, water filling stations, and BBQ areas.

### Auto-Refresh Logic

Different screens refresh at different intervals to balance usability and network usage:

```cpp
unsigned long lastMapRefresh = 0;
unsigned long lastEtaRefresh = 0;

void loop() {
    unsigned long now = millis();

    // Map auto-refresh every 5 seconds
    if (currentScreen == SCREEN_MAP && now - lastMapRefresh > 5000) {
        refreshMap();
        lastMapRefresh = now;
    }

    // Bus ETA auto-refresh every 30 seconds
    if ((currentScreen == SCREEN_KMB || currentScreen == SCREEN_GMB)
        && now - lastEtaRefresh > 30000) {
        refreshEta();
        lastEtaRefresh = now;
    }
}
```

### Word-Wrapping for LCD Display

Weather reports can be lengthy. A custom word-wrapping function ensures text fits the LCD width without breaking mid-word:

```cpp
void drawWrappedText(String text, int x, int y, int maxWidth) {
    String line = "";
    int cursorY = y;

    for (int i = 0; i < text.length(); i++) {
        line += text[i];

        // Check if current line exceeds display width
        if (tft.textWidth(line) > maxWidth) {
            // Find last space to break at word boundary
            int lastSpace = line.lastIndexOf(' ');
            if (lastSpace > 0) {
                String printLine = line.substring(0, lastSpace);
                tft.drawString(printLine, x, cursorY);
                line = line.substring(lastSpace + 1);
            } else {
                tft.drawString(line, x, cursorY);
                line = "";
            }
            cursorY += tft.fontHeight();
        }
    }

    // Print remaining text
    if (line.length() > 0) {
        tft.drawString(line, x, cursorY);
    }
}
```

### Pagination

When data exceeds one screen (e.g., many bus routes), pagination splits content into pages:

```cpp
int currentPage = 0;
int totalPages = 0;

void displayWithPagination(String lines[], int totalLines, int linesPerPage) {
    totalPages = (totalLines + linesPerPage - 1) / linesPerPage;
    int startIdx = currentPage * linesPerPage;
    int endIdx = min(startIdx + linesPerPage, totalLines);

    tft.fillScreen(TFT_BLACK);
    int y = 10;

    for (int i = startIdx; i < endIdx; i++) {
        tft.drawString(lines[i], 10, y);
        y += tft.fontHeight() + 4;
    }

    // Show page indicator
    tft.drawString("Page " + String(currentPage + 1) + "/" + String(totalPages),
                   10, tft.height() - 20);
}

// Up → previous page, Down → next page
void handleUpButton() {
    if (currentPage > 0) { currentPage--; refreshCurrentScreen(); }
}
void handleDownButton() {
    if (currentPage < totalPages - 1) { currentPage++; refreshCurrentScreen(); }
}
```

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

> **Note:** Verify pin assignments against your actual wiring. Check the `#define` statements at the top of `finalcode.ino` for exact values.

---

## API Endpoints Used

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

## Setup & Upload

1. **Clone the repository**
   ```bash
   git clone https://github.com/bryantang2449/PKPD_HardwareCode.git
   ```
2. **Open `finalcode.ino`** in Arduino IDE
3. **Install ESP32 Board Manager** — Add `https://dl.espressif.com/dl/package_esp32_index.json` to Board Manager URLs
4. **Install required libraries** — See [Required Libraries](#required-libraries) above
5. **Configure TFT_eSPI** — Edit `User_Setup.h` in the library folder:
   ```cpp
   #define ILI9488_DRIVER
   #define TFT_MISO 19
   #define TFT_MOSI 23
   #define TFT_SCLK 18
   #define TFT_CS   15
   #define TFT_DC    2
   #define TFT_RST   4
   ```
6. **Configure Wi-Fi** — Update SSID and password in the code
7. **Configure server URL** — Point to your running [pkpd-server](https://github.com/Cinnoline/pkpd-server) instance
8. **Select board** — `ESP32 Dev Module` under Tools → Board
9. **Upload** — Connect ESP32 via USB and upload
10. **Use outdoors** — GPS requires clear sky view. First fix takes 1–2 minutes.

---



## Acknowledgements

- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) by Bodmer
- [DATA.GOV.HK](https://data.gov.hk/en/) and [Common Spatial Data Infrastructure](https://portal.csdi.gov.hk/) for open data APIs
- [Google Maps Static API](https://developers.google.com/maps/documentation/maps-static) for map rendering
- Map marker icon authors: [bsd](https://www.flaticon.com/authors/bsd), [meaicon](https://www.flaticon.com/authors/meaicon), [Freepik](https://www.freepik.com/), [Icongeek26](https://www.flaticon.com/authors/icongeek26)
