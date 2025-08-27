#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Wire.h>
#include <RTClib.h>
#include <Servo.h>

// ============= CONFIG WIFI =================
const char* ssid     = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASS";

// Web server at port 80
WebServer server(80);

// RTC
RTC_DS3231 rtc;

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

// Feeding schedule
int feedHours[3] = {8, 16, 0};
bool alreadyFed[3] = {false, false, false};

// Feeding settings
float gramsPerTurn = 20.0;
float feedAmountGrams = 60.0;
float gearRatio = 2.0;

void setup() {
  Serial.begin(115200);

  // RTC init
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Servo + Motor init
  feederServo.attach(servoPin);
  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW);

  // Level sensors
  pinMode(levelA, INPUT);
  pinMode(levelB, INPUT);
  pinMode(levelC, INPUT);

  // Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("Connected!");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  // mDNS
  if (MDNS.begin("fishfeeder")) {
    Serial.println("mDNS responder started: http://fishfeeder.local/");
  }

  // Web routes
  server.on("/", handleRoot);
  server.on("/feed", handleFeed);
  server.begin();
}

void loop() {
  server.handleClient();

  DateTime now = rtc.now();

  // Reset feeding flags daily
  if (now.hour() == 0 && now.minute() == 1) {
    for (int i = 0; i < 3; i++) alreadyFed[i] = false;
  }

  // Auto feeding
  for (int i = 0; i < 3; i++) {
    if (now.hour() == feedHours[i] && now.minute() == 0 && !alreadyFed[i]) {
      feedFish(feedAmountGrams);
      alreadyFed[i] = true;
    }
  }
}

// ================== HANDLERS ==================

void handleRoot() {
  DateTime now = rtc.now();

  String html = "<html><head><title>Fish Feeder</title></head><body>";
  html += "<h1>🐟 ESP32 Fish Feeder</h1>";
  html += "<p><b>Current Time:</b> " + String(now.timestamp()) + "</p>";

  // Feeding info
  html += "<p><b>Feeding Amount:</b> " + String(feedAmountGrams) + " g</p>";
  html += "<p><b>Grams per Turn:</b> " + String(gramsPerTurn) + " g</p>";
  html += "<p><b>Gear Ratio:</b> " + String(gearRatio) + "</p>";

  // Schedule
  html += "<p><b>Feeding Times:</b> ";
  for (int i = 0; i < 3; i++) {
    html += String(feedHours[i]) + ":00 ";
  }
  html += "</p>";

  // Level sensors
  html += "<p><b>Level Sensors:</b> A:" + String(digitalRead(levelA)) +
          " B:" + String(digitalRead(levelB)) +
          " C:" + String(digitalRead(levelC)) + "</p>";

  // Ultrasound
  long dist = readUltrasonic();
  html += "<p><b>Ultrasonic Distance:</b> " + String(dist) + " cm</p>";

  // Manual feed button
  html += "<p><a href=\"/feed\"><button>Feed Now</button></a></p>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleFeed() {
  feedFish(feedAmountGrams);
  server.sendHeader("Location", "/");
  server.send(303); // redirect
}

// =============== CORE FUNCTIONS =================

void feedFish(float grams) {
  Serial.print("Feeding: ");
  Serial.print(grams); Serial.println(" g");

  float feederTurns = grams / gramsPerTurn;
  float servoTurns = feederTurns * gearRatio;

  digitalWrite(motorPin, HIGH);

  for (int t = 0; t < servoTurns; t++) {
    for (int i = 0; i < 180; i += 10) {
      feederServo.write(i); delay(30);
    }
    for (int i = 180; i >= 0; i -= 10) {
      feederServo.write(i); delay(30);
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
  return duration * 0.034 / 2;
}
