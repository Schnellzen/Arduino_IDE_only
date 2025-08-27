#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Wire.h>
#include "RTClib.h"
#include <ESP32Servo.h>

// ============= CONFIG WIFI =================
const char* ssid     = "xx";
const char* password = "xx";

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
const int servoPin = 27;

// Motor
const int motorPin = 26;

// Level sensors
const int levelA = 33;
const int levelB = 32;
const int levelC = 25;

// Ultrasonic
const int trigPin = 14;
const int echoPin = 12;

// Feeding settings (with defaults)
float gramsPerTurn = 20.0;
float feedAmountGrams = 60.0;
float gearRatio = 2.0;
int feedHours[3] = {8, 16, 0};
bool alreadyFed[3] = {false, false, false};

// Function declarations
DateTime getCurrentTime();
void updateSoftwareClock();
void saveSettings();
void loadSettings();

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Load saved settings
  loadSettings();

  // RTC init with error handling
  Serial.println("Initializing RTC...");
  if (!rtc.begin()) {
    Serial.println("⚠️ WARNING: Couldn't find RTC!");
    Serial.println("⚠️ Switching to software clock mode");
    Serial.println("⚠️ Time will be set to compile time");
    rtcConnected = false;
    useSoftwareClock = true;
    
    // Initialize software clock with compile time
    softwareTime = DateTime(F(__DATE__), F(__TIME__));
    softwareClockMillis = millis();
    Serial.print("Software time set to: ");
    Serial.println(softwareTime.timestamp());
  } else {
    rtcConnected = true;
    Serial.println("RTC found successfully!");
    
    // Only set time if RTC lost power
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
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500); 
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
    
    // mDNS
    if (MDNS.begin("fishfeeder")) {
      Serial.println("mDNS responder started: http://fishfeeder.local/");
    }
  } else {
    Serial.println("Failed to connect to WiFi!");
  }

  // Web routes
  server.on("/", handleRoot);
  server.on("/feed", handleFeed);
  server.on("/time", handleTime);
  server.on("/settings", handleSettings);
  server.on("/update", handleUpdate);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();

  // Update software clock if RTC is not available
  if (useSoftwareClock) {
    updateSoftwareClock();
  }

  DateTime now = getCurrentTime();

  // Reset feeding flags daily
  if (now.hour() == 0 && now.minute() == 1) {
    for (int i = 0; i < 3; i++) alreadyFed[i] = false;
    Serial.println("Reset daily feeding flags");
  }

  // Auto feeding
  for (int i = 0; i < 3; i++) {
    if (now.hour() == feedHours[i] && now.minute() == 0 && !alreadyFed[i]) {
      Serial.print("Auto feeding scheduled for ");
      Serial.print(feedHours[i]);
      Serial.println(":00");
      feedFish(feedAmountGrams);
      alreadyFed[i] = true;
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

// ================== SETTINGS STORAGE ==================

void saveSettings() {
  // In a real implementation, you'd save to EEPROM or SPIFFS
  // For now, we'll just print the settings
  Serial.println("=== SAVED SETTINGS ===");
  Serial.print("Feed Amount: "); Serial.println(feedAmountGrams);
  Serial.print("Grams per Turn: "); Serial.println(gramsPerTurn);
  Serial.print("Gear Ratio: "); Serial.println(gearRatio);
  Serial.print("Feed Times: ");
  for (int i = 0; i < 3; i++) {
    Serial.print(feedHours[i]); Serial.print(" ");
  }
  Serial.println();
  Serial.println("======================");
}

void loadSettings() {
  // In a real implementation, you'd load from EEPROM or SPIFFS
  // For now, we'll use default values
  Serial.println("Loaded default settings");
}

// ================== HANDLERS ==================

void handleRoot() {
  DateTime now = getCurrentTime();

  String html = "<html><head><title>Fish Feeder</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += ".warning{background:#fff3cd;border-left:4px solid #ffc107;color:#856404;}";
  html += ".success{background:#d4edda;border-left:4px solid #28a745;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-success{background:#28a745;color:white;}";
  html += ".btn-warning{background:#ffc107;color:black;}";
  html += "form{margin:10px 0;}label{display:block;margin:5px 0;}input{margin:5px 0;padding:8px;width:100px;}</style></head><body>";
  html += "<h1>Abid's ESP32 Fish Feeder</h1>";
  
  // RTC status indicator
  if (!rtcConnected) {
    html += "<div class='card warning'>";
    html += "<h3>!!! RTC NOT CONNECTED</h3>";
    html += "<p>Using software clock. Time may drift over time.</p>";
    html += "<p>Connect RTC for accurate timekeeping.</p>";
    html += "</div>";
  } else {
    html += "<div class='card success'>";
    html += "<p>✓✓✓ RTC connected and working</p>";
    html += "</div>";
  }
  
  html += "<div class='card'>";
  html += "<p><b>Current Time:</b> " + String(now.timestamp()) + "</p>";
  html += "<p><b>RTC Status:</b> " + String(rtcConnected ? "Connected" : "Disconnected") + "</p>";
  html += "<p><b>Clock Mode:</b> " + String(useSoftwareClock ? "Software" : "Hardware") + "</p>";

  // Feeding info
  html += "<p><b>Feeding Amount:</b> " + String(feedAmountGrams) + " g</p>";
  html += "<p><b>Grams per Turn:</b> " + String(gramsPerTurn) + " g</p>";
  html += "<p><b>Gear Ratio:</b> " + String(gearRatio) + "</p>";

  // Schedule
  html += "<p><b>Feeding Times:</b> ";
  for (int i = 0; i < 3; i++) {
    html += String(feedHours[i]) + ":00 ";
    html += alreadyFed[i] ? "(fed) " : "(pending) ";
  }
  html += "</p>";

  // Level sensors
  html += "<p><b>Level Sensors:</b> A:" + String(digitalRead(levelA)) +
          " B:" + String(digitalRead(levelB)) +
          " C:" + String(digitalRead(levelC)) + "</p>";

  // Ultrasound
  long dist = readUltrasonic();
  html += "<p><b>Ultrasonic Distance:</b> " + String(dist) + " cm</p>";

  // Control buttons
  html += "<p>";
  html += "<a href=\"/feed\"><button class=\"btn btn-primary\">Feed Now</button></a>";
  html += "<a href=\"/time\"><button class=\"btn btn-success\">Sync Time</button></a>";
  html += "<a href=\"/settings\"><button class=\"btn btn-warning\">Edit Settings</button></a>";
  html += "</p>";

  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleSettings() {
  String html = "<html><head><title>Fish Feeder - Settings</title>";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
  html += ".card{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1);margin:10px 0;}";
  html += "form{margin:15px 0;}label{display:block;margin:8px 0 3px 0;font-weight:bold;}";
  html += "input,select{padding:8px;margin:5px 0;width:200px;border:1px solid #ccc;border-radius:4px;}";
  html += ".btn{padding:10px 15px;font-size:16px;border:none;border-radius:5px;cursor:pointer;margin:5px;}";
  html += ".btn-primary{background:#007bff;color:white;}";
  html += ".btn-secondary{background:#6c757d;color:white;}</style></head><body>";
  html += "<h1>⚙️ Fish Feeder Settings</h1>";
  
  html += "<div class='card'>";
  html += "<form action='/update' method='POST'>";
  
  // Feeding Amount
  html += "<label for='amount'>Feeding Amount (grams):</label>";
  html += "<input type='number' id='amount' name='amount' step='0.1' min='1' max='1000' value='" + String(feedAmountGrams) + "' required><br>";
  
  // Grams per Turn
  html += "<label for='gramsPerTurn'>Grams per Turn:</label>";
  html += "<input type='number' id='gramsPerTurn' name='gramsPerTurn' step='0.1' min='0.1' max='100' value='" + String(gramsPerTurn) + "' required><br>";
  
  // Gear Ratio
  html += "<label for='gearRatio'>Gear Ratio:</label>";
  html += "<input type='number' id='gearRatio' name='gearRatio' step='0.1' min='0.1' max='10' value='" + String(gearRatio) + "' required><br>";
  
  // Feeding Times
  html += "<label>Feeding Times (24h format):</label><br>";
  for (int i = 0; i < 3; i++) {
    html += "<label for='time" + String(i) + "'>Time " + String(i+1) + ":</label>";
    html += "<input type='number' id='time" + String(i) + "' name='time" + String(i) + "' min='0' max='23' value='" + String(feedHours[i]) + "' required><br>";
  }
  
  html += "<br>";
  html += "<button type='submit' class='btn btn-primary'>Save Settings</button>";
  html += "<a href='/' style='text-decoration:none;'><button type='button' class='btn btn-secondary'>Cancel</button></a>";
  html += "</form>";
  html += "</div></body></html>";

  server.send(200, "text/html", html);
}

void handleUpdate() {
  if (server.method() == HTTP_POST) {
    // Update feeding amount
    if (server.hasArg("amount")) {
      feedAmountGrams = server.arg("amount").toFloat();
      Serial.print("Updated feed amount to: "); Serial.println(feedAmountGrams);
    }
    
    // Update grams per turn
    if (server.hasArg("gramsPerTurn")) {
      gramsPerTurn = server.arg("gramsPerTurn").toFloat();
      Serial.print("Updated grams per turn to: "); Serial.println(gramsPerTurn);
    }
    
    // Update gear ratio
    if (server.hasArg("gearRatio")) {
      gearRatio = server.arg("gearRatio").toFloat();
      Serial.print("Updated gear ratio to: "); Serial.println(gearRatio);
    }
    
    // Update feeding times
    for (int i = 0; i < 3; i++) {
      String argName = "time" + String(i);
      if (server.hasArg(argName)) {
        int newHour = server.arg(argName).toInt();
        if (newHour >= 0 && newHour <= 23) {
          feedHours[i] = newHour;
          Serial.print("Updated time "); Serial.print(i); 
          Serial.print(" to: "); Serial.println(newHour);
        }
      }
    }
    
    // Reset feeding flags since schedule changed
    for (int i = 0; i < 3; i++) alreadyFed[i] = false;
    
    // Save settings (in real implementation, save to EEPROM/SPIFFS)
    saveSettings();
    
    server.sendHeader("Location", "/");
    server.send(303);
  } else {
    server.send(400, "text/plain", "Invalid request");
  }
}

void handleFeed() {
  feedFish(feedAmountGrams);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleTime() {
  if (rtcConnected) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Serial.println("RTC time synchronized with compile time");
  } else {
    softwareTime = DateTime(F(__DATE__), F(__TIME__));
    softwareClockMillis = millis();
    Serial.println("Software clock synchronized with compile time");
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// =============== CORE FUNCTIONS =================

void feedFish(float grams) {
  Serial.print("Feeding: ");
  Serial.print(grams); Serial.println(" g");

  float feederTurns = grams / gramsPerTurn;
  float servoTurns = feederTurns * gearRatio;

  digitalWrite(motorPin, HIGH);

  for (int t = 0; t < (int)servoTurns; t++) {
    for (int i = 0; i <= 180; i += 10) {
      feederServo.write(i); 
      delay(30);
    }
    for (int i = 180; i >= 0; i -= 10) {
      feederServo.write(i); 
      delay(30);
    }
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
  if (duration == 0) {
    return -1;
  }
  return duration * 0.034 / 2;
}