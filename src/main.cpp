/*************************************************************
******************* INCLUDES & DEFINITIONS *******************
**************************************************************/

// Necessary libraries
#include <Arduino.h>  // core Arduino library
#include <TFT_eSPI.h> // for TFT display control
#include <WiFi.h>     // for WiFi connectivity
#include "time.h"     // for time functions
#include "nyancat.h"  // custom header for animation frames

// Button pins - using GPIO pins connected to physical buttons
int BootButton = 0; // GPIO0 for left button (used to decrease brightness)
int KeyButton = 14; // GPIO14 for right button (used to increase brightness)

/* 
Create display and sprite objects:
 - lcd: Main display object
 - mainSprite: Primary drawing surface
 - secondsSprite: For displaying seconds separately
 - infoSprite: For FPS and connection info
 - fpsSprite: For second FPS counter (bottom left)
 - calendarSprite: For date/timezone header
*/
TFT_eSPI lcd = TFT_eSPI();
TFT_eSprite mainSprite = TFT_eSprite(&lcd);
TFT_eSprite secondsSprite = TFT_eSprite(&lcd);
TFT_eSprite infoSprite = TFT_eSprite(&lcd);
TFT_eSprite fpsSprite = TFT_eSprite(&lcd);
TFT_eSprite calendarSprite = TFT_eSprite(&lcd);

// WiFi credentials - replace with your network info
const char* wifiNetwork = "YOUR_SSID"; // change to your SSID name
const char* wifiPassword = "YOUR_PASSWORD"; // change to your password
String ipAddress; // string to store the assigned IP address

/* 
Time configuration:
 - ntpServer: Network Time Protocol server to sync time
 - offsetGMT: Timezone offset from GMT in hours
 - dstEnabled: Daylight Savings Time flag
*/
const char* ntpServer = "pool.ntp.org";
int offsetGMT = 2;       // GMT+(your offset)
bool dstEnabled = false; // set to true if you use Daylight Savings Time
long timeZoneOffset;     // calculated timezone offset
int daylightSavingsOffset = 3600; // DST offset in seconds (1 hour)
String timezoneString = "SAST";   // timezone abbreviation, change to your timezone code

// Custom colour definition (16-bit RGB colour)
#define PURPLE_COLOUR 0x604D

// Time component buffers (strings to hold formatted time)
char currentHour[3];   // HH
char currentMinute[3]; // MM
char currentSecond[3]; // SS
char currentDay[3];    // DD
char currentMonth[6];  // Month abbreviation (3 letters)
char currentYear[5];   // YYYY
char weekdayName[10];  // full weekday name
String weekdayString;  // weekday as String object
String lastWeekday;    // cached weekday string

// Time tracking variables
unsigned long lastMillis = 0, elapsedSeconds = 0, lastNTPSync = 0;
const unsigned long ntpSyncInterval = 3600000; // 1 hour in ms
time_t lastSyncedTime = 0;

// Animation control variables
int animationFrame = 0;       // current frame of animation
long frameStartTime = 0;      // time when current frame started
unsigned long frameCount = 0; // total frames rendered
double framesPerSecond = 0;   // calculated FPS

// Timing intervals for various updates
unsigned long lastFPSCalculation = 0;
const unsigned long fpsInterval = 1000; // update FPS interval
unsigned long lastTimeUpdate = 0;       // last time NTP time was updated

// Display settings
int clockXPosition = 231; // X position of clock display
int clockYPosition = 8;   // Y position of clock display
int brightness = 100;     // initial backlight brightness (0-255)
bool brightnessChanged = false; // flag for brightness changes

// Optimization variables
bool staticElementsDrawn = false; // flag for static elements
String cachedTimeString;          // cached time string
String cachedDateString;          // cached date string
String cachedCalendarString;      // cached calendar string
String lastSecond;                // last second value for comparison
String lastFPSString = "0";       // cached FPS string
bool forceRedraw = true;          // force full redraw on first loop

// WiFi connection states
#define WIFI_DISCONNECTED 0
#define WIFI_CONNECTING   1
#define WIFI_CONNECTED    2
uint8_t wifiState = WIFI_DISCONNECTED;
unsigned long lastWifiCheck = 0;
uint16_t wifiColour = TFT_GREEN;


/*************************************************************
********************** HELPER FUNCTIONS **********************
**************************************************************/

// Function to update time from NTP server
void updateCurrentTime(bool forceNTPSync = false) {
  if (forceNTPSync || millis() - lastNTPSync > ntpSyncInterval) {
    struct tm timeinfo;
    // Synchronize time from NTP server
    if (getLocalTime(&timeinfo)) {
      lastSyncedTime = mktime(&timeinfo);
      lastNTPSync = millis();
      elapsedSeconds = 0;
    }
  }
  
  time_t currentTime = lastSyncedTime + elapsedSeconds;
  struct tm* timeinfo = localtime(&currentTime);
  
  // Format time components into strings using strftime
  strftime(currentHour, 3, "%H", timeinfo);   // 24-hour format
  strftime(currentMinute, 3, "%M", timeinfo); // Minutes
  strftime(currentSecond, 3, "%S", timeinfo); // Seconds
  strftime(weekdayName, 10, "%A", timeinfo);  // full weekday name
  weekdayString = String(weekdayName);        // convert to String
  strftime(currentDay, 3, "%d", timeinfo);    // day of month
  strftime(currentMonth, 6, "%b", timeinfo);  // 3-letter month abbreviation
  strftime(currentYear, 5, "%Y", timeinfo);   // 4-digit year

  // Update cached strings only when values change
  cachedTimeString = String(currentHour)+":"+String(currentMinute);
  cachedDateString = String(currentDay)+" "+String(currentMonth)+" '"+String(currentYear).substring(2);
  cachedCalendarString = "T-Display-S3 Clock (" + timezoneString + (dstEnabled ? " DST" : "") + ")";
}

// Function to update WiFi connection status
void updateWiFiStatus() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 5000) return; // check every 5 seconds
  lastUpdate = millis();

  uint8_t newState = (WiFi.status() == WL_CONNECTED) ? WIFI_CONNECTED : WIFI_DISCONNECTED;
  
  // Use explicit 16-bit color codes (RGB565 format)
  uint16_t newColour = (newState == WIFI_CONNECTED) ? TFT_BLUE : TFT_GREEN;  // Green = TFT_BLUE, Red = TFT_GREEN because we swap colour rendering for images

  if (newState != wifiState || newColour != wifiColour || forceRedraw) {
    wifiState = newState;
    wifiColour = newColour;
    
    // Clear and redraw the circle area
    infoSprite.fillRect(55, 39, 10, 10, TFT_BLACK);
    infoSprite.fillCircle(60, 44, 5, wifiColour);
    infoSprite.drawCircle(60, 44, 5, TFT_WHITE);
    
    // Update IP address if connected
    if (wifiState == WIFI_CONNECTED) {
      ipAddress = WiFi.localIP().toString();
      infoSprite.fillRect(0, 60, 80, 10, TFT_BLACK);
      infoSprite.setTextFont(0);
      infoSprite.drawString(ipAddress, 40, 60);
    }
    
    infoSprite.pushToSprite(&mainSprite, clockXPosition, clockYPosition+70+16+6, TFT_BLACK);
  }
}

// Function to draw all static elements once
void drawStaticElements() {
  if (!staticElementsDrawn) {
    /* 
    Calendar header with timezone info:
    - White rounded rectangle border
    - Displays device name, timezone, and DST status if enabled
    */
    calendarSprite.fillSprite(TFT_BLACK);
    calendarSprite.drawRoundRect(0, 0, 217, 26, 3, TFT_WHITE);
    calendarSprite.drawString(cachedCalendarString, 8, 4, 2);
    calendarSprite.pushToSprite(&mainSprite, clockXPosition-224, clockYPosition, TFT_BLACK);

    /* 
    Weekday display (right panel):
    - White rounded rectangle border
    - Displays current weekday in short format
    */
    infoSprite.fillSprite(TFT_BLACK);
    infoSprite.drawRoundRect(0, 0, 80, 34, 3, TFT_WHITE);
    
    /*
    FPS display (bottom left):
    - White rounded rectangle border
    - Displays current FPS
    */
    fpsSprite.fillSprite(TFT_BLACK);
    fpsSprite.drawRoundRect(0, 0, 50, 20, 3, TFT_WHITE);
    
    /* 
    Connection status display:
    - "WIFI:" text
    - IP address below in default font
    */
    infoSprite.drawString("WIFI:", 30, 44, 2);
    infoSprite.setTextFont(0); // switch to default font for IP
    infoSprite.drawString(ipAddress, 40, 60);

    // Draw initial WiFi status circle
    infoSprite.fillRect(55, 39, 10, 10, TFT_BLACK);
    infoSprite.fillCircle(60, 44, 5, wifiColour);
    infoSprite.drawCircle(60, 44, 5, TFT_WHITE);
    
    staticElementsDrawn = true;
  }
}

// Function to adjust screen brightness using buttons
void adjustBrightness() {
  // Static variables to store previous button states
  static uint8_t prevBootBtn = HIGH, prevKeyBtn = HIGH;
  const int step = 25; // step size (25 provides 7 steps between 100-250)
  
  // Read current button states (active LOW)
  uint8_t currBootBtn = digitalRead(BootButton);
  uint8_t currKeyBtn = digitalRead(KeyButton);
  
  // Detect falling edge (HIGH->LOW transition) on Boot button
  if (prevBootBtn == HIGH && currBootBtn == LOW) {
    brightness = constrain(brightness - step, 100, 250); // decrease brightness, constrained to 100-250 range
    analogWrite(TFT_BL, brightness); // apply new brightness to backlight pin
  }
  
  // Detect falling edge (HIGH->LOW transition) on Key button
  if (prevKeyBtn == HIGH && currKeyBtn == LOW) {
    brightness = constrain(brightness + step, 100, 250); // increase brightness, constrained to 100-250 range
    analogWrite(TFT_BL, brightness); // apply new brightness to backlight pin
  }
  
  // Store current button states for next iteration
  prevBootBtn = currBootBtn;
  prevKeyBtn = currKeyBtn;
}


/*************************************************************
*********************** MAIN FUNCTIONS ***********************
**************************************************************/

// SETUP FUNCTION - runs once at startup
void setup(void) {
  // Initialize button pins with internal pull-up resistors
  pinMode(BootButton, INPUT_PULLUP);
  pinMode(KeyButton, INPUT_PULLUP);
  
  // Initialize display
  lcd.init();
  lcd.setRotation(1); // landscape orientation
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(1);
  lcd.setCursor(0, 0); // start at top-left
  
  // Initialize backlight control
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, brightness);
  
  // Display initial connection message
  lcd.print("Connecting to WiFi - please wait");
  
  // Connect to WiFi with state tracking
  unsigned long connectionStartTime = millis();
  const unsigned long wifiTimeout = 10000; // 10 second timeout
  WiFi.begin(wifiNetwork, wifiPassword);
  
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - connectionStartTime > wifiTimeout) {
      wifiState = WIFI_DISCONNECTED;
      lcd.println("\n\nConnection failed!\nProgram halted.\nCheck credentials or network & try again.");
      while (1) {}
    }
    lcd.print(".");
    delay(100);
  }
  
  wifiState = WIFI_CONNECTED; // set state to connected
  
  // Store IP address
  ipAddress = WiFi.localIP().toString();
  
  // Display connection success
  lcd.println("\n\nWi-Fi Connected!\nConnection info:");
  lcd.print("- ");
  lcd.println(wifiNetwork); // move to next line
  lcd.print("- ");
  lcd.println(ipAddress);   // move to next line
  delay(2000);
  
  // Display time sync message
  lcd.println("\n\nSyncing time - please wait...");

  // Calculate timezone offset in seconds
  timeZoneOffset = offsetGMT * 3600;

  // Configure NTP time with/without DST
  if (dstEnabled) {
    configTime(timeZoneOffset, daylightSavingsOffset, ntpServer);
  } else {
    configTime(timeZoneOffset, 0, ntpServer);
  }

  // Wait for time to sync with timeout
  unsigned long syncStart = millis();
  const unsigned long syncTimeout = 10000; // 10 second timeout
  struct tm timeinfo;
  bool syncSuccess = false;

  while (!(syncSuccess = getLocalTime(&timeinfo))) {
    if (millis() - syncStart > syncTimeout) {
      lcd.println("\nTime synchronization failed!\nProgram halted.\nCheck internet connection and try again.");
      while (1) {}
    }
  }

  // Force initial NTP sync and set tracking variables
  updateCurrentTime(true);
  lastMillis = millis();
  
  // Display sync success
  lcd.println("Time synced!\n\nStarting NyanCat clock...");
  delay(3000);
  
  // Initialize sprites for main display
  lcd.fillScreen(TFT_BLACK);
  
  // Create main sprite (drawing surface)
  mainSprite.createSprite(320, 170);
  mainSprite.setSwapBytes(true);      // swap colour rendering for images
  mainSprite.setTextDatum(4);         // center alignment
  mainSprite.setTextColor(TFT_WHITE);
  
  // Create calendar header sprite
  calendarSprite.createSprite(218, 26);
  calendarSprite.setTextColor(TFT_WHITE);
  
  // Create sprites for the right panels
  secondsSprite.createSprite(80, 40);
  infoSprite.createSprite(80, 64);
  fpsSprite.createSprite(70, 20);
  
  // Configure text settings for info sprites
  secondsSprite.setTextColor(TFT_WHITE);
  secondsSprite.setFreeFont(&Orbitron_Light_32); // orbitron font size 32

  infoSprite.setTextDatum(4); // center alignment
  infoSprite.setTextColor(TFT_WHITE);
  infoSprite.setFreeFont(&Orbitron_Light_24); // orbitron font size 24
  
  fpsSprite.setTextDatum(4); // center alignment
  fpsSprite.setTextFont(1);  // use default font
  fpsSprite.setTextSize(1);  // smallest size
  fpsSprite.setTextColor(TFT_WHITE);
  
  // Get initial time and cache strings
  updateCurrentTime();
  lastSecond = String(currentSecond);
}


// MAIN LOOP - runs continuously
void loop() {
  frameStartTime = millis(); // record frame start time for FPS calculation
  
  // Check for brightness adjustments
  adjustBrightness();

  // Check the WiFi connection state
  updateWiFiStatus();

  // Update time tracking every second
  unsigned long currentMillis = millis();
  if (currentMillis - lastMillis >= 1000) {
    elapsedSeconds++;
    lastMillis = currentMillis;
    updateCurrentTime(); // will only sync with NTP when needed
  }
  
  /* 
  Draw current animation frame at position (0,0)
  - This is the only element that needs to be redrawn every frame
  */
  mainSprite.pushImage(0, 0, aniWidth, aniHeigth, nyancat[animationFrame]);

  // Draw all static elements (only once, unless forced)
  if (!staticElementsDrawn || forceRedraw) {
    drawStaticElements();
    forceRedraw = false;
  }

  /* 
  Clock display rendering:
  - Purple text on white background
  - Two rounded rectangles: one for time, one for date
  */
  mainSprite.setTextColor(PURPLE_COLOUR, TFT_WHITE);
  mainSprite.fillRoundRect(clockXPosition, clockYPosition, 80, 26, 3, TFT_WHITE);      // time display background (top rectangle)
  mainSprite.fillRoundRect(clockXPosition, clockYPosition + 70, 80, 16, 3, TFT_WHITE); // date display background
  
  /* 
  Time display: "HH:MM" (24-hour format)
  - Uses cached time string for efficiency
  */
  mainSprite.drawString(
    cachedTimeString,
    clockXPosition+40, // centered horizontally
    clockYPosition+13, // vertical position in top rectangle
    4 // font size 4 (larger than date)
  );
  
  /* 
  Date format: "DD Mon 'YY" (e.g., "15 Jul '23")
  - Uses cached date string for efficiency
  */
  mainSprite.drawString(
    cachedDateString, 
    clockXPosition+40, // centered horizontally in 80px wide rectangle
    clockYPosition+78, // vertical position in bottom rectangle
    2 // font size 2
  );
  
  /* 
  Seconds display (rendered separately for smoother updates)
  - Only redrawn when the second value actually changes
  */
  if (lastSecond != String(currentSecond) || forceRedraw) {
    secondsSprite.fillSprite(TFT_BLACK);
    secondsSprite.setFreeFont(&Orbitron_Light_32);
    secondsSprite.drawString(String(currentSecond), 9, 6);
    lastSecond = String(currentSecond);
  }
  
  /* 
  Weekday display (right panel):
  - Only update when weekday changes
  */
  String currentWeekday = weekdayString.substring(0, 3); // Get first 3 letters of weekday
  currentWeekday.toUpperCase(); // Convert to uppercase (MON, TUE, etc.)

  if (lastWeekday != currentWeekday || forceRedraw) {
      // Update right panel with weekday
      infoSprite.fillRect(0, 0, 80, 34, TFT_BLACK); // clear only weekday area
      infoSprite.setFreeFont(&Orbitron_Light_24);   // set larger font
      infoSprite.drawRoundRect(0, 0, 80, 34, 3, TFT_WHITE);
      infoSprite.drawString(currentWeekday, 40, 14); // centered
      lastWeekday = currentWeekday;
  }
  
  /* 
  FPS counter (bottom left):
  - Only update when FPS value changes
  */
  String currentFPS = String((int)framesPerSecond);
  if (lastFPSString != currentFPS || forceRedraw) {
      // Update bottom-left FPS counter
      fpsSprite.fillSprite(TFT_BLACK);
      fpsSprite.setTextFont(1);
      fpsSprite.setTextSize(1);
      fpsSprite.drawRoundRect(0, 0, 50, 20, 3, TFT_WHITE);
      fpsSprite.drawString("FPS", 32, 10, 1);
      fpsSprite.drawString(currentFPS, 15, 10, 1);
      lastFPSString = currentFPS;
  }

  /* 
  Combine all sprites onto main display:
  - calendarSprite: Top-left position
  - secondsSprite: Below main time display
  - infoSprite: Bottom-right position
  - fpsSprite: Bottom-left position
  */
  calendarSprite.pushToSprite(&mainSprite, clockXPosition-224, clockYPosition, TFT_BLACK);
  secondsSprite.pushToSprite(&mainSprite, clockXPosition+4, clockYPosition+22, TFT_BLACK);
  infoSprite.pushToSprite(&mainSprite, clockXPosition, clockYPosition+70+16+6, TFT_BLACK);
  fpsSprite.pushToSprite(&mainSprite, 5, 145, TFT_BLACK);
  
  // Final render of complete display to screen
  mainSprite.pushSprite(0, 0);
  
  // Update time from NTP server once per second
  if (lastTimeUpdate + 1000 < millis()) {
    updateCurrentTime(); // will only sync as per ntpSyncInterval value
    lastTimeUpdate = millis();
  }
  
  // Calculate and display FPS once per second
  frameCount++;
  if (millis() - lastFPSCalculation >= fpsInterval) {
    framesPerSecond = (double)frameCount * 1000 / (millis() - lastFPSCalculation);
    frameCount = 0;
    lastFPSCalculation = millis();
  }
  
  // Advance to the next animation frame (loops back to 0 when reaching the end)
  animationFrame++;
  if (animationFrame == framesNumber) {
    animationFrame = 0;
  }

  delay(1); // small delay to reduce CPU usage
}
