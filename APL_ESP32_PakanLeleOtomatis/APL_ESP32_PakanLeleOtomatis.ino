#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include <ESP32Servo.h>
#include <vector>

// ============= CONFIG WIFI =================
const char* ssid     = "PONDOK AGNIA 2";
const char* password = "01191998";

// Web server at port 80
WebServer server(80);

// RTC
RTC_DS3231 rtc;
bool rtcConnected = false;
bool useSoftwareClock = false;
unsigned long softwareClockMillis = 0;
DateTime softwareTime;

// Servo
Servo feederServo;
const int servoPin = 13;

// Motor
const int motorPin = 12;

// Level sensors
const int levelA = 33;
const int levelB = 32;
const int levelC = 25;

// Ultrasonic
const int trigPin = 15;
const int echoPin = 2;

// LED
const int WebAvailable = 17;
const int RTCAvailable = 16;
const int FishAvailable = 4;

// Feeding settings
float gramsPerTurn = 20.0;
float feedAmountGrams = 60.0;  // Default: 3 rotations (60g)
float gearRatio = 2.0;

// Dynamic feeding schedule
struct FeedingTime {
  int hour;
  int minute;
  bool enabled;
  bool alreadyFed;
};

std::vector<FeedingTime> feedingSchedule;
int nextFeedingId = 0;

// Function declarations
DateTime getCurrentTime();
void updateSoftwareClock();
void saveSettings();
void loadSettings();
void addFeedingTime(int hour, int minute, bool enabled = true);
void removeFeedingTime(int index);
String getFeedingStatus();
float calculateValidFeedAmount(float desiredAmount);
int calculateRotations(float grams);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Load saved settings
  loadSettings();

  // Ensure feed amount is valid multiple
  feedAmountGrams = calculateValidFeedAmount(feedAmountGrams);

  // Add some default feeding times if schedule is empty
  if (feedingSchedule.empty()) {
    addFeedingTime(8, 0);
    addFeedingTime(16, 0);
    addFeedingTime(20, 0);
  }

  // RTC init with error handling
  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("WARNING: Couldn't find RTC!");
    Serial.println("Switching to software clock mode");
    Serial.println("Time will be set to compile time");
    rtcConnected = false;
    useSoftwareClock = true;
    
    softwareTime = DateTime(F(__DATE__), F(__TIME__));
    softwareClockMillis = millis();
    Serial.print("Software time set to: ");
    Serial.println(softwareTime.timestamp());
  } else {
    rtcConnected = true;
    Serial.println("RTC found successfully!");
    
    if (rtc.lostPower()) {
      Serial.println("RTC lost power, setting time to compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  // Servo + Motor init
  feederServo.setPeriodHertz(50);
  feederServo.attach(servoPin, 500, 2400);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);

  // Level sensors
  pinMode(levelA, INPUT);
  pinMode(levelB, INPUT);
  pinMode(levelC, INPUT);

  // Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  digitalWrite(trigPin, LOW);

  // Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 200) {
    delay(500); 
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    
    if (MDNS.begin("fishfeeder")) {
      Serial.println("mDNS responder started: http://fishfeeder.local/");
    }
  } else {
    Serial.println("Failed to connect to WiFi!");
  }

  // Web routes - FIXED: Added the missing /update route handler
  server.on("/", handleRoot);
  server.on("/feed", handleFeed);
  server.on("/time", handleTime);
  server.on("/settings", handleSettings);
  server.on("/schedule", handleSchedule);
  server.on("/add-time", handleAddTime);
  server.on("/update", handleUpdateTime); // FIXED: Added this missing route
  server.on("/delete-time", handleDeleteTime);
  server.on("/toggle-time", handleToggleTime);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  if (useSoftwareClock) {
    updateSoftwareClock();
  }

  DateTime now = getCurrentTime();

  // Reset feeding flags daily
  if (now.hour() == 0 && now.minute() == 1) {
    for (auto& feeding : feedingSchedule) {
      feeding.alreadyFed = false;
    }
    Serial.println("Reset daily feeding flags");
  }

  // Auto feeding
  for (auto& feeding : feedingSchedule) {
    if (feeding.enabled && !feeding.alreadyFed && 
        now.hour() == feeding.hour && now.minute() == feeding.minute) {
      Serial.print("Auto feeding scheduled for ");
      Serial.print(feeding.hour);
      Serial.print(":");
      Serial.println(feeding.minute < 10 ? "0" + String(feeding.minute) : String(feeding.minute));
      feedFish(feedAmountGrams);
      feeding.alreadyFed = true;
    }
  }
  
  delay(1000);
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

// ================== FEEDING CALCULATION FUNCTIONS ==================

float calculateValidFeedAmount(float desiredAmount) {
  // Calculate the nearest multiple of gramsPerTurn
  int rotations = round(desiredAmount / gramsPerTurn);
  if (rotations < 1) rotations = 1;  // Minimum 1 rotation
  float validAmount = rotations * gramsPerTurn;
  
  Serial.print("Adjusted feed amount from ");
  Serial.print(desiredAmount);
  Serial.print("g to ");
  Serial.print(validAmount);
  Serial.print("g (");
  Serial.print(rotations);
  Serial.println(" rotations)");
  
  return validAmount;
}

int calculateRotations(float grams) {
  return round(grams / gramsPerTurn);
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

// ================== SETTINGS STORAGE ==================

void saveSettings() {
  Serial.println("=== SAVED SETTINGS ===");
  Serial.print("Feed Amount: "); Serial.println(feedAmountGrams);
  Serial.print("Grams per Turn: "); Serial.println(gramsPerTurn);
  Serial.print("Gear Ratio: "); Serial.println(gearRatio);
  Serial.print("Rotations per feed: "); Serial.println(calculateRotations(feedAmountGrams));
  Serial.println("Feeding Schedule:");
  for (const auto& feeding : feedingSchedule) {
    Serial.printf("  %02d:%02d %s %s\\n", 
                 feeding.hour, feeding.minute,
                 feeding.enabled ? "Enabled" : "Disabled",
                 feeding.alreadyFed ? "(fed)" : "");
  }
  Serial.println("======================");
}

void loadSettings() {
  Serial.println("Loaded default settings");
}

// ================== HANDLERS ==================

void handleRoot() {
  DateTime now = getCurrentTime();
  int rotations = calculateRotations(feedAmountGrams);

  String html = "<!DOCTYPE html><html><head><title>Fish Feeder</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
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
  html += ".schedule-item{margin:5px 0;padding:10px;background:#f8f9fa;border-radius:5px;}";
  html += "</style></head><body>";
  html += "<h1>🐟 ESP32 Fish Feeder</h1>";
  
  if (!rtcConnected) {
    html += "<div class='card warning'><h3>⚠️ RTC NOT CONNECTED</h3><p>Using software clock</p></div>";
  } else {
    html += "<div class='card success'><p>✅ RTC connected</p></div>";
  }
  
  html += "<div class='card'>";
  html += "<p><b>Current Time:</b> " + String(now.timestamp()) + "</p>";
  
  // Feeding info with rotation details
  html += "<div class='card info'>";
  html += "<p><b>Feeding Amount:</b> " + String(feedAmountGrams) + " g</p>";
  html += "<p><b>Rotations:</b> " + String(rotations) + " full rotations</p>";
  html += "<p><b>Grams per Rotation:</b> " + String(gramsPerTurn) + " g</p>";
  html += "</div>";

  // Schedule display
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

  // Control buttons
  html += "<p>";
  html += "<a href=\"/feed\"><button class=\"btn btn-primary\">Feed Now (" + String(feedAmountGrams) + "g)</button></a>";
  html += "<a href=\"/time\"><button class=\"btn btn-success\">Sync Time</button></a>";
  html += "<a href=\"/settings\"><button class=\"btn btn-warning\">Settings</button></a>";
  html += "<a href=\"/schedule\"><button class=\"btn btn-danger\">Manage Schedule</button></a>";
  html += "</p>";

  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

void handleSettings() {
  int currentRotations = calculateRotations(feedAmountGrams);
  int minRotations = 1;
  int maxRotations = 50;  // Maximum 50 rotations (1000g if 20g per rotation)

  String html = "<!DOCTYPE html><html><head><title>Fish Feeder - Settings</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".info{background:#d1ecf1;border-left:4px solid #17a2b8;color:#0c5460;padding:10px;border-radius:5px;}";
  html += "form{margin:15px 0;}label{display:block;margin:8px 0 3px 0;font-weight:bold;}";
  html += "input,select{padding:8px;margin:5px 0;width:200px;border:1px solid #ccc;border-radius:4px;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-secondary{background:#6c757d;color:white;}";
  html += "</style></head><body>";
  html += "<h1>⚙️ Fish Feeder Settings</h1>";
  
  html += "<div class='card'>";
  html += "<div class='info'>";
  html += "<p><b>Note:</b> Feeding amount must be multiples of " + String(gramsPerTurn) + "g</p>";
  html += "<p>Current: " + String(feedAmountGrams) + "g = " + String(currentRotations) + " rotations</p>";
  html += "</div>";
  
  html += "<form action='/update' method='POST'>";
  
  // Feeding Amount in rotations (instead of grams)
  html += "<label for='rotations'>Number of Rotations:</label>";
  html += "<input type='number' id='rotations' name='rotations' min='" + String(minRotations) + 
          "' max='" + String(maxRotations) + "' value='" + String(currentRotations) + "' required><br>";
  html += "<small>(" + String(minRotations) + " to " + String(maxRotations) + " rotations, " + 
          String(gramsPerTurn) + "g each)</small><br><br>";
  
  // Grams per Turn (read-only since it's mechanical)
  html += "<label for='gramsPerTurn'>Grams per Rotation (mechanical):</label>";
  html += "<input type='number' id='gramsPerTurn' name='gramsPerTurn' step='0.1' min='0.1' max='100' value='" + 
          String(gramsPerTurn) + "' readonly style='background:#f0f0f0;'><br>";
  html += "<small>This value depends on your mechanical setup</small><br><br>";
  
  // Gear Ratio (read-only since it's mechanical)
  html += "<label for='gearRatio'>Gear Ratio (mechanical):</label>";
  html += "<input type='number' id='gearRatio' name='gearRatio' step='0.1' min='0.1' max='10' value='" + 
          String(gearRatio) + "' readonly style='background:#f0f0f0;'><br>";
  html += "<small>This value depends on your gear setup</small><br><br>";
  
  html += "<button type='submit' class='btn btn-primary'>Save Settings</button>";
  html += "<a href='/'><button type='button' class='btn btn-secondary'>Back</button></a>";
  html += "</form>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", html);
}

// FIXED: Added the missing handleUpdate function that matches the form action
void handleUpdateTime() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("rotations")) {
      int rotations = server.arg("rotations").toInt();
      if (rotations >= 1 && rotations <= 50) {
        feedAmountGrams = rotations * gramsPerTurn;
        Serial.print("Updated feed amount to: ");
        Serial.print(feedAmountGrams);
        Serial.print("g (");
        Serial.print(rotations);
        Serial.println(" rotations)");
      }
    }
    
    // These are read-only in the form, but handle them anyway
    if (server.hasArg("gramsPerTurn")) gramsPerTurn = server.arg("gramsPerTurn").toFloat();
    if (server.hasArg("gearRatio")) gearRatio = server.arg("gearRatio").toFloat();
    
    saveSettings();
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
      saveSettings();
    }
  }
  server.sendHeader("Location", "/schedule");
  server.send(303);
}

void handleDeleteTime() {
  if (server.hasArg("id")) {
    int id = server.arg("id").toInt();
    removeFeedingTime(id);
    saveSettings();
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
      saveSettings();
    }
  }
  server.sendHeader("Location", "/schedule");
  server.send(303);
}

void handleSchedule() {
  String html = "<!DOCTYPE html><html><head><title>Fish Feeder - Schedule</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
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
  html += "tr:hover{background:#f5f5f5;}</style></head><body>";
  html += "<h1>📅 Feeding Schedule</h1>";
  
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

void handleFeed() {
  feedFish(feedAmountGrams);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleTime() {
  if (rtcConnected) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  } else {
    softwareTime = DateTime(F(__DATE__), F(__TIME__));
    softwareClockMillis = millis();
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// =============== CORE FUNCTIONS =================

void feedFish(float grams) {
  int rotations = calculateRotations(grams);
  Serial.print("Feeding: "); 
  Serial.print(grams); 
  Serial.print("g ("); 
  Serial.print(rotations); 
  Serial.println(" rotations)");
  
  float servoTurns = rotations * gearRatio;

  digitalWrite(motorPin, HIGH);
  for (int t = 0; t < (int)servoTurns; t++) {
    for (int i = 0; i <= 180; i += 10) feederServo.write(i), delay(30);
    for (int i = 180; i >= 0; i -= 10) feederServo.write(i), delay(30);
  }
  digitalWrite(motorPin, LOW);
  Serial.println("Feeding complete!");
}

long readUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  return duration == 0 ? -1 : duration * 0.034 / 2;
}