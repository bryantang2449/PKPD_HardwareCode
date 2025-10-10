#include <WiFi.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <Bounce2.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>   
#include <vector>
#include "time.h"

// WiFi connection
const char* ssid = "YourSSID";
const char* password = "YOURPASSWORD";

// Base URL
const char* baseUrl = "https://pkpd-server-zil3m.ondigitalocean.app";
String _mapUrl = "";
int currentZoomLevel = 15; // Default zoom level
#define MAP_FILE "/map.jpg"

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;// GMT+8 for Hong Kong (8 hours * 60 * 60 seconds)
const int   daylightOffset_sec = 0;// Hong Kong doesn't use Daylight Saving Time

// Define button pins
#define BUTTON_FORWARD 14
#define BUTTON_BACK 27
#define BUTTON_UP 12
#define BUTTON_DOWN 13

// Initialize the screen
TFT_eSPI tft = TFT_eSPI();

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// Location
float latitude = 22.324390;
float longitude = 114.211037;
unsigned long lastMapUpdateTime = 0; // Tracks last map update
const unsigned long mapUpdateInterval = 5000; // Auto-refresh map every 5 seconds
unsigned long lastLocationUpdateTime = 0; // Tracks last location send
const unsigned long locationUpdateInterval = 60000; // 60 seconds

// For KMB
String busETAData;           // Stores Bus ETA data
std::vector<String> busRows; // Stores individual rows of Bus ETA data
int rowsPerPage = 12;        // Number of rows to display per page
int currentPage = 0;         // Tracks current page
int totalPages = 0;          // Total number of pages
bool inBusETA = false;       // Whether in Bus ETA page
unsigned long lastBusETAUpdateTime = 0; // Tracks last Bus ETA update
const unsigned long busETAUpdateInterval = 30000; // 30 seconds

// For GMB
String gmbETAData;           // Stores GMB ETA data
std::vector<String> gmbRows; // Stores individual rows of GMB ETA data
int gmbrowsPerPage = 12;     // Number of rows to display per page
int gmbCurrentPage = 0;      // Tracks current page for GMB
int gmbTotalPages = 0;       // Total number of pages for GMB
bool inGmbETA = false;       // Whether in GMB ETA page
unsigned long lastGmbETAUpdateTime = 0; // Tracks last GMB ETA update
const unsigned long gmbETAUpdateInterval = 30000; // 30 seconds

// For Weather Information
bool inWeather = false;       // Whether in Weather Information view
int weatherPage = 0;          // Current weather page (0 for Warning, 1 for Forecast)
String weatherReportInfo;     // Stores weather report "info" text 
String weatherWarningData;    // Stores warning information
String weatherForecastData;   // Stores forecast data
const int totalWeatherPages = 2; // Fixed at 2 pages (Warning and Forecast)

// Initialize buttons
Bounce forwardButton = Bounce();
Bounce backButton = Bounce();
Bounce upButton = Bounce();
Bounce downButton = Bounce();

// Initialize GPS
TinyGPSPlus gps;
HardwareSerial mySerial(1); // Use Serial1 (TX = GPIO 17, RX = GPIO 16)

// Menu options
const char *menuOptions[] = {"View KMB ETA", "View GMB ETA", "View Map", "View Weather Information"};
int selectedOption = 0; // Currently selected option
bool inMenu = true; // Whether in main menu
bool inmap = false;

// Colors
#define HEADER_BG_COLOR TFT_SKYBLUE
#define HEADER_TEXT_COLOR TFT_WHITE
#define MENU_BG_COLOR TFT_WHITE
#define MENU_TEXT_COLOR TFT_BLACK
#define SELECTED_OPTION_COLOR TFT_BLUE
#define OPTION_BG_COLOR TFT_LIGHTGREY
#define OPTION_BORDER_COLOR TFT_DARKGREY

void setup() {
    // Initialize serial communication
    Serial.begin(115200);

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS initialization failed!");
        while (1) yield();
    }
    Serial.println("SPIFFS initialized.");

    // Initialize TFT display
    tft.init();
    tft.setRotation(3); // Adjust screen orientation
    tft.fillScreen(MENU_BG_COLOR);

    // Initialize buttons with pull-up resistors
    forwardButton.attach(BUTTON_FORWARD, INPUT_PULLUP);
    backButton.attach(BUTTON_BACK, INPUT_PULLUP);
    upButton.attach(BUTTON_UP, INPUT_PULLUP);
    downButton.attach(BUTTON_DOWN, INPUT_PULLUP);

    // Set debounce interval for buttons
    forwardButton.interval(25);
    backButton.interval(25);
    upButton.interval(25);
    downButton.interval(25);

    // Initialize GPS
    mySerial.begin(9600, SERIAL_8N1, 16, 17); // Start GPS serial communication

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");

    // Configure time synchronization
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    // Try to get time update
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.println("Time synchronized");
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    } else {
        Serial.println("Failed to obtain time");
    }

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);

    // Draw the main menu
    drawHeader();
    drawMenu();
}

void loop() {
    // Update button states
    forwardButton.update();
    backButton.update();
    upButton.update();
    downButton.update();

    // Current time for auto-refresh
    unsigned long currentTime = millis();

    // Read data from GPS module
    while (mySerial.available() > 0) {
        if (gps.encode(mySerial.read())) {
            // Check if we have a new valid position
            if (gps.location.isValid() && gps.location.isUpdated()) {
                latitude = gps.location.lat();
                longitude = gps.location.lng();
                Serial.printf("Location updated: Lat: %.7f, Long: %.7f\n", latitude, longitude);
                
                // If in map view, update coordinates display
                if (inmap) {
                    displayCoordinates();
                }
            }
        }
    }

    // Send location update every 60 seconds
    if (currentTime - lastLocationUpdateTime >= locationUpdateInterval) {
        sendLocationToServer();
        lastLocationUpdateTime = currentTime;
    }

    if (inMenu) {
        // Handle menu navigation
        if (upButton.fell()) {
            selectedOption = (selectedOption - 1 + 4) % 4; // Move up
            updateMenu();
        }
        if (downButton.fell()) {
            selectedOption = (selectedOption + 1) % 4; // Move down
            updateMenu();
        }
        if (forwardButton.fell()) {
            inMenu = false; // Enter selected option
            enterOption(selectedOption);
        }
    } else {
        if (inmap) {
            // Auto-refresh map every 10 seconds
            if (currentTime - lastMapUpdateTime >= mapUpdateInterval) {
                fetchAndDisplayMap(true);
                lastMapUpdateTime = currentTime;
            }
            
            // Handle zoom controls
            if (upButton.fell()) {
                currentZoomLevel++;
                if (currentZoomLevel > 20) currentZoomLevel = 20;
                updateMapZoom();
            }
            if (downButton.fell()) {
                currentZoomLevel--;
                if (currentZoomLevel < 1) currentZoomLevel = 1;
                updateMapZoom();
            }
        }

        // Handle Bus ETA paging
        if (inBusETA) {
            if (currentTime - lastBusETAUpdateTime >= busETAUpdateInterval) {
                displayRefreshingETAMessage("Refreshing Bus ETA...");
                fetchBusETA();
                lastBusETAUpdateTime = currentTime;
            }
            
            if (upButton.fell() && currentPage > 0) {
                currentPage--;
                displayBusETAPage(currentPage);
            }
            if (downButton.fell() && currentPage < totalPages - 1) {
                currentPage++;
                displayBusETAPage(currentPage);
            }
        }

        // Handle GMB ETA paging
        if (inGmbETA) {
            if (currentTime - lastGmbETAUpdateTime >= gmbETAUpdateInterval) {
                displayRefreshingETAMessage("Refreshing GMB ETA...");
                fetchGmbETA();
                lastGmbETAUpdateTime = currentTime;
            }
            
            if (upButton.fell() && gmbCurrentPage > 0) {
                gmbCurrentPage--;
                displayGmbETAPage(gmbCurrentPage);
            }
            if (downButton.fell() && gmbCurrentPage < gmbTotalPages - 1) {
                gmbCurrentPage++;
                displayGmbETAPage(gmbCurrentPage);
            }
        }

        // Handle Weather Information paging
        if (inWeather) {
            if (upButton.fell() && weatherPage > 0) {
                weatherPage--;
                displayWeatherPage(weatherPage);
            }
            if (downButton.fell() && weatherPage < totalWeatherPages - 1) {
                weatherPage++;
                displayWeatherPage(weatherPage);
            }
        }

        // Handle return to menu
        if (backButton.fell()) {
            inMenu = true;
            inmap = false;
            inBusETA = false;
            inGmbETA = false;
            inWeather = false;
            drawHeader();
            drawMenu();
        }
    }
}

// Send location data to server
void sendLocationToServer() {
    if (latitude == 0 && longitude == 0) {
        Serial.println("No valid GPS data to send");
        return;
    }

    HTTPClient http;
    String serverUrl = String(baseUrl) + "/location/track";
    
    time_t now;
    time(&now);

    StaticJsonDocument<200> jsonDoc;
    jsonDoc["time"] = (long)now;
    
    JsonArray locationArray = jsonDoc.createNestedArray("location");
    locationArray.add(longitude);
    locationArray.add(latitude);
    
    String jsonPayload;
    serializeJson(jsonDoc, jsonPayload);
    
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Location sent successfully");
        Serial.println(httpResponseCode);
        Serial.println(response);
    } else {
        Serial.println("Error sending location: " + String(httpResponseCode));
    }
    
    http.end();
}

// Display refreshing message for ETA updates
void displayRefreshingETAMessage(const char* message) {
    int boxWidth = 160;
    int boxHeight = 20;
    int boxX = tft.width() - boxWidth - 5;
    int boxY = tft.height() - boxHeight - 5;
    
    tft.fillRect(boxX, boxY, boxWidth, boxHeight, TFT_WHITE);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLUE);
    tft.setCursor(boxX + 5, boxY + 6);
    tft.print(message);
}

// Draw the header
void drawHeader() {
    tft.fillRect(0, 0, tft.width(), 50, HEADER_BG_COLOR);
    tft.setTextColor(HEADER_TEXT_COLOR, HEADER_BG_COLOR);
    tft.setTextSize(2);
    tft.drawCentreString("Solo Hiker SmartSync Insight System", tft.width() / 2, 15, 2);
}

// Draw the main menu
void drawMenu() {
    tft.fillScreen(MENU_BG_COLOR);
    drawHeader();

    for (int i = 0; i < 4; i++) {
        int y = 70 + i * 60;
        drawMenuOption(i, y);
    }
}

// Update the menu options
void updateMenu() {
    for (int i = 0; i < 4; i++) {
        int y = 70 + i * 60;
        drawMenuOption(i, y);
    }
}

// Draw a specific menu option
void drawMenuOption(int index, int y) {
    tft.fillRoundRect(20, y, tft.width() - 40, 50, 10, OPTION_BG_COLOR);
    tft.drawRoundRect(20, y, tft.width() - 40, 50, 10, OPTION_BORDER_COLOR);

    if (index == selectedOption) {
        tft.drawRoundRect(20, y, tft.width() - 40, 50, 10, SELECTED_OPTION_COLOR);
        tft.setTextColor(SELECTED_OPTION_COLOR, OPTION_BG_COLOR);
    } else {
        tft.setTextColor(MENU_TEXT_COLOR, OPTION_BG_COLOR);
    }

    tft.setTextSize(2);
    tft.drawCentreString(menuOptions[index], tft.width() / 2, y + 15, 2);
}

// Enter the selected option
void enterOption(int option) {
    tft.fillScreen(MENU_BG_COLOR);

    switch (option) {
        case 0:
            lastBusETAUpdateTime = 0;
            fetchBusETA();
            break;
        case 1:
            lastGmbETAUpdateTime = 0;
            fetchGmbETA();
            break;
        case 2:
            lastMapUpdateTime = 0;
            fetchAndDisplayMap(false);
            break;
        case 3:
            weatherPage = 0;
            fetchWeatherData();
            inWeather = true;
            inBusETA = false;
            inGmbETA = false;
            inmap = false;
            displayWeatherPage(weatherPage);
            break;
    }
}

// Fetch and display Bus ETA data
void fetchBusETA() {
    char busStops[80];
    sprintf(busStops, "/transport/kmbStops/nearest?lat=%.6f&long=%.6f", latitude, longitude);
    HTTPClient http;

    String fullUrl = String(baseUrl) + String(busStops);
    Serial.println("Fetching Bus ETA from: " + fullUrl);
    http.begin(fullUrl);
    int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        String busETAData = http.getString();
        Serial.println("Bus ETA Data: " + busETAData);

        splitBusETAIntoPages(busETAData);

        if (!inBusETA) {
            currentPage = 0;
        } else if (currentPage >= totalPages) {
            currentPage = totalPages > 0 ? totalPages - 1 : 0;
        }
        
        displayBusETAPage(currentPage);
        inBusETA = true;
        inGmbETA = false;
        lastBusETAUpdateTime = millis();
    } else {
        Serial.println("Error fetching Bus ETA");
        
        if (inBusETA) {
            int boxWidth = 160;
            int boxHeight = 20;
            int boxX = tft.width() - boxWidth - 5;
            int boxY = tft.height() - boxHeight - 5;
            
            tft.fillRect(boxX, boxY, boxWidth, boxHeight, TFT_WHITE);
            
            tft.setTextSize(1);
            tft.setTextColor(TFT_RED);
            tft.setCursor(boxX + 5, boxY + 6);
            tft.print("Error finding data");
        }
    }

    http.end();
}

// Split Bus ETA data into rows and pages
void splitBusETAIntoPages(const String &data) {
    busRows.clear();
    int startIndex = 0;
    while (startIndex < data.length()) {
        int endIndex = data.indexOf('\n', startIndex);
        if (endIndex == -1) {
            endIndex = data.length();
        }

        String row = data.substring(startIndex, endIndex);
        busRows.push_back(row);

        startIndex = endIndex + 1;
    }

    totalPages = (busRows.size() + rowsPerPage - 1) / rowsPerPage;
}

// Display a specific Bus ETA page
void displayBusETAPage(int page) {
    tft.fillScreen(TFT_WHITE);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_WHITE);
    tft.setCursor(10, 10);
    tft.print("KMB ETA");

    int screenWidth = tft.width();
    int lineHeight = 20;
    int maxWidth = screenWidth - 20;
    int y = 40;

    int startRow = page * rowsPerPage;
    int endRow = min(startRow + rowsPerPage, (int)busRows.size());

    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);

    for (int i = startRow; i < endRow; i++) {
        String row = busRows[i];
        String currentLine;

        for (int j = 0; j < row.length(); j++) {
            char c = row[j];
            currentLine += c;

            if (tft.textWidth(currentLine) > maxWidth || c == '\n') {
                tft.setCursor(10, y);
                tft.print(currentLine);
                currentLine = "";
                y += lineHeight;

                if (y > tft.height() - 60) break;
            }
        }

        if (currentLine.length() > 0) {
            tft.setCursor(10, y);
            tft.print(currentLine);
            y += lineHeight;

            if (y > tft.height() - 60) break;
        }
    }

    tft.setCursor(10, tft.height() - 20);
    tft.setTextSize(2);
    tft.print("Page ");
    tft.print(page + 1);
    tft.print(" of ");
    tft.print(totalPages);
}

// Fetch and display GMB ETA data
void fetchGmbETA() {
    char gmbStops[80];
    sprintf(gmbStops, "/transport/gmbStops/nearest?lat=%.6f&long=%.6f", latitude, longitude);
    HTTPClient http;

    String fullUrl = String(baseUrl) + String(gmbStops);
    Serial.println("Fetching GMB ETA from: " + fullUrl);
    http.begin(fullUrl);
    int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        String gmbETAData = http.getString();
        Serial.println("GMB ETA Data: " + gmbETAData);

        splitGmbETAIntoPages(gmbETAData);

        if (!inGmbETA) {
            gmbCurrentPage = 0;
        } else if (gmbCurrentPage >= gmbTotalPages) {
            gmbCurrentPage = gmbTotalPages > 0 ? gmbTotalPages - 1 : 0;
        }
        
        displayGmbETAPage(gmbCurrentPage);
        inGmbETA = true;
        inBusETA = false;
        lastGmbETAUpdateTime = millis();
    } else {
        Serial.println("Error fetching GMB ETA");
        
        if (inGmbETA) {
            int boxWidth = 160;
            int boxHeight = 20;
            int boxX = tft.width() - boxWidth - 5;
            int boxY = tft.height() - boxHeight - 5;
            
            tft.fillRect(boxX, boxY, boxWidth, boxHeight, TFT_WHITE);
            
            tft.setTextSize(1);
            tft.setTextColor(TFT_RED);
            tft.setCursor(boxX + 5, boxY + 6);
            tft.print("Error finding GMB data");
        }
    }

    http.end();
}

// Split GMB ETA data into pages
void splitGmbETAIntoPages(const String &data) {
    gmbRows.clear();
    int startIndex = 0;
    while (startIndex < data.length()) {
        int endIndex = data.indexOf('\n', startIndex);
        if (endIndex == -1) {
            endIndex = data.length();
        }

        String row = data.substring(startIndex, endIndex);
        gmbRows.push_back(row);

        startIndex = endIndex + 1;
    }

    gmbTotalPages = (gmbRows.size() + gmbrowsPerPage - 1) / gmbrowsPerPage;
}

// Display a specific GMB ETA page
void displayGmbETAPage(int page) {
    tft.fillScreen(TFT_WHITE);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_WHITE);
    tft.setCursor(10, 10);
    tft.print("GMB ETA");

    int screenWidth = tft.width();
    int lineHeight = 20;
    int y = 40;

    int startRow = page * gmbrowsPerPage;
    int endRow = min(startRow + gmbrowsPerPage, (int)gmbRows.size());

    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);

    for (int i = startRow; i < endRow; i++) {
        if (y > tft.height() - 60) break;
        
        String row = gmbRows[i];
        
        if (row.length() == 0) {
            y += lineHeight / 2;
            continue;
        }
        
        tft.setCursor(10, y);
        tft.print(row);
        y += lineHeight;
    }

    tft.setCursor(10, tft.height() - 20);
    tft.setTextSize(2);
    tft.print("Page ");
    tft.print(page + 1);
    tft.print(" of ");
    tft.print(gmbTotalPages);
}

// Update map zoom level
void updateMapZoom() {
    if (!_mapUrl.isEmpty()) {
        int zoomStartIndex = _mapUrl.indexOf("zoom=");
        if (zoomStartIndex >= 0) {
            zoomStartIndex += 5;
            int zoomEndIndex = _mapUrl.indexOf("&", zoomStartIndex);
            
            String newMapUrl = _mapUrl.substring(0, zoomStartIndex) + 
                              String(currentZoomLevel) + 
                              _mapUrl.substring(zoomEndIndex);
            
            _mapUrl = newMapUrl;
            
            if (getStaticMapImage(_mapUrl.c_str(), MAP_FILE)) {
                tft.fillScreen(TFT_WHITE);
                TJpgDec.drawFsJpg(0, 0, MAP_FILE);
                displayZoomLevel();
                displayCoordinates();
            }
        }
    }
}

// Display zoom level
void displayZoomLevel() {
    int boxWidth = 80;
    int boxHeight = 20;
    int boxX = tft.width() - boxWidth;
    int boxY = 0;
    
    tft.fillRect(boxX, boxY, boxWidth, boxHeight, TFT_LIGHTGREY);
    tft.drawRect(boxX, boxY, boxWidth, boxHeight, TFT_DARKGREY);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(boxX + 5, boxY + 6);
    tft.print("Zoom: ");
    tft.print(currentZoomLevel);
}

// Display coordinates
void displayCoordinates() {
    int panelWidth = 140;
    int panelHeight = 160;
    int panelX = tft.width() - panelWidth - 5;
    int panelY = 50;
    
    tft.fillRoundRect(panelX, panelY, panelWidth, panelHeight, 10, TFT_LIGHTGREY);
    tft.drawRoundRect(panelX, panelY, panelWidth, panelHeight, 10, TFT_DARKGREY);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLUE, TFT_LIGHTGREY);
    tft.setCursor(panelX + 15, panelY + 15);
    tft.print("Location");
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, TFT_LIGHTGREY);
    tft.setCursor(panelX + 10, panelY + 50);
    tft.print("Latitude:");
    
    char latStr[15];
    sprintf(latStr, "%.6f", latitude);
    tft.setCursor(panelX + 10, panelY + 80);
    tft.print(latStr);
    
    tft.setCursor(panelX + 10, panelY + 110);
    tft.print("Longitude:");
    
    char longStr[15];
    sprintf(longStr, "%.6f", longitude);
    tft.setCursor(panelX + 10, panelY + 140);
    tft.print(longStr);
}

bool fetchAndDisplayMap(bool keepZoom) {
    inmap = true;
    HTTPClient http;
    char map_endpoint[80];
    sprintf(map_endpoint, "/map?lat=%.6f&long=%.6f", latitude, longitude);
    String serverUrl = String(baseUrl) + map_endpoint;

    Serial.println("Fetching map URL from: " + serverUrl);
    http.begin(serverUrl);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP error when fetching map URL: %d\n", httpCode);
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.println("JSON parsing failed: " + String(error.c_str()));
        return false;
    }

    if (doc.containsKey("location") && doc["location"].is<JsonArray>()) {
        JsonArray locationArray = doc["location"].as<JsonArray>();
        if (locationArray.size() >= 2) {
            float serverLat = locationArray[0].as<float>();
            float serverLong = locationArray[1].as<float>();
            
            if (serverLat != 0 || serverLong != 0) {
                latitude = serverLat;
                longitude = serverLong;
                Serial.printf("Location updated from server: Lat: %.6f, Long: %.6f\n", latitude, longitude);
            }
        }
    }

    String mapUrl = doc["url"].as<String>();
    if (mapUrl.length() == 0) {
        Serial.println("No map URL found in response");
        return false;
    }
    
    if (keepZoom && !_mapUrl.isEmpty()) {
        int oldZoomStartIndex = _mapUrl.indexOf("zoom=");
        if (oldZoomStartIndex >= 0) {
            oldZoomStartIndex += 5;
            int oldZoomEndIndex = _mapUrl.indexOf("&", oldZoomStartIndex);
            
            if (_mapUrl.substring(oldZoomStartIndex, oldZoomEndIndex).toInt() != 0) {
                currentZoomLevel = _mapUrl.substring(oldZoomStartIndex, oldZoomEndIndex).toInt();
            }
            
            int newZoomStartIndex = mapUrl.indexOf("zoom=");
            if (newZoomStartIndex >= 0) {
                newZoomStartIndex += 5;
                int newZoomEndIndex = mapUrl.indexOf("&", newZoomStartIndex);
                
                mapUrl = mapUrl.substring(0, newZoomStartIndex) + 
                         String(currentZoomLevel) + 
                         mapUrl.substring(newZoomEndIndex);
            }
        }
    } else {
        int zoomStartIndex = mapUrl.indexOf("zoom=");
        if (zoomStartIndex >= 0) {
            zoomStartIndex += 5;
            int zoomEndIndex = mapUrl.indexOf("&", zoomStartIndex);
            currentZoomLevel = mapUrl.substring(zoomStartIndex, zoomEndIndex).toInt();
        }
    }
    
    _mapUrl = mapUrl;
    Serial.println("Map URL received: " + mapUrl);

    if (!getStaticMapImage(mapUrl.c_str(), MAP_FILE)) {
        return false;
    }

    tft.fillScreen(TFT_WHITE);
    TJpgDec.drawFsJpg(0, 0, MAP_FILE);
    
    displayZoomLevel();
    displayCoordinates();
    
    lastMapUpdateTime = millis();
    
    return true;
}

bool getStaticMapImage(const char *url, const char *fileName) {
    HTTPClient http;
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("HTTP error when fetching map image: %d\n", httpCode);
        http.end();
        return false;
    }

    File file = SPIFFS.open(fileName, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        http.end();
        return false;
    }

    http.writeToStream(&file);
    file.close();
    http.end();
    Serial.println("Map image downloaded and saved.");
    return true;
}

// Fetches weather data (report, warning, and forecast)
void fetchWeatherData() {
    const char* weatherForecast = "/weather/weather_forecast";
    const char* weatherReport = "/weather/weather_report?lat=%.6f&long=%.6f";
    const char* warningInfo = "/weather/warning_info";
    
    HTTPClient http;

    // Fetch weather report (only for "info")
    char reportUrl[100];
    sprintf(reportUrl, weatherReport, latitude, longitude);
    String fullUrl = String(baseUrl) + String(reportUrl);
    Serial.println("Fetching weather report from: " + fullUrl);
    http.begin(fullUrl);
    int httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.println("Weather Report Data: " + response);

        // Parse JSON response for "info" only
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, response);
        if (error) {
            Serial.println("Weather report JSON parsing failed: " + String(error.c_str()));
            weatherReportInfo = "Error parsing weather report";
        } else {
            weatherReportInfo = doc["info"].as<String>();
        }
    } else {
        Serial.println("Error fetching weather report: " + String(httpCode));
        weatherReportInfo = "Error fetching weather report";
    }
    http.end();

    // Fetch warning
    fullUrl = String(baseUrl) + String(warningInfo);
    Serial.println("Fetching warning information from: " + fullUrl);
    http.begin(fullUrl);
    httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        weatherWarningData = http.getString();
        Serial.println("Warning Data: " + weatherWarningData);
    } else {
        weatherWarningData = "";
        Serial.println("Error fetching warning information: " + String(httpCode));
    }
    http.end();

    // Fetch forecast
    fullUrl = String(baseUrl) + String(weatherForecast);
    Serial.println("Fetching weather forecast from: " + fullUrl);
    http.begin(fullUrl);
    httpCode = http.GET();

    if (httpCode > 0 && httpCode == HTTP_CODE_OK) {
        weatherForecastData = http.getString();
        Serial.println("Forecast Data: " + weatherForecastData);
    } else {
        weatherForecastData = "";
        Serial.println("Error fetching weather forecast: " + String(httpCode));
    }
    http.end();
}

// Displays the current weather page
void displayWeatherPage(int page) {
    tft.fillScreen(MENU_BG_COLOR);

    if (page == 0) {
        displayWeatherCombined(weatherWarningData, weatherReportInfo); // Updated to combined display
    } else {
        displayForecast(weatherForecastData);
    }

    // Show page navigation
    tft.setCursor(10, tft.height() - 20);
    tft.setTextSize(2);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    tft.print("Page ");
    tft.print(page + 1);
    tft.print(" of ");
    tft.print(totalWeatherPages);
}

// Displays warning info at top and weather report below
void displayWeatherCombined(const String &warningData, const String &reportInfo) {
    tft.fillScreen(MENU_BG_COLOR);

    // Warning info at top with prefix
    String warningFormatted = "Warning Information: " + warningData;
    warningFormatted.replace('@', ' ');
    
    tft.setTextSize(1);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);
    
    int screenWidth = tft.width();
    int lineHeight = 12;
    int topY = 10;
    int topMaxY = 100; // Limit warning to ~90px height
    
    String topLine;
    String topWord;
    
    // Word-wrap warning text
    for (int i = 0; i < warningFormatted.length(); i++) {
        char c = warningFormatted[i];
        
        if (c != ' ' && c != '\n') {
            topWord += c;
        }
        
        if (c == ' ' || c == '\n' || i == warningFormatted.length() - 1) {
            if (tft.textWidth(topLine + topWord + " ") > (screenWidth - 20)) {
                tft.setCursor(10, topY);
                tft.print(topLine);
                topY += lineHeight;
                topLine = topWord + " ";
                
                if (topY > topMaxY) break;
            } else {
                topLine += topWord + " ";
            }
            
            topWord = "";
            
            if (c == '\n') {
                tft.setCursor(10, topY);
                tft.print(topLine);
                topY += lineHeight;
                topLine = "";
                
                if (topY > topMaxY) break;
            }
        }
    }
    
    if (!topLine.isEmpty() && topY <= topMaxY) {
        tft.setCursor(10, topY);
        tft.print(topLine);
        topY += lineHeight;
    }
    
    if (!topWord.isEmpty() && topY <= topMaxY) {
        tft.setCursor(10, topY);
        tft.print(topWord);
        topY += lineHeight;
    }
    
    if (warningData.isEmpty() && topY <= topMaxY) {
        tft.setCursor(10, topY);
        tft.print("Warning Information: None");
        topY += lineHeight;
    }

    // Weather report below with increased spacing
    String reportFormatted = reportInfo;
    reportFormatted.replace('@', ' ');
    
    int reportY = topY + 60; // 20px gap for more spacing
    int reportMaxY = tft.height() - 20; // Leave space for page info
    
    String reportLine;
    String reportWord;
    
    // Word-wrap report text
    for (int i = 0; i < reportFormatted.length(); i++) {
        char c = reportFormatted[i];
        
        if (c != ' ' && c != '\n') {
            reportWord += c;
        }
        
        if (c == ' ' || c == '\n' || i == reportFormatted.length() - 1) {
            if (tft.textWidth(reportLine + reportWord + " ") > (screenWidth - 20)) {
                tft.setCursor(10, reportY);
                tft.print(reportLine);
                reportY += lineHeight;
                reportLine = reportWord + " ";
                
                if (reportY > reportMaxY) break;
            } else {
                reportLine += reportWord + " ";
            }
            
            reportWord = "";
            
            if (c == '\n') {
                tft.setCursor(10, reportY);
                tft.print(reportLine);
                reportY += lineHeight;
                reportLine = "";
                
                if (reportY > reportMaxY) break;
            }
        }
    }
    
    if (!reportLine.isEmpty() && reportY <= reportMaxY) {
        tft.setCursor(10, reportY);
        tft.print(reportLine);
        reportY += lineHeight;
    }
    
    if (!reportWord.isEmpty() && reportY <= reportMaxY) {
        tft.setCursor(10, reportY);
        tft.print(reportWord);
        reportY += lineHeight;
    }
    
    if (reportInfo.isEmpty() && reportY <= reportMaxY) {
        tft.setCursor(10, reportY);
        tft.print("Report: None");
    }
}

// Displays weather forecast with word wrapping
void displayForecast(const String &data) {
    tft.fillScreen(MENU_BG_COLOR);

    String formattedData = data;
    formattedData.replace('@', ' ');

    tft.setTextSize(1);
    tft.setTextColor(MENU_TEXT_COLOR, MENU_BG_COLOR);

    int screenWidth = tft.width();
    int lineHeight = 12;
    int currentY = 10;
    int maxY = tft.height() - lineHeight - 20;

    String currentLine;
    String currentWord;

    // Process text with word wrapping
    for (int i = 0; i < formattedData.length(); i++) {
        char c = formattedData[i];

        if (c != ' ' && c != '\n') {
            currentWord += c;
        }

        if (c == ' ' || c == '\n' || i == formattedData.length() - 1) {
            if (tft.textWidth(currentLine + currentWord + " ") > (screenWidth - 20)) {
                tft.setCursor(10, currentY);
                tft.print(currentLine);
                currentY += lineHeight;
                currentLine = currentWord + " ";

                if (currentY > maxY) break;
            } else {
                currentLine += currentWord + " ";
            }

            currentWord = "";

            if (c == '\n') {
                tft.setCursor(10, currentY);
                tft.print(currentLine);
                currentY += lineHeight;
                currentLine = "";

                if (currentY > maxY) break;
            }
        }
    }

    if (!currentLine.isEmpty() && currentY <= maxY) {
        tft.setCursor(10, currentY);
        tft.print(currentLine);
        currentY += lineHeight;
    }

    if (!currentWord.isEmpty() && currentY <= maxY) {
        tft.setCursor(10, currentY);
        tft.print(currentWord);
    }

    if (data.isEmpty() && currentY <= maxY) {
        tft.setCursor(10, currentY);
        tft.print("Forecast Information: Error");
    }
}