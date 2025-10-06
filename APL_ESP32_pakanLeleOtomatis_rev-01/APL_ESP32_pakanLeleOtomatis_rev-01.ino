#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include <ESP32Servo.h>
#include <vector>
#include <EEPROM.h>

// ====== WIFI MODE SELECTION ======
// Uncomment ONLY ONE of the following lines:

// #define WIFI_MODE_STATION     // ESP32 connects to external Wi-Fi (e.g., router / ESP-01)
#define WIFI_MODE_AP        // ESP32 creates its own Wi-Fi Access Point

// ============= CONFIG WIFI =================
char ssid[32] = "seen";
char password[64] = "qweasdzxc123";

const char* ap_ssid = "FishFeeder_AP";
const char* ap_password = "12345678";

const int wifiapMode = 34;

// Web server at port 80
WebServer server(80);

// RTC
RTC_DS3231 rtc;
bool rtcConnected = false;
bool useSoftwareClock = false;
unsigned long softwareClockMillis = 0;
DateTime softwareTime;

// Continuous Rotation Servo
Servo feederServo;
const int servoPin = 13;
bool servoAttached = false;

// Motor - Using PWM for speed control
const int motorPin = 12;
const int motorPWMFreq = 5000;
const int motorPWMResolution = 8;
bool motorPWMInitialized = false;

// Level sensors - UPDATED levelA pin
const int levelA = 39;  // Changed from 33 to 39 - Feeder basket sensor
const int levelB = 32;
const int levelC = 25;

// Ultrasonic - Water Surface Monitoring
const int trigPin = 15;
const int echoPin = 2;
bool ultrasonicWorking = false;
float waterStabilityThreshold = 2.0; // cm - default threshold for detecting movement
unsigned long movementTimeout = 30000; // 30 seconds default - NOW CONFIGURABLE
float lastWaterDistance = 0;
bool fishMoving = false;
unsigned long lastMovementTime = 0;

// LED Indicators
const int WebAvailable = 17;
const int RTCAvailable = 16;
const int FishAvailable = 4;

// Feeding settings
float gramsPerSecond = 10.0;
float feedAmountGrams = 30.0;

// Motor speed settings
int motorSpeed = 200;
int minMotorSpeed = 100;
int maxMotorSpeed = 255;

// Continuous Servo settings
int servoStop = 90;
int servoSpeed = 30;
int minServoSpeed = 10;
int maxServoSpeed = 40;
bool clockwiseRotation = true;

// Dynamic feeding schedule
struct FeedingTime {
  int hour;
  int minute;
  bool enabled;
  bool alreadyFed;
};

std::vector<FeedingTime> feedingSchedule;

// Feed History Tracking
struct FeedHistory {
  DateTime timestamp;
  float amount;
  bool success;
  String type; // "manual", "scheduled", "auto"
};

std::vector<FeedHistory> feedHistory;
const int MAX_HISTORY_RECORDS = 50;

// EEPROM Settings
#define EEPROM_SIZE 2048
#define SETTINGS_START 0
#define SCHEDULE_START 100
#define WIFI_SETTINGS_START 500
#define ULTRASONIC_SETTINGS_START 600

struct Settings {
  float gramsPerSecond;
  float feedAmountGrams;
  int motorSpeed;
  int servoSpeed;
  bool clockwiseRotation;
  float waterStabilityThreshold;
  unsigned long movementTimeout;
  uint32_t checksum;
};

struct WiFiSettings {
  char ssid[32];
  char password[64];
  uint32_t checksum;
};

// Function declarations
DateTime getCurrentTime();
void updateSoftwareClock();
void saveSettings();
void loadSettings();
void saveWiFiSettings();
void loadWiFiSettings();
void addFeedingTime(int hour, int minute, bool enabled = true);
void removeFeedingTime(int index);
String getFeedingStatus();
float calculateFeedDuration(float grams);
void updateLEDs();
void blinkLED(int pin, int times, int delayTime);
void setMotorSpeed(int speed);
void setupMotorPWM();
void setupServo();
void setServoSpeed(int speed, bool clockwise = true);
void stopServo();
void stopAllMotors();
uint32_t calculateChecksum(uint8_t* data, size_t len);
void saveScheduleToEEPROM();
void loadScheduleFromEEPROM();
void connectToWiFi();
void startAPMode();
void feedFish(float grams);
void feedFishWithSafety(float grams, String feedType = "manual");
long readUltrasonic();
void monitorWaterSurface();
void checkUltrasonicStatus();
void systemHealthCheck();
void logFeeding(float grams, bool success, String type = "manual");
String getFeedHistoryHTML();
bool isFeederBasketEmpty();
void handleSetTime(); // NEW: Browser time handler

// Web handler declarations
void handleRoot();
void handleFeed();
void handleTime();
void handleSettings();
void handleSchedule();
void handleAddTime();
void handleUpdate();
void handleDeleteTime();
void handleToggleTime();
void handleWiFiConfig();
void handleWiFiUpdate();
void handleHistory();

void setup() {
  Serial.begin(115200);
  Wire.begin();

  EEPROM.begin(EEPROM_SIZE);
  Serial.println("EEPROM initialized");

  pinMode(WebAvailable, OUTPUT);
  pinMode(RTCAvailable, OUTPUT);
  pinMode(FishAvailable, OUTPUT);
  
  digitalWrite(WebAvailable, LOW);
  digitalWrite(RTCAvailable, LOW);
  digitalWrite(FishAvailable, LOW);

  stopAllMotors();

  loadSettings();
  loadWiFiSettings();
  loadScheduleFromEEPROM();

  if (feedingSchedule.empty()) {
    addFeedingTime(8, 0);
    addFeedingTime(16, 0);
    addFeedingTime(20, 0);
    saveScheduleToEEPROM();
  }

  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("WARNING: Couldn't find RTC!");
    rtcConnected = false;
    useSoftwareClock = true;
    softwareTime = DateTime(F(__DATE__), F(__TIME__));
    softwareClockMillis = millis();
  } else {
    rtcConnected = true;
    Serial.println("RTC found successfully!");
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting time to compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  setupMotorPWM();
  setMotorSpeed(0);

  // Initialize level sensors
  pinMode(levelA, INPUT);  // Feeder basket sensor
  pinMode(levelB, INPUT);
  pinMode(levelC, INPUT);

  // Initialize Ultrasonic Sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);
  
  // Test ultrasonic sensor
  checkUltrasonicStatus();

  pinMode(wifiapMode, INPUT);

  if (digitalRead(wifiapMode) == HIGH) {
    connectToWiFi();
  } else {
    startAPMode();
  }

  server.on("/", handleRoot);
  server.on("/feed", handleFeed);
  server.on("/time", handleTime);
  server.on("/settings", handleSettings);
  server.on("/schedule", handleSchedule);
  server.on("/add-time", handleAddTime);
  server.on("/update", handleUpdate);
  server.on("/delete-time", handleDeleteTime);
  server.on("/toggle-time", handleToggleTime);
  server.on("/wifi-config", handleWiFiConfig);
  server.on("/wifi-update", handleWiFiUpdate);
  server.on("/history", handleHistory);
  server.on("/set-time", handleSetTime); // NEW: Browser time endpoint
  server.begin();
  Serial.println("HTTP server started");

  Serial.println("System initialized - ALL MOTORS ARE STOPPED");
  Serial.print("Initial free heap: ");
  Serial.println(ESP.getFreeHeap());
}

// ================== BROWSER TIME SYNC IMPLEMENTATION ==================

void handleTime() {
  // NEW: Use browser time instead of compilation time
  String html = "<!DOCTYPE html><html><head><title>Sync Time</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 40px; background: #f0f0f0; text-align: center; }";
  html += ".card { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); display: inline-block; }";
  html += ".spinner { border: 4px solid #f3f3f3; border-top: 4px solid #007bff; border-radius: 50%; width: 40px; height: 40px; animation: spin 1s linear infinite; margin: 20px auto; }";
  html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
  html += "</style>";
  html += "<script>";
  html += "function syncWithBrowserTime() {";
  html += "  var now = new Date();";
  html += "  console.log('Browser time: ' + now.toString());";
  html += "  ";
  html += "  // Create hidden form with browser time";
  html += "  var form = document.createElement('form');";
  html += "  form.method = 'POST';";
  html += "  form.action = '/set-time';";
  html += "  ";
  html += "  function addInput(name, value) {";
  html += "    var input = document.createElement('input');";
  html += "    input.type = 'hidden';";
  html += "    input.name = name;";
  html += "    input.value = value;";
  html += "    form.appendChild(input);";
  html += "  }";
  html += "  ";
  html += "  // Add time components to form";
  html += "  addInput('year', now.getFullYear());";
  html += "  addInput('month', now.getMonth() + 1);"; // JavaScript months are 0-11
  html += "  addInput('day', now.getDate());";
  html += "  addInput('hour', now.getHours());";
  html += "  addInput('minute', now.getMinutes());";
  html += "  addInput('second', now.getSeconds());";
  html += "  ";
  html += "  document.body.appendChild(form);";
  html += "  form.submit();";
  html += "}";
  html += "// Auto-execute on page load";
  html += "window.onload = function() {";
  html += "  setTimeout(syncWithBrowserTime, 1000);"; // Small delay to show UI
  html += "};";
  html += "</script>";
  html += "</head><body>";
  html += "<div class='card'>";
  html += "<h1>🕐 Syncing Time</h1>";
  html += "<div class='spinner'></div>";
  html += "<p>Getting current time from your browser...</p>";
  html += "<p><small>This uses your computer/phone's accurate time</small></p>";
  html += "</div>";
  html += "</body></html>";
  
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSetTime() {
  // NEW: Handle the browser time submission
  if (server.method() == HTTP_POST) {
    if (server.hasArg("year") && server.hasArg("month") && server.hasArg("day") && 
        server.hasArg("hour") && server.hasArg("minute") && server.hasArg("second")) {
      
      int year = server.arg("year").toInt();
      int month = server.arg("month").toInt();
      int day = server.arg("day").toInt();
      int hour = server.arg("hour").toInt();
      int minute = server.arg("minute").toInt();
      int second = server.arg("second").toInt();
      
      // Validate the time (basic validation)
      if (year >= 2024 && year <= 2030 &&
          month >= 1 && month <= 12 &&
          day >= 1 && day <= 31 &&
          hour >= 0 && hour <= 23 &&
          minute >= 0 && minute <= 59 &&
          second >= 0 && second <= 59) {
        
        DateTime browserTime(year, month, day, hour, minute, second);
        
        if (rtcConnected) {
          rtc.adjust(browserTime);
          Serial.println("✅ RTC synchronized with browser time: " + String(browserTime.timestamp()));
        } else {
          softwareTime = browserTime;
          softwareClockMillis = millis();
          Serial.println("✅ Software clock synchronized with browser time: " + String(browserTime.timestamp()));
        }
        
        blinkLED(RTCAvailable, 3, 200);
        
        // Success response with redirect
        server.send(200, "text/html", 
          "<script>alert('✅ Time synchronized successfully!\\\\nNew time: " + 
          String(browserTime.timestamp()) + "'); window.location='/';</script>");
        return;
      }
    }
    
    // Error response
    server.send(200, "text/html", 
      "<script>alert('❌ Failed to sync time! Invalid time received.'); window.location='/';</script>");
  } else {
    server.sendHeader("Location", "/");
    server.send(303);
  }
}

// ================== FIXED FEEDER BASKET LOGIC ==================

bool isFeederBasketEmpty() {
  // INVERTED LOGIC: 
  // LOW = Basket has food (sensor not triggered)
  // HIGH = Basket empty (sensor triggered)
  return (digitalRead(levelA) == HIGH);
}

// ================== IMPROVEMENT 1: ADD MISSING FUNCTION DECLARATIONS ==================

void setupServo() {
  if (!servoAttached) {
    feederServo.setPeriodHertz(50);
    feederServo.attach(servoPin, 500, 2400);
    servoAttached = true;
    delay(100);
    feederServo.write(servoStop);
    delay(100);
    Serial.println("Servo initialized and stopped");
  }
}

// ================== IMPROVEMENT 2: IMPROVED ERROR RECOVERY ==================

long readUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) {
    return -1;
  }
  return duration * 0.034 / 2;
}

void checkUltrasonicStatus() {
  Serial.println("Testing ultrasonic sensor...");
  
  // Attempt recovery by reinitializing pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);
  delay(50);
  
  long distance = readUltrasonic();
  if (distance == -1) {
    ultrasonicWorking = false;
    Serial.println("❌ Ultrasonic sensor not working!");
    
    // Additional recovery attempt
    delay(100);
    pinMode(echoPin, INPUT_PULLUP);
    delay(50);
    
    // Test one more time
    distance = readUltrasonic();
    if (distance == -1) {
      Serial.println("❌ Ultrasonic sensor recovery failed!");
    } else {
      ultrasonicWorking = true;
      lastWaterDistance = distance;
      Serial.print("✅ Ultrasonic sensor recovered! Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
    }
  } else {
    ultrasonicWorking = true;
    lastWaterDistance = distance;
    Serial.print("✅ Ultrasonic sensor working. Initial distance: ");
    Serial.print(distance);
    Serial.println(" cm");
  }
}

// ================== IMPROVEMENT 3: SAFETY TIMEOUT FOR FEEDING ==================

void feedFish(float grams) {
  feedFishWithSafety(grams, "manual");
}

void feedFishWithSafety(float grams, String feedType) {
  float feedDuration = calculateFeedDuration(grams);
  unsigned long feedingDuration = feedDuration * 1000;
  unsigned long startTime = millis();
  
  // Safety: maximum feeding time (2 minutes)
  unsigned long maxDuration = 120000;
  
  if (feedingDuration > maxDuration) {
    Serial.println("⚠️  Feeding duration too long, limiting to 2 minutes");
    feedingDuration = maxDuration;
  }
  
  int motorPercent = (motorSpeed * 100) / 255;
  
  Serial.print("Feeding: "); 
  Serial.print(grams); 
  Serial.print("g for "); 
  Serial.print(feedDuration, 1); 
  Serial.print(" seconds at motor speed: ");
  Serial.print(motorPercent);
  Serial.print("%, servo speed: ");
  Serial.print(servoSpeed);
  Serial.print(" (");
  Serial.print(clockwiseRotation ? "CW" : "CCW");
  Serial.println(")");
  
  setMotorSpeed(motorSpeed);
  setServoSpeed(servoSpeed, clockwiseRotation);
  
  Serial.print("Feeding duration: ");
  Serial.print(feedDuration, 1);
  Serial.print(" seconds (");
  Serial.print(feedingDuration);
  Serial.println(" ms)");
  
  bool feedingSuccess = true;
  bool emergencyStop = false;
  
  // Use non-blocking delay for better responsiveness
  while (millis() - startTime < feedingDuration) {
    server.handleClient(); // Keep handling web requests
    
    // Emergency stop check - basket empty (using fixed logic)
    if (isFeederBasketEmpty()) {
      Serial.println("🛑 EMERGENCY STOP: Feeder basket empty!");
      emergencyStop = true;
      feedingSuccess = false;
      break;
    }
    
    // Emergency stop check - motor/servo failure detection
    if (millis() - startTime > 5000) { // After 5 seconds
      // Check if motors are still responding
      if (!servoAttached) {
        Serial.println("🛑 EMERGENCY STOP: Servo detached unexpectedly!");
        feedingSuccess = false;
        break;
      }
    }
    
    delay(100);
  }
  
  stopServo();
  setMotorSpeed(0);
  
  if (emergencyStop) {
    Serial.println("Feeding stopped due to emergency!");
    blinkLED(FishAvailable, 10, 100); // Rapid blinking for emergency
  } else if (feedingSuccess) {
    Serial.println("Feeding complete!");
  } else {
    Serial.println("Feeding completed with issues!");
  }
  
  // Log the feeding
  logFeeding(grams, feedingSuccess, feedType);
}

// ================== IMPROVEMENT 4: SYSTEM HEALTH MONITORING ==================

void systemHealthCheck() {
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck > 30000) { // Every 30 seconds
    lastHealthCheck = millis();
    
    Serial.println("=== SYSTEM HEALTH CHECK ===");
    
    // Check WiFi connection
    if (digitalRead(wifiapMode) == HIGH && WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️  WiFi disconnected, attempting reconnect...");
      WiFi.reconnect();
      delay(1000);
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ WiFi reconnected successfully!");
      } else {
        Serial.println("❌ WiFi reconnect failed!");
      }
    } else if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ WiFi: Connected");
    }
    
    // Check RTC
    if (rtcConnected) {
      if (!rtc.begin()) {
        Serial.println("⚠️  RTC connection lost!");
        rtcConnected = false;
        useSoftwareClock = true;
      } else {
        Serial.println("✅ RTC: Connected");
      }
    } else {
      Serial.println("⚠️  RTC: Using software clock");
    }
    
    // Check ultrasonic sensor
    if (!ultrasonicWorking) {
      Serial.println("🔄 Attempting ultrasonic sensor recovery...");
      checkUltrasonicStatus();
    } else {
      Serial.println("✅ Ultrasonic: Working");
    }
    
    // Check memory
    int freeHeap = ESP.getFreeHeap();
    Serial.print("📊 Free heap: ");
    Serial.print(freeHeap);
    Serial.println(" bytes");
    
    if (freeHeap < 10000) {
      Serial.println("⚠️  Low memory warning!");
    }
    
    // Check feed history size
    Serial.print("📈 Feed history records: ");
    Serial.println(feedHistory.size());
    
    Serial.println("=== HEALTH CHECK COMPLETE ===");
  }
}

// ================== IMPROVEMENT 5: FEED HISTORY TRACKING ==================

void logFeeding(float grams, bool success, String type) {
  FeedHistory record;
  record.timestamp = getCurrentTime();
  record.amount = grams;
  record.success = success;
  record.type = type;
  
  feedHistory.push_back(record);
  
  // Keep only last MAX_HISTORY_RECORDS records
  if (feedHistory.size() > MAX_HISTORY_RECORDS) {
    feedHistory.erase(feedHistory.begin());
  }
  
  Serial.print("📝 Logged feeding: ");
  Serial.print(grams);
  Serial.print("g, ");
  Serial.print(success ? "success" : "failed");
  Serial.print(", type: ");
  Serial.println(type);
}

String getFeedHistoryHTML() {
  String html = "<div style='max-height: 300px; overflow-y: auto; margin: 10px 0;'>";
  html += "<table style='width: 100%; border-collapse: collapse;'>";
  html += "<tr style='background: #f8f9fa; position: sticky; top: 0;'>";
  html += "<th style='padding: 8px; border: 1px solid #ddd;'>Date/Time</th>";
  html += "<th style='padding: 8px; border: 1px solid #ddd;'>Amount</th>";
  html += "<th style='padding: 8px; border: 1px solid #ddd;'>Type</th>";
  html += "<th style='padding: 8px; border: 1px solid #ddd;'>Status</th>";
  html += "</tr>";
  
  if (feedHistory.empty()) {
    html += "<tr><td colspan='4' style='padding: 8px; text-align: center;'>No feeding history</td></tr>";
  } else {
    for (int i = feedHistory.size() - 1; i >= 0; i--) {
      FeedHistory record = feedHistory[i];
      String timestamp = String(record.timestamp.year()) + "-" + 
                        String(record.timestamp.month()) + "-" + 
                        String(record.timestamp.day()) + " " + 
                        String(record.timestamp.hour()) + ":" + 
                        String(record.timestamp.minute());
      
      html += "<tr>";
      html += "<td style='padding: 8px; border: 1px solid #ddd;'>" + timestamp + "</td>";
      html += "<td style='padding: 8px; border: 1px solid #ddd;'>" + String(record.amount, 1) + "g</td>";
      html += "<td style='padding: 8px; border: 1px solid #ddd;'>" + record.type + "</td>";
      html += "<td style='padding: 8px; border: 1px solid #ddd;'>";
      html += record.success ? 
        "<span style='color: green; font-weight: bold;'>✅ Success</span>" : 
        "<span style='color: red; font-weight: bold;'>❌ Failed</span>";
      html += "</td>";
      html += "</tr>";
    }
  }
  html += "</table></div>";
  return html;
}

// ================== ULTRASONIC FUNCTIONS ==================

void monitorWaterSurface() {
  static unsigned long lastCheck = 0;
  static float distanceHistory[5] = {0};
  static int historyIndex = 0;
  
  if (millis() - lastCheck < 1000) {
    return;
  }
  lastCheck = millis();
  
  if (!ultrasonicWorking) {
    if (millis() % 10000 == 0) {
      checkUltrasonicStatus();
    }
    return;
  }
  
  long currentDistance = readUltrasonic();
  if (currentDistance == -1) {
    ultrasonicWorking = false;
    Serial.println("Ultrasonic sensor error detected");
    return;
  }
  
  distanceHistory[historyIndex] = currentDistance;
  historyIndex = (historyIndex + 1) % 5;
  
  float sum = 0;
  float mean = 0;
  float variance = 0;
  
  for (int i = 0; i < 5; i++) {
    sum += distanceHistory[i];
  }
  mean = sum / 5;
  
  for (int i = 0; i < 5; i++) {
    variance += pow(distanceHistory[i] - mean, 2);
  }
  variance = variance / 5;
  
  bool newFishMoving = (variance > waterStabilityThreshold);
  
  if (newFishMoving && !fishMoving) {
    fishMoving = true;
    lastMovementTime = millis();
    Serial.print("🎣 Fish movement detected! Variance: ");
    Serial.print(variance);
    Serial.print(" cm², Threshold: ");
    Serial.println(waterStabilityThreshold);
  } 
  else if (fishMoving && !newFishMoving) {
    if (millis() - lastMovementTime > movementTimeout) {
      fishMoving = false;
      Serial.println("🐟 Water surface calm - fish are calm");
    }
  }
  else if (fishMoving && newFishMoving) {
    lastMovementTime = millis();
  }
  
  lastWaterDistance = currentDistance;
}

// ================== WIFI FUNCTIONS ==================

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 200) {
    delay(500);
    Serial.print(".");
    digitalWrite(WebAvailable, !digitalRead(WebAvailable));
    wifiTimeout++;
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.print("IP address: "); 
    Serial.println(WiFi.localIP());

    if (MDNS.begin("fishfeeder")) {
      Serial.println("mDNS responder started: http://fishfeeder.local/");
    }
  } else {
    Serial.println("Failed to connect to WiFi!");
    Serial.println("Switching to AP mode...");
    startAPMode();
  }
}

void startAPMode() {
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Access Point started: ");
  Serial.println(ap_ssid);
  Serial.print("IP address: "); 
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin("fishfeeder")) {
    Serial.println("mDNS responder started: http://fishfeeder.local/");
  }
}

void saveWiFiSettings() {
  WiFiSettings wifiSettings;
  strncpy(wifiSettings.ssid, ssid, sizeof(wifiSettings.ssid) - 1);
  strncpy(wifiSettings.password, password, sizeof(wifiSettings.password) - 1);
  wifiSettings.ssid[sizeof(wifiSettings.ssid) - 1] = '\0';
  wifiSettings.password[sizeof(wifiSettings.password) - 1] = '\0';
  
  wifiSettings.checksum = calculateChecksum((uint8_t*)&wifiSettings, sizeof(wifiSettings) - sizeof(wifiSettings.checksum));
  
  EEPROM.put(WIFI_SETTINGS_START, wifiSettings);
  
  if (EEPROM.commit()) {
    Serial.println("=== WIFI SETTINGS SAVED TO EEPROM ===");
    Serial.print("SSID: "); Serial.println(ssid);
    Serial.println("====================================");
  } else {
    Serial.println("ERROR: Failed to save WiFi settings to EEPROM!");
  }
}

void loadWiFiSettings() {
  WiFiSettings wifiSettings;
  EEPROM.get(WIFI_SETTINGS_START, wifiSettings);
  
  uint32_t storedChecksum = wifiSettings.checksum;
  wifiSettings.checksum = 0;
  uint32_t calculatedChecksum = calculateChecksum((uint8_t*)&wifiSettings, sizeof(wifiSettings));
  
  if (calculatedChecksum == storedChecksum && storedChecksum != 0) {
    strncpy(ssid, wifiSettings.ssid, sizeof(ssid) - 1);
    strncpy(password, wifiSettings.password, sizeof(password) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    password[sizeof(password) - 1] = '\0';
    
    Serial.println("=== WIFI SETTINGS LOADED FROM EEPROM ===");
    Serial.print("SSID: "); Serial.println(ssid);
    Serial.println("======================================");
  } else {
    Serial.println("No valid WiFi settings found in EEPROM, using defaults");
    saveWiFiSettings();
  }
}

// ================== EEPROM FUNCTIONS ==================

uint32_t calculateChecksum(uint8_t* data, size_t len) {
  uint32_t checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum += data[i];
  }
  return checksum;
}

void saveSettings() {
  Settings settings;
  settings.gramsPerSecond = gramsPerSecond;
  settings.feedAmountGrams = feedAmountGrams;
  settings.motorSpeed = motorSpeed;
  settings.servoSpeed = servoSpeed;
  settings.clockwiseRotation = clockwiseRotation;
  settings.waterStabilityThreshold = waterStabilityThreshold;
  settings.movementTimeout = movementTimeout;
  
  settings.checksum = calculateChecksum((uint8_t*)&settings, sizeof(settings) - sizeof(settings.checksum));
  
  EEPROM.put(SETTINGS_START, settings);
  
  if (EEPROM.commit()) {
    Serial.println("=== SETTINGS SAVED TO EEPROM ===");
  } else {
    Serial.println("ERROR: Failed to save settings to EEPROM!");
  }
}

void loadSettings() {
  Settings settings;
  EEPROM.get(SETTINGS_START, settings);
  
  uint32_t storedChecksum = settings.checksum;
  settings.checksum = 0;
  uint32_t calculatedChecksum = calculateChecksum((uint8_t*)&settings, sizeof(settings));
  
  if (calculatedChecksum == storedChecksum && storedChecksum != 0) {
    gramsPerSecond = settings.gramsPerSecond;
    feedAmountGrams = settings.feedAmountGrams;
    motorSpeed = settings.motorSpeed;
    servoSpeed = settings.servoSpeed;
    clockwiseRotation = settings.clockwiseRotation;
    waterStabilityThreshold = settings.waterStabilityThreshold;
    movementTimeout = settings.movementTimeout;
    
    Serial.println("=== SETTINGS LOADED FROM EEPROM ===");
  } else {
    Serial.println("No valid settings found in EEPROM, using defaults");
    saveSettings();
  }
}

void saveScheduleToEEPROM() {
  int address = SCHEDULE_START;
  int scheduleSize = feedingSchedule.size();
  EEPROM.put(address, scheduleSize);
  address += sizeof(scheduleSize);
  
  for (int i = 0; i < scheduleSize; i++) {
    EEPROM.put(address, feedingSchedule[i]);
    address += sizeof(FeedingTime);
  }
  
  uint32_t checksum = 0;
  for (int i = SCHEDULE_START; i < address; i++) {
    checksum += EEPROM.read(i);
  }
  EEPROM.put(address, checksum);
  
  if (EEPROM.commit()) {
    Serial.println("Schedule saved to EEPROM: " + String(scheduleSize) + " entries");
  } else {
    Serial.println("ERROR: Failed to save schedule to EEPROM!");
  }
}

void loadScheduleFromEEPROM() {
  int address = SCHEDULE_START;
  int scheduleSize;
  EEPROM.get(address, scheduleSize);
  address += sizeof(scheduleSize);
  
  if (scheduleSize < 0 || scheduleSize > 50) {
    Serial.println("Invalid schedule size in EEPROM, using empty schedule");
    feedingSchedule.clear();
    return;
  }
  
  uint32_t calculatedChecksum = 0;
  for (int i = SCHEDULE_START; i < address + (scheduleSize * sizeof(FeedingTime)); i++) {
    calculatedChecksum += EEPROM.read(i);
  }
  
  uint32_t storedChecksum;
  EEPROM.get(address + (scheduleSize * sizeof(FeedingTime)), storedChecksum);
  
  if (calculatedChecksum != storedChecksum || storedChecksum == 0) {
    Serial.println("Invalid schedule checksum in EEPROM, using empty schedule");
    feedingSchedule.clear();
    return;
  }
  
  feedingSchedule.clear();
  for (int i = 0; i < scheduleSize; i++) {
    FeedingTime feeding;
    EEPROM.get(address, feeding);
    address += sizeof(FeedingTime);
    feedingSchedule.push_back(feeding);
  }
  
  Serial.println("Schedule loaded from EEPROM: " + String(scheduleSize) + " entries");
}

// ================== MOTOR CONTROL FUNCTIONS ==================

void stopAllMotors() {
  if (motorPWMInitialized) {
    ledcWrite(motorPin, 0);
  } else {
    digitalWrite(motorPin, LOW);
  }
  
  if (servoAttached) {
    feederServo.write(servoStop);
    delay(100);
    feederServo.detach();
    servoAttached = false;
  }
  
  Serial.println("All motors stopped during initialization");
}

void setupMotorPWM() {
  ledcAttach(motorPin, motorPWMFreq, motorPWMResolution);
  motorPWMInitialized = true;
  ledcWrite(motorPin, 0);
  Serial.println("Motor PWM initialized and STOPPED");
}

void setMotorSpeed(int speed) {
  if (!motorPWMInitialized) {
    setupMotorPWM();
  }
  
  speed = constrain(speed, 0, maxMotorSpeed);
  ledcWrite(motorPin, speed);
  
  if (speed == 0) {
    Serial.println("Motor STOPPED (speed: 0/255)");
  } else {
    Serial.print("Motor speed set to: ");
    Serial.print(speed);
    Serial.print("/255 (");
    Serial.print((speed * 100) / 255);
    Serial.println("%)");
  }
}

// ================== CONTINUOUS SERVO CONTROL ==================

void setServoSpeed(int speed, bool clockwise) {
  setupServo();
  speed = constrain(speed, 0, maxServoSpeed);
  
  int servoValue;
  if (speed == 0) {
    servoValue = servoStop;
  } else if (clockwise) {
    servoValue = servoStop + speed;
  } else {
    servoValue = servoStop - speed;
  }
  
  servoValue = constrain(servoValue, 0, 180);
  feederServo.write(servoValue);
  
  if (speed == 0) {
    Serial.println("Servo STOPPED");
  } else {
    Serial.print("Servo set to: ");
    Serial.print(servoValue);
    Serial.print(" (Speed: ");
    Serial.print(speed);
    Serial.print(", Direction: ");
    Serial.print(clockwise ? "CW" : "CCW");
    Serial.println(")");
  }
}

void stopServo() {
  if (servoAttached) {
    feederServo.write(servoStop);
    delay(100);
    feederServo.detach();
    servoAttached = false;
    Serial.println("Servo stopped and detached");
  }
}

void loop() {
  server.handleClient();

  // System health monitoring
  systemHealthCheck();

  if (useSoftwareClock) {
    updateSoftwareClock();
  }

  DateTime now = getCurrentTime();

  if (now.hour() == 0 && now.minute() == 1) {
    for (auto& feeding : feedingSchedule) {
      feeding.alreadyFed = false;
    }
    Serial.println("Reset daily feeding flags");
    saveScheduleToEEPROM();
    blinkLED(FishAvailable, 2, 100);
  }

  for (auto& feeding : feedingSchedule) {
    if (feeding.enabled && !feeding.alreadyFed && 
        now.hour() == feeding.hour && now.minute() == feeding.minute) {
      Serial.print("Auto feeding scheduled for ");
      Serial.print(feeding.hour);
      Serial.print(":");
      Serial.println(feeding.minute < 10 ? "0" + String(feeding.minute) : String(feeding.minute));
      
      blinkLED(FishAvailable, 3, 150);
      feedFishWithSafety(feedAmountGrams, "scheduled");
      feeding.alreadyFed = true;
      saveScheduleToEEPROM();
      blinkLED(FishAvailable, 2, 100);
    }
  }
  
  monitorWaterSurface();
  
  updateLEDs();
  delay(1000);
}

// ================== LED FUNCTIONS ==================

void updateLEDs() {
  static unsigned long lastWiFiBlink = 0;
  static bool wifiBlinkState = false;
  
  if (WiFi.status() != WL_CONNECTED && digitalRead(wifiapMode) == HIGH) {
    if (millis() - lastWiFiBlink > 500) {
      wifiBlinkState = !wifiBlinkState;
      digitalWrite(WebAvailable, wifiBlinkState);
      lastWiFiBlink = millis();
    }
  } else {
    digitalWrite(WebAvailable, HIGH);
  }
  
  // RTC LED - fast blinking if not connected, solid if working
  static unsigned long lastRTCBlink = 0;
  static bool rtcBlinkState = false;
  
  if (!rtcConnected) {
    if (millis() - lastRTCBlink > 200) {
      rtcBlinkState = !rtcBlinkState;
      digitalWrite(RTCAvailable, rtcBlinkState);
      lastRTCBlink = millis();
    }
  } else {
    digitalWrite(RTCAvailable, HIGH);
  }
  
  // Update FishAvailable LED
  static unsigned long lastMovementBlink = 0;
  static unsigned long movementBlinkInterval = 0;
  static unsigned long basketBlinkInterval = 0;
  
  bool basketEmpty = isFeederBasketEmpty(); // USING FIXED LOGIC
  
  if (!ultrasonicWorking) {
    // Fast blink for ultrasonic error
    blinkLED(FishAvailable, 2, 100);
  } 
  else if (basketEmpty) {
    // Triple blink periodically when feeder basket is empty
    if (millis() - basketBlinkInterval > 3000) {
      blinkLED(FishAvailable, 3, 200);
      basketBlinkInterval = millis();
    }
  }
  else if (fishMoving) {
    // Triple blink periodically while fish are moving
    if (millis() - movementBlinkInterval > 2000) {
      blinkLED(FishAvailable, 3, 150);
      movementBlinkInterval = millis();
    }
  }
  else {
    // Solid ON when normal
    digitalWrite(FishAvailable, HIGH);
  }
}

void blinkLED(int pin, int times, int delayTime) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(delayTime);
    digitalWrite(pin, LOW);
    if (i < times - 1) delay(delayTime);
  }
}

// ================== TIME FUNCTIONS ==================

DateTime getCurrentTime() {
  if (rtcConnected) {
    return rtc.now();
  } else if (useSoftwareClock) {
    return softwareTime;
  } else {
    return DateTime(F(__DATE__), F(__TIME__));
  }
}

void updateSoftwareClock() {
  unsigned long currentMillis = millis();
  unsigned long elapsedSeconds = (currentMillis - softwareClockMillis) / 1000;
  
  if (elapsedSeconds >= 1) {
    softwareTime = softwareTime + TimeSpan(elapsedSeconds);
    softwareClockMillis = currentMillis - ((currentMillis - softwareClockMillis) % 1000);
  }
}

// ================== WIFI CONFIG HANDLERS ==================

void handleWiFiConfig() {
  String html = "<!DOCTYPE html><html><head><title>Fish Feeder - WiFi Configuration</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv='refresh' content='30'>"; // AUTO-REFRESH every 30 seconds
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".info{background:#d1ecf1;border-left:4px solid #17a2b8;color:#0c5460;padding:10px;border-radius:5px;}";
  html += ".warning{background:#fff3cd;border-left:4px solid #ffc107;color:#856404;}";
  html += "form{margin:15px 0;}label{display:block;margin:8px 0 3px 0;font-weight:bold;}";
  html += "input,select{padding:8px;margin:5px 0;width:250px;border:1px solid #ccc;border-radius:4px;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-secondary{background:#6c757d;color:white;}";
  html += ".refresh-info{background:#e7f3ff;padding:8px;border-radius:5px;margin:10px 0;font-size:14px;}";
  html += "</style></head><body>";
  html += "<h1>📶 WiFi Configuration</h1>";
  
  html += "<div class='refresh-info'>";
  html += "🔄 Page auto-refreshes every 30 seconds";
  html += "</div>";
  
  html += "<div class='card'>";
  
  if (digitalRead(wifiapMode) == HIGH) {
    html += "<div class='info'>";
    html += "<p><b>Current Mode:</b> Station Mode (Connecting to WiFi)</p>";
    html += "<p><b>Current SSID:</b> " + String(ssid) + "</p>";
    html += "</div>";
  } else {
    html += "<div class='warning'>";
    html += "<p><b>Current Mode:</b> Access Point Mode</p>";
    html += "<p><b>AP SSID:</b> " + String(ap_ssid) + "</p>";
    html += "<p><b>AP Password:</b> " + String(ap_password) + "</p>";
    html += "</div>";
  }
  
  html += "<h3>Configure WiFi Credentials</h3>";
  html += "<p>These settings will be used when the device is in Station Mode (pin 34 HIGH).</p>";
  
  html += "<form action='/wifi-update' method='POST'>";
  html += "<label for='ssid'>WiFi SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' value='" + String(ssid) + "' maxlength='31' required><br>";
  
  html += "<label for='password'>WiFi Password:</label>";
  html += "<input type='password' id='password' name='password' value='" + String(password) + "' maxlength='63'><br>";
  html += "<small>Leave password empty if the network has no password</small><br><br>";
  
  html += "<button type='submit' class='btn btn-primary'>Save WiFi Settings</button>";
  html += "<a href='/'><button type='button' class='btn btn-secondary'>Back to Main</button></a>";
  html += "</form>";
  
  html += "<div class='info' style='margin-top:20px;'>";
  html += "<p><b>Note:</b> After saving, the device will continue running in current mode.</p>";
  html += "<p>To apply new WiFi settings:</p>";
  html += "<ol>";
  html += "<li>Set pin 34 to HIGH (Station Mode)</li>";
  html += "<li>Restart the device</li>";
  html += "<li>The device will attempt to connect using the new credentials</li>";
  html += "</ol>";
  html += "</div>";
  
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleWiFiUpdate() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("ssid")) {
      String newSSID = server.arg("ssid");
      if (newSSID.length() > 0 && newSSID.length() < sizeof(ssid)) {
        strncpy(ssid, newSSID.c_str(), sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
        Serial.print("Updated SSID to: ");
        Serial.println(ssid);
      }
    }
    
    if (server.hasArg("password")) {
      String newPassword = server.arg("password");
      if (newPassword.length() < sizeof(password)) {
        strncpy(password, newPassword.c_str(), sizeof(password) - 1);
        password[sizeof(password) - 1] = '\0';
        Serial.println("Updated WiFi password");
      }
    }
    
    saveWiFiSettings();
    blinkLED(WebAvailable, 3, 200);
    
    server.sendHeader("Location", "/wifi-config");
    server.send(303);
  } else {
    server.sendHeader("Location", "/");
    server.send(303);
  }
}

// ================== HISTORY HANDLER ==================

void handleHistory() {
  String html = "<!DOCTYPE html><html><head><title>Fish Feeder - Feeding History</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv='refresh' content='30'>"; // AUTO-REFRESH every 30 seconds
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-secondary{background:#6c757d;color:white;}";
  html += "table{width:100%;border-collapse:collapse;}";
  html += "th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd;}";
  html += "tr:hover{background:#f5f5f5;}";
  html += ".refresh-info{background:#e7f3ff;padding:8px;border-radius:5px;margin:10px 0;font-size:14px;}";
  html += "</style></head><body>";
  html += "<h1>📊 Feeding History</h1>";
  
  html += "<div class='refresh-info'>";
  html += "🔄 Page auto-refreshes every 30 seconds";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>Recent Feedings</h3>";
  html += "<p>Showing last " + String(MAX_HISTORY_RECORDS) + " feeding records</p>";
  
  html += getFeedHistoryHTML();
  
  html += "<br>";
  html += "<a href='/'><button class='btn btn-secondary'>Back to Main</button></a>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// ================== MAIN HANDLERS ==================

void handleRoot() {
  DateTime now = getCurrentTime();
  float feedDuration = calculateFeedDuration(feedAmountGrams);
  int motorPercent = (motorSpeed * 100) / 255;
  bool basketEmpty = isFeederBasketEmpty(); // USING FIXED LOGIC

  String html = "<!DOCTYPE html><html><head><title>Fish Feeder</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv='refresh' content='15'>"; // AUTO-REFRESH every 15 seconds - OPTIMAL FOR MAIN PAGE
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".warning{background:#fff3cd;border-left:4px solid #ffc107;color:#856404;}";
  html += ".success{background:#d4edda;border-left:4px solid #28a745;}";
  html += ".info{background:#d1ecf1;border-left:4px solid #17a2b8;color:#0c5460;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-success{background:#28a745;color:white;}";
  html += ".btn-warning{background:#ffc107;color:black;}";
  html += ".btn-danger{background:#dc3545;color:white;}";
  html += ".btn-info{background:#17a2b8;color:white;}";
  html += ".schedule-item{margin:5px 0;padding:10px;background:#f8f9fa;border-radius:5px;}";
  html += ".led-indicator{display:inline-block;width:12px;height:12px;border-radius:50%;margin-right:5px;}";
  html += ".led-green{background:#28a745;}";
  html += ".led-red{background:#dc3545;}";
  html += ".led-yellow{background:#ffc107;}";
  html += ".led-off{background:#6c757d;}";
  html += ".speed-indicator{display:inline-block;padding:5px 10px;background:#17a2b8;color:white;border-radius:15px;font-size:14px;}";
  html += ".refresh-info{background:#e7f3ff;padding:8px;border-radius:5px;margin:10px 0;font-size:14px;}";
  html += "</style></head><body>";
  html += "<h1>🐟 ESP32 Fish Feeder</h1>";
  
  html += "<div class='refresh-info'>";
  html += "🔄 Page auto-refreshes every 15 seconds - Last update: " + String(now.timestamp());
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>System Status</h3>";
  html += "<p>";
  html += "<span class='led-indicator " + String(WiFi.status() == WL_CONNECTED ? "led-green" : "led-red") + "'></span> WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  html += "&nbsp;&nbsp;&nbsp;";
  html += "<span class='led-indicator " + String(rtcConnected ? "led-green" : "led-red") + "'></span> RTC: " + String(rtcConnected ? "Connected" : "Disconnected");
  html += "&nbsp;&nbsp;&nbsp;";
  
  String ultrasonicStatus = ultrasonicWorking ? 
    "<span class='led-indicator led-green'></span> Ultrasonic: Working" :
    "<span class='led-indicator led-red'></span> Ultrasonic: Not Working";
  html += ultrasonicStatus;
  html += "&nbsp;&nbsp;&nbsp;";
  
  String fishStatus = fishMoving ? 
    "<span class='led-indicator led-yellow'></span> Fish: Moving" :
    "<span class='led-indicator led-green'></span> Fish: Calm";
  html += fishStatus;
  html += "</p>";
  
  // USING FIXED BASKET LOGIC
  String basketStatus = basketEmpty ? 
    "<span class='led-indicator led-red'></span> Feeder Basket: EMPTY - Needs Refill" :
    "<span class='led-indicator led-green'></span> Feeder Basket: Has Food";
  html += "<p>" + basketStatus + "</p>";
  
  if (digitalRead(wifiapMode) == HIGH) {
    html += "<p><b>WiFi Mode:</b> Station (Connected to: " + String(ssid) + ")</p>";
  } else {
    html += "<p><b>WiFi Mode:</b> Access Point (SSID: " + String(ap_ssid) + ")</p>";
  }
  
  html += "<p><b>Current Water Distance:</b> " + String(lastWaterDistance, 1) + " cm</p>";
  html += "<p><b>Movement Threshold:</b> " + String(waterStabilityThreshold, 1) + " cm² variance</p>";
  html += "<p><b>Movement Timeout:</b> " + String(movementTimeout / 1000) + " seconds</p>";
  html += "</div>";
  
  if (!rtcConnected) {
    html += "<div class='card warning'><h3>⚠️ RTC NOT CONNECTED</h3><p>Using software clock - Time may drift over time</p></div>";
  } else {
    html += "<div class='card success'><p>✅ RTC connected</p></div>";
  }
  
  if (!ultrasonicWorking) {
    html += "<div class='card warning'><h3>⚠️ ULTRASONIC SENSOR NOT WORKING</h3><p>Check connections to pins 15 (Trig) and 2 (Echo)</p></div>";
  }
  
  if (basketEmpty) {
    html += "<div class='card warning'><h3>⚠️ FEEDER BASKET EMPTY</h3><p>Please refill the fish food in the feeder basket</p></div>";
  }
  
  html += "<div class='card'>";
  html += "<p><b>Current Time:</b> " + String(now.timestamp()) + "</p>";
  
  html += "<div class='card info'>";
  html += "<p><b>Feeding Amount:</b> " + String(feedAmountGrams) + " g</p>";
  html += "<p><b>Feed Duration:</b> " + String(feedDuration, 1) + " seconds</p>";
  html += "<p><b>Grams per Second:</b> " + String(gramsPerSecond, 1) + " g/s</p>";
  html += "<p><b>Motor Speed Setting:</b> <span class='speed-indicator'>" + String(motorPercent) + "% (" + String(motorSpeed) + "/255)</span></p>";
  html += "<p><b>Servo Speed Setting:</b> <span class='speed-indicator'>" + String(servoSpeed) + "/" + String(maxServoSpeed) + " (" + (clockwiseRotation ? "CW" : "CCW") + ")</span></p>";
  html += "<p><b>Current Motor State:</b> <span style='color:green;font-weight:bold;'>STOPPED</span></p>";
  html += "</div>";

  html += "<p><b>Feeding Schedule:</b></p>";
  html += "<div style='max-height:200px;overflow-y:auto;margin:10px 0;'>";
  for (size_t i = 0; i < feedingSchedule.size(); i++) {
    String timeStr = String(feedingSchedule[i].hour) + ":" + 
                    (feedingSchedule[i].minute < 10 ? "0" + String(feedingSchedule[i].minute) : String(feedingSchedule[i].minute));
    html += "<div class='schedule-item'>";
    html += "<b>" + timeStr + "</b> - ";
    html += feedingSchedule[i].enabled ? "✅ Enabled" : "❌ Disabled";
    html += " - " + String(feedingSchedule[i].alreadyFed ? "Fed" : "Pending");
    html += "</div>";
  }
  html += "</div>";

  html += "<p>";
  html += "<a href=\"/feed\"><button class=\"btn btn-primary\">Feed Now (" + String(feedAmountGrams) + "g)</button></a>";
  html += "<a href=\"/time\"><button class=\"btn btn-success\">Sync Time</button></a>";
  html += "<a href=\"/settings\"><button class=\"btn btn-warning\">Settings</button></a>";
  html += "<a href=\"/schedule\"><button class=\"btn btn-danger\">Manage Schedule</button></a>";
  html += "<a href=\"/history\"><button class=\"btn btn-info\">View History</button></a>";
  
  if (digitalRead(wifiapMode) == LOW || WiFi.status() != WL_CONNECTED) {
    html += "<a href=\"/wifi-config\"><button class=\"btn btn-info\">WiFi Config</button></a>";
  }
  
  html += "</p>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleFeed() {
  blinkLED(FishAvailable, 3, 150);
  feedFishWithSafety(feedAmountGrams, "manual");
  blinkLED(FishAvailable, 2, 100);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSettings() {
  float feedDuration = calculateFeedDuration(feedAmountGrams);
  int motorPercent = (motorSpeed * 100) / 255;

  String html = "<!DOCTYPE html><html><head><title>Fish Feeder - Settings</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv='refresh' content='30'>"; // AUTO-REFRESH every 30 seconds
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".info{background:#d1ecf1;border-left:4px solid #17a2b8;color:#0c5460;padding:10px;border-radius:5px;}";
  html += "form{margin:15px 0;}label{display:block;margin:8px 0 3px 0;font-weight:bold;}";
  html += "input,select{padding:8px;margin:5px 0;width:200px;border:1px solid #ccc;border-radius:4px;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-secondary{background:#6c757d;color:white;}";
  html += ".speed-info{margin:10px 0;padding:10px;background:#e9ecef;border-radius:5px;}";
  html += ".refresh-info{background:#e7f3ff;padding:8px;border-radius:5px;margin:10px 0;font-size:14px;}";
  html += "</style></head><body>";
  html += "<h1>⚙️ Fish Feeder Settings</h1>";
  
  html += "<div class='refresh-info'>";
  html += "🔄 Page auto-refreshes every 30 seconds";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<div class='info'>";
  html += "<p><b>Current Settings:</b> " + String(feedAmountGrams) + "g for " + String(feedDuration, 1) + " seconds</p>";
  html += "<p><b>Feed Rate:</b> " + String(gramsPerSecond, 1) + " grams per second</p>";
  html += "<p><b>Current Water Distance:</b> " + String(lastWaterDistance, 1) + " cm</p>";
  html += "<p><b>Fish Movement Threshold:</b> " + String(waterStabilityThreshold, 1) + " cm² variance</p>";
  html += "<p><b>Movement Timeout:</b> " + String(movementTimeout / 1000) + " seconds</p>";
  html += "</div>";
  
  html += "<form action='/update' method='POST'>";
  
  html += "<label for='feedAmount'>Feed Amount (grams):</label>";
  html += "<input type='number' id='feedAmount' name='feedAmount' step='0.1' min='1' max='1000' value='" + 
          String(feedAmountGrams) + "' required><br>";
  html += "<small>(1 to 1000 grams)</small><br><br>";
  
  html += "<label for='gramsPerSecond'>Grams per Second:</label>";
  html += "<input type='number' id='gramsPerSecond' name='gramsPerSecond' step='0.1' min='0.1' max='50' value='" + 
          String(gramsPerSecond) + "' required><br>";
  html += "<small>Calibration: How many grams are dispensed per second of servo operation</small><br><br>";
  
  html += "<label for='waterStabilityThreshold'>Fish Movement Threshold (cm² variance):</label>";
  html += "<input type='number' id='waterStabilityThreshold' name='waterStabilityThreshold' step='0.1' min='0.1' max='10' value='" + 
          String(waterStabilityThreshold) + "' required><br>";
  html += "<small>Lower = more sensitive to movement. Higher = less sensitive</small><br><br>";
  
  html += "<label for='movementTimeout'>Movement Timeout (seconds):</label>";
  html += "<input type='number' id='movementTimeout' name='movementTimeout' min='5' max='300' value='" + 
          String(movementTimeout / 1000) + "' required><br>";
  html += "<small>How long to keep 'fish moving' status after last detection (5-300 seconds)</small><br><br>";
  
  html += "<label for='motorSpeed'>Motor Speed (PWM):</label>";
  html += "<input type='range' id='motorSpeed' name='motorSpeed' min='" + String(minMotorSpeed) + 
          "' max='" + String(maxMotorSpeed) + "' value='" + String(motorSpeed) + "' step='5' oninput='updateSpeedValue(this.value)'>";
  html += "<span id='speedValue'>" + String(motorPercent) + "% (" + String(motorSpeed) + "/255)</span><br>";
  html += "<div class='speed-info'>";
  html += "<small>Slow: " + String(minMotorSpeed) + "/255 (" + String((minMotorSpeed * 100) / 255) + "%) - Gentle feeding</small><br>";
  html += "<small>Fast: " + String(maxMotorSpeed) + "/255 (" + String((maxMotorSpeed * 100) / 255) + "%) - Quick feeding</small>";
  html += "</div><br>";
  
  html += "<label for='servoSpeed'>Servo Speed:</label>";
  html += "<input type='range' id='servoSpeed' name='servoSpeed' min='" + String(minServoSpeed) + 
          "' max='" + String(maxServoSpeed) + "' value='" + String(servoSpeed) + "' step='1' oninput='updateServoSpeedValue(this.value)'>";
  html += "<span id='servoSpeedValue'>" + String(servoSpeed) + "/" + String(maxServoSpeed) + "</span><br>";
  html += "<div class='speed-info'>";
  html += "<small>Slow: " + String(minServoSpeed) + " - Gentle rotation</small><br>";
  html += "<small>Fast: " + String(maxServoSpeed) + " - Quick rotation</small>";
  html += "</div><br>";
  
  html += "<label for='servoDirection'>Servo Rotation Direction:</label>";
  html += "<select id='servoDirection' name='servoDirection'>";
  html += "<option value='clockwise' " + String(clockwiseRotation ? "selected" : "") + ">Clockwise</option>";
  html += "<option value='counterclockwise' " + String(!clockwiseRotation ? "selected" : "") + ">Counter-Clockwise</option>";
  html += "</select><br><br>";
  
  html += "<button type='submit' class='btn btn-primary'>Save Settings</button>";
  html += "<a href='/'><button type='button' class='btn btn-secondary'>Back</button></a>";
  html += "</form>";
  
  html += "<script>";
  html += "function updateSpeedValue(value) {";
  html += "  var percent = Math.round((value * 100) / 255);";
  html += "  document.getElementById('speedValue').textContent = percent + '% (' + value + '/255)';";
  html += "}";
  html += "function updateServoSpeedValue(value) {";
  html += "  document.getElementById('servoSpeedValue').textContent = value + '/' + " + String(maxServoSpeed) + ";";
  html += "}";
  html += "</script>";
  
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleUpdate() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("feedAmount")) {
      float newAmount = server.arg("feedAmount").toFloat();
      if (newAmount >= 1.0 && newAmount <= 1000.0) {
        feedAmountGrams = newAmount;
        Serial.print("Updated feed amount to: ");
        Serial.print(feedAmountGrams);
        Serial.println("g");
      }
    }
    
    if (server.hasArg("gramsPerSecond")) {
      float newGramsPerSecond = server.arg("gramsPerSecond").toFloat();
      if (newGramsPerSecond >= 0.1 && newGramsPerSecond <= 50.0) {
        gramsPerSecond = newGramsPerSecond;
        Serial.print("Updated grams per second to: ");
        Serial.println(gramsPerSecond);
      }
    }
    
    if (server.hasArg("waterStabilityThreshold")) {
      float newThreshold = server.arg("waterStabilityThreshold").toFloat();
      if (newThreshold >= 0.1 && newThreshold <= 10.0) {
        waterStabilityThreshold = newThreshold;
        Serial.print("Updated water stability threshold to: ");
        Serial.print(waterStabilityThreshold);
        Serial.println(" cm²");
      }
    }
    
    if (server.hasArg("movementTimeout")) {
      unsigned long newTimeout = server.arg("movementTimeout").toInt() * 1000;
      if (newTimeout >= 5000 && newTimeout <= 300000) {
        movementTimeout = newTimeout;
        Serial.print("Updated movement timeout to: ");
        Serial.print(movementTimeout / 1000);
        Serial.println(" seconds");
      }
    }
    
    if (server.hasArg("motorSpeed")) {
      int newSpeed = server.arg("motorSpeed").toInt();
      if (newSpeed >= minMotorSpeed && newSpeed <= maxMotorSpeed) {
        motorSpeed = newSpeed;
        Serial.print("Motor speed setting updated to: ");
        Serial.print(motorSpeed);
        Serial.println("/255");
      }
    }
    
    if (server.hasArg("servoSpeed")) {
      int newServoSpeed = server.arg("servoSpeed").toInt();
      if (newServoSpeed >= minServoSpeed && newServoSpeed <= maxServoSpeed) {
        servoSpeed = newServoSpeed;
        Serial.print("Servo speed setting updated to: ");
        Serial.println(servoSpeed);
      }
    }
    
    if (server.hasArg("servoDirection")) {
      String direction = server.arg("servoDirection");
      clockwiseRotation = (direction == "clockwise");
      Serial.print("Servo direction setting updated to: ");
      Serial.println(clockwiseRotation ? "Clockwise" : "Counter-Clockwise");
    }
    
    saveSettings();
    blinkLED(FishAvailable, 2, 100);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAddTime() {
  if (server.method() == HTTP_POST) {
    int hour = server.arg("hour").toInt();
    int minute = server.arg("minute").toInt();
    bool enabled = server.hasArg("enabled");
    
    if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59) {
      addFeedingTime(hour, minute, enabled);
      saveScheduleToEEPROM();
    }
  }
  server.sendHeader("Location", "/schedule");
  server.send(303);
}

void handleDeleteTime() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    removeFeedingTime(id);
    saveScheduleToEEPROM();
  }
  server.sendHeader("Location", "/schedule");
  server.send(303);
}

void handleToggleTime() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    if (id >= 0 && id < feedingSchedule.size()) {
      feedingSchedule[id].enabled = !feedingSchedule[id].enabled;
      Serial.print("Toggled feeding time ");
      Serial.print(id);
      Serial.print(" to ");
      Serial.println(feedingSchedule[id].enabled ? "enabled" : "disabled");
      saveScheduleToEEPROM();
    }
  }
  server.sendHeader("Location", "/schedule");
  server.send(303);
}

void handleSchedule() {
  String html = "<!DOCTYPE html><html><head><title>Fish Feeder - Schedule</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<meta http-equiv='refresh' content='30'>"; // AUTO-REFRESH every 30 seconds
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".btn{padding:8px 12px;font-size:14px;border:none;border-radius:4px;cursor:pointer;margin:2px;}";
  html += ".btn-sm{padding:5px 8px;font-size:12px;}";
  html += ".btn-success{background:#28a745;color:white;}";
  html += ".btn-danger{background:#dc3545;color:white;}";
  html += ".btn-warning{background:#ffc107;color:black;}";
  html += ".btn-secondary{background:#6c757d;color:white;}";
  html += "table{width:100%;border-collapse:collapse;}";
  html += "th,td{padding:8px;text-align:left;border-bottom:1px solid #ddd;}";
  html += "tr:hover{background:#f5f5f5;}";
  html += ".refresh-info{background:#e7f3ff;padding:8px;border-radius:5px;margin:10px 0;font-size:14px;}";
  html += "</style></head><body>";
  html += "<h1>📅 Feeding Schedule</h1>";
  
  html += "<div class='refresh-info'>";
  html += "🔄 Page auto-refreshes every 30 seconds";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>Current Schedule</h3>";
  
  if (feedingSchedule.empty()) {
    html += "<p>No feeding times scheduled.</p>";
  } else {
    html += "<table>";
    html += "<tr><th>Time</th><th>Status</th><th>Fed Today</th><th>Actions</th></tr>";
    for (size_t i = 0; i < feedingSchedule.size(); i++) {
      String timeStr = String(feedingSchedule[i].hour) + ":" + 
                      (feedingSchedule[i].minute < 10 ? "0" + String(feedingSchedule[i].minute) : String(feedingSchedule[i].minute));
      html += "<tr>";
      html += "<td><b>" + timeStr + "</b></td>";
      html += "<td>" + String(feedingSchedule[i].enabled ? "✅ Enabled" : "❌ Disabled") + "</td>";
      html += "<td>" + String(feedingSchedule[i].alreadyFed ? "Yes" : "No") + "</td>";
      html += "<td>";
      html += "<a href=\"/toggle-time?id=" + String(i) + "\"><button class=\"btn btn-sm " + 
              String(feedingSchedule[i].enabled ? "btn-warning" : "btn-success") + "\">" +
              String(feedingSchedule[i].enabled ? "Disable" : "Enable") + "</button></a>";
      html += "<a href=\"/delete-time?id=" + String(i) + "\"><button class=\"btn btn-sm btn-danger\">Delete</button></a>";
      html += "</td></tr>";
    }
    html += "</table>";
  }
  
  html += "<br><h3>Add New Feeding Time</h3>";
  html += "<form action='/add-time' method='POST'>";
  html += "<label>Hour (0-23): <input type='number' name='hour' min='0' max='23' required></label><br>";
  html += "<label>Minute (0-59): <input type='number' name='minute' min='0' max='59' required></label><br>";
  html += "<label><input type='checkbox' name='enabled' checked> Enabled</label><br><br>";
  html += "<button type='submit' class='btn btn-success'>Add Time</button>";
  html += "<a href='/'><button type='button' class='btn btn-secondary'>Back</button></a>";
  html += "</form>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// ================== FEEDING CALCULATION FUNCTIONS ==================

float calculateFeedDuration(float grams) {
  if (gramsPerSecond <= 0) return 0;
  return grams / gramsPerSecond;
}

// ================== SCHEDULE MANAGEMENT ==================

void addFeedingTime(int hour, int minute, bool enabled) {
  FeedingTime newFeeding;
  newFeeding.hour = hour;
  newFeeding.minute = minute;
  newFeeding.enabled = enabled;
  newFeeding.alreadyFed = false;
  feedingSchedule.push_back(newFeeding);
  Serial.print("Added feeding time: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.println(minute < 10 ? "0" + String(minute) : String(minute));
}

void removeFeedingTime(int index) {
  if (index >= 0 && index < feedingSchedule.size()) {
    Serial.print("Removed feeding time: ");
    Serial.print(feedingSchedule[index].hour);
    Serial.print(":");
    Serial.println(feedingSchedule[index].minute);
    feedingSchedule.erase(feedingSchedule.begin() + index);
  }
}

String getFeedingStatus() {
  String status;
  for (const auto& feeding : feedingSchedule) {
    status += String(feeding.hour) + ":" + 
             (feeding.minute < 10 ? "0" + String(feeding.minute) : String(feeding.minute)) +
             (feeding.enabled ? " [ON] " : " [OFF] ") +
             (feeding.alreadyFed ? "(fed)" : "(pending)") + "\\n";
  }
  return status;
}