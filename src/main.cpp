// Include necessary libraries
#include <Arduino.h>  // core Arduino library
#include <TFT_eSPI.h> // for TFT display control
#include <WiFi.h>     // for WiFi connectivity
#include "time.h"     // for time functions
#include "nyancat.h"  // custom header for animation frames

// Button pins - using GPIO pins connected to physical buttons
int leftButtonPin = 0;   // GPIO0 for left button (used to decrease brightness)
int rightButtonPin = 14; // GPIO14 for right button (used to increase brightness)

/* 
Create display and sprite objects:
 - lcd: Main display object
 - mainSprite: Primary drawing surface
 - secondsSprite: For displaying seconds separately
 - infoSprite: For FPS and connection info
 - calendarSprite: For date/timezone header
*/
TFT_eSPI lcd = TFT_eSPI();
TFT_eSprite mainSprite = TFT_eSprite(&lcd);
TFT_eSprite secondsSprite = TFT_eSprite(&lcd);
TFT_eSprite infoSprite = TFT_eSprite(&lcd);
TFT_eSprite calendarSprite = TFT_eSprite(&lcd);

// WiFi credentials - replace with your network info
const char* wifiNetwork = "TP-LINK 2.5GHz";
const char* wifiPassword = "Uitenhage1";
String ipAddress; // to store device's assigned IP address

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

// Custom color definition (16-bit RGB color)
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

// Animation control variables
int animationFrame = 0;       // current frame of animation
long frameStartTime = 0;      // time when current frame started
unsigned long frameCount = 0; // total frames rendered
double framesPerSecond = 0;   // calculated FPS

// Timing intervals for various updates
unsigned long lastFPSCalculation = 0;
const unsigned long fpsInterval = 1000; // update FPS every second
unsigned long lastTimeUpdate = 0;       // last time NTP time was updated

// Display settings
int clockXPosition = 231; // X position of clock display
int clockYPosition = 8;   // Y position of clock display
int brightness = 100;     // initial backlight brightness (0-255)
bool brightnessChanged = false; // flag for brightness changes


// Function to update time from NTP server
void updateCurrentTime() {
  struct tm timeinfo; // tm struct holds broken-down time
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    return;
  }
  
  // Format time components into strings using strftime
  strftime(currentHour, 3, "%H", &timeinfo);   // 24-hour format
  strftime(currentMinute, 3, "%M", &timeinfo); // Minutes
  strftime(currentSecond, 3, "%S", &timeinfo); // Seconds
  strftime(weekdayName, 10, "%A", &timeinfo);  // full weekday name
  weekdayString = String(weekdayName);         // convert to String
  strftime(currentDay, 3, "%d", &timeinfo);    // day of month
  strftime(currentMonth, 6, "%b", &timeinfo);  // 3-letter month abbreviation
  strftime(currentYear, 5, "%Y", &timeinfo);   // 4-digit year
}

// Function to adjust screen brightness using buttons
void adjustBrightness() {
  static unsigned long lastButtonPress = 0; // for debouncing
  const int debounceDelay = 20;             // milliseconds to wait between presses
  const int brightnessStep = 50;            // how much to change brightness each press
  
  // Left button decreases brightness
  if(digitalRead(leftButtonPin) == 0 && millis() - lastButtonPress > debounceDelay) {
    brightness = max(50, brightness - brightnessStep); // don't go below 50
    brightnessChanged = true;
    lastButtonPress = millis();
  }
  
  // Right button increases brightness
  if(digitalRead(rightButtonPin) == 0 && millis() - lastButtonPress > debounceDelay) {
    brightness = min(250, brightness + brightnessStep); // don't exceed 250
    brightnessChanged = true;
    lastButtonPress = millis();
  }
  
  // Apply brightness change if needed
  if(brightnessChanged) {
    analogWrite(TFT_BL, brightness); // TFT_BL is backlight control pin
    brightnessChanged = false;
  }
}


// SETUP FUNCTION - runs once at startup
void setup(void) {
  // Initialize button pins with internal pull-up resistors
  pinMode(leftButtonPin, INPUT_PULLUP);
  pinMode(rightButtonPin, INPUT_PULLUP);
  
  // Initialize display
  lcd.init();
  lcd.setRotation(1); // landscape orientation
  
  // Initialize backlight control
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, brightness);
  
  // Create main sprite (drawing surface)
  mainSprite.createSprite(320, 170);
  mainSprite.setSwapBytes(true);              // for proper color rendering
  mainSprite.setTextColor(TFT_WHITE, 0xEAA9); // white text with custom bg
  mainSprite.setTextDatum(4);                 // center alignment
  
  // Create calendar header sprite
  calendarSprite.createSprite(218, 26);
  calendarSprite.fillSprite(TFT_GREEN);
  calendarSprite.setTextColor(TFT_WHITE, TFT_GREEN);
  
  // Create sprites for seconds display and info panel
  secondsSprite.createSprite(80, 40);
  secondsSprite.fillSprite(TFT_GREEN);
  infoSprite.createSprite(80, 64);
  infoSprite.fillSprite(TFT_GREEN);
  
  // Configure text settings for info sprites
  infoSprite.setTextDatum(4); // center alignment
  secondsSprite.setTextColor(TFT_WHITE, TFT_GREEN);
  infoSprite.setTextColor(TFT_WHITE, TFT_GREEN);
  secondsSprite.setFreeFont(&Orbitron_Light_32); // special font
  infoSprite.setFreeFont(&Orbitron_Light_24);
  
  // Calculate timezone offset in seconds
  timeZoneOffset = offsetGMT * 3600;
  
  // Connect to WiFi
  WiFi.begin(wifiNetwork, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  // Store IP address
  ipAddress = WiFi.localIP().toString();
  
  // Configure NTP time with/without DST
  if (dstEnabled) {
    configTime(timeZoneOffset, daylightSavingsOffset, ntpServer);
  }
  else {
    configTime(timeZoneOffset, 0, ntpServer);
  }
}


// MAIN LOOP - runs continuously
void loop() {
  frameStartTime = millis(); // record frame start time for FPS calculation
  
  // Check for brightness adjustments
  adjustBrightness();
  
  // Clear secondary sprites
  secondsSprite.fillSprite(TFT_GREEN);
  infoSprite.fillSprite(TFT_GREEN);
  infoSprite.setFreeFont(&Orbitron_Light_24);
  
  // Draw current animation frame
  mainSprite.pushImage(0, 0, aniWidth, aniHeigth, logo2[animationFrame]);
  
  // Draw clock face with white rounded rectangle background
  mainSprite.setTextColor(PURPLE_COLOUR, TFT_WHITE);
  mainSprite.fillRoundRect(clockXPosition, clockYPosition, 80, 26, 3, TFT_WHITE);
  mainSprite.fillRoundRect(clockXPosition, clockYPosition+70, 80, 16, 3, TFT_WHITE);
  
  // Draw formatted date: "DD Mon 'YY"
  mainSprite.drawString(String(currentDay)+" "+String(currentMonth)+" '"+String(currentYear).substring(2), 
                       clockXPosition+40, clockYPosition+78, 2);
  
  // Draw time: "HH:MM"
  mainSprite.drawString(String(currentHour)+":"+String(currentMinute), 
                       clockXPosition+40, clockYPosition+13, 4);
  
  // Draw seconds in separate sprite
  secondsSprite.drawString(String(currentSecond), 9, 6);
  
  // Draw FPS counter with border
  infoSprite.drawRoundRect(0, 0, 80, 34, 3, TFT_WHITE);
  infoSprite.drawString("FPS", 62, 14, 2);
  infoSprite.drawString(String((int)framesPerSecond), 26, 14);
  
  // Draw connection status
  infoSprite.drawString("CONNECTED", 40, 44, 2);
  infoSprite.setTextFont(0); // default font for IP
  infoSprite.drawString(ipAddress, 40, 60);
  
  // Draw calendar header with timezone info
  calendarSprite.drawRoundRect(0, 0, 217, 26, 3, TFT_WHITE);
  calendarSprite.drawString("T-Display-S3 Clock (" + timezoneString + 
                           (dstEnabled ? " DST" : "") + ")", 8, 4, 2);
  
  // Combine all sprites onto main display
  calendarSprite.pushToSprite(&mainSprite, clockXPosition-224, clockYPosition, TFT_GREEN);
  secondsSprite.pushToSprite(&mainSprite, clockXPosition+4, clockYPosition+22, TFT_GREEN);
  infoSprite.pushToSprite(&mainSprite, clockXPosition, clockYPosition+70+16+6, TFT_GREEN);
  mainSprite.pushSprite(0, 0); // final render to screen
  
  // Update time once per second (not every frame)
  if(lastTimeUpdate + 1000 < millis()) {
    updateCurrentTime();
    lastTimeUpdate = millis();
  }
  
  // Calculate FPS once per second
  frameCount++;
  if(millis() - lastFPSCalculation >= fpsInterval) {
    framesPerSecond = (double)frameCount * 1000 / (millis() - lastFPSCalculation);
    frameCount = 0;
    lastFPSCalculation = millis();
  }
  
  // Advance animation frame (loops back to 0 when reaching end)
  animationFrame++;
  if(animationFrame == framesNumber) {
    animationFrame = 0;
  }
}
