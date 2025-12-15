#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Configuration (64x48)
#define OLED_RESET 0  // GPIO0 for Wemos D1 Mini
#define OLED_ADDRESS 0x3C
Adafruit_SSD1306 display(OLED_RESET);

// AS5600 Configuration
const uint8_t AS5600_ADDR = 0x36;
const uint8_t RAW_ANGLE_REG = 0x0E;
const uint16_t FULL_COUNT = 4096; // 12-bit resolution

// Wemos D1 Mini pins: D2 = SDA, D1 = SCL
const int SDA_PIN = D2;
const int SCL_PIN = D1;

// Moving average filter configuration
const int FILTER_SIZE = 8;  // Number of samples for moving average
float dpsBuffer[FILTER_SIZE];
int bufferIndex = 0;
bool bufferFilled = false;

unsigned long lastTime = 0;
uint16_t lastAngle = 0;
int32_t totalTurns = 0;   // total revolutions since power-up
float instantDPS = 0.0;   // Instantaneous degrees per second
float avgDPS = 0.0;       // Moving average degrees per second
bool firstSample = true;

void setup() {
  Serial.begin(115200);
  Serial.println("AS5600 with OLED Display");
  
  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);        // Text size 1 = 8 pixels tall
  display.setTextColor(WHITE);
  
  // Show startup message on OLED
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();
  delay(500);
  
  // Initialize I2C for AS5600
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
  // Initialize filter buffer
  for (int i = 0; i < FILTER_SIZE; i++) {
    dpsBuffer[i] = 0.0;
  }
  
  // Check for AS5600
  if (!probeAS5600()) {
    Serial.println("AS5600 not found! Check wiring.");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ERROR");
    display.println("AS5600 not");
    display.println("found!");
    display.display();
    while(1);
  }
  
  Serial.println("AS5600 OK");
  
  // Show ready message
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("AS5600 Ready");
  display.display();
  delay(500);
  
  // Initial reading
  lastAngle = readRawAngle();
  lastTime = millis();
}

bool probeAS5600() {
  Wire.beginTransmission(AS5600_ADDR);
  return (Wire.endTransmission() == 0);
}

uint16_t readRawAngle() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(RAW_ANGLE_REG);
  if (Wire.endTransmission(false) != 0) {
    return 0;
  }
  Wire.requestFrom(AS5600_ADDR, (uint8_t)2);
  if (Wire.available() < 2) return 0;
  uint8_t hi = Wire.read();
  uint8_t lo = Wire.read();
  uint16_t raw = ((uint16_t)hi << 8) | lo;
  return raw & 0x0FFF; // 12-bit mask
}

float updateMovingAverage(float newValue) {
  // Add new value to buffer
  dpsBuffer[bufferIndex] = newValue;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  
  // Mark buffer as filled after first complete cycle
  if (bufferIndex == 0) {
    bufferFilled = true;
  }
  
  // Calculate average
  float sum = 0.0;
  int samples = bufferFilled ? FILTER_SIZE : bufferIndex;
  
  for (int i = 0; i < samples; i++) {
    sum += dpsBuffer[i];
  }
  
  return sum / samples;
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);  // Ensure text size is 1
  
  // Center the speed reading
  // Convert avgDPS to string with appropriate decimal places
  String speedStr;
  
  if (fabs(avgDPS) >= 100.0) {
    speedStr = String(avgDPS, 0);  // No decimals for ≥100
  } else if (fabs(avgDPS) >= 10.0) {
    speedStr = String(avgDPS, 1);  // 1 decimal for 10-99
  } else if (fabs(avgDPS) >= 1.0) {
    speedStr = String(avgDPS, 2);  // 2 decimals for 1-9.9
  } else {
    speedStr = String(avgDPS, 3);  // 3 decimals for <1
  }
  
  // Calculate position for speed (top line)
  // Each character is 6 pixels wide at textSize 1
  int speedStrLength = speedStr.length() + 3; // +3 for "°/s"
  int speedX = (64 - (speedStrLength * 6)) / 2;
  
  // Display speed on first line (8 pixels tall)
  display.setCursor(speedX, 8);  // Y = 8 (middle of first 16 pixels)
  display.print(speedStr);
  display.print(" ");
  display.print((char)247);  // Degree symbol
  display.print("/s");
  
  // Calculate position for turns (second line)
  String turnsStr = String(totalTurns);
  int turnsX = (64 - (turnsStr.length() * 6)) / 2;
  
  // Display turns on second line (8 pixels tall, starting at Y=24)
  display.setCursor(turnsX, 32);  // Y = 32 (middle of second 16 pixels)
  display.print(turnsStr);
  
  display.display();
}

void loop() {
  unsigned long now = millis();
  uint16_t angle = readRawAngle();
  float elapsedSec = (now - lastTime) / 1000.0f;
  
  if (firstSample) {
    lastAngle = angle;
    lastTime = now;
    firstSample = false;
    delay(50);
    return;
  }
  
  // Handle angle wrap (4095 -> 0)
  int delta = (int)angle - (int)lastAngle;
  if (delta > 2048) {
    delta -= 4096;
    totalTurns--; // crossed zero backward
  } else if (delta < -2048) {
    delta += 4096;
    totalTurns++; // crossed zero forward
  }
  
  // Convert raw delta to degrees delta
  float deltaDegrees = delta * 360.0f / FULL_COUNT;
  
  // Calculate instantaneous degrees per second
  if (elapsedSec > 0.0f) {
    instantDPS = deltaDegrees / elapsedSec;
  } else {
    instantDPS = 0.0f;
  }
  
  // Update moving average filter
  avgDPS = updateMovingAverage(instantDPS);
  
  // Update OLED display
  updateOLED();
  
  // Serial output (optional, for debugging)
  static unsigned long lastSerialUpdate = 0;
  if (now - lastSerialUpdate > 500) { // Update serial every 500ms
    float currentDegrees = angle * 360.0f / FULL_COUNT;
    Serial.print("Angle: ");
    Serial.print(currentDegrees, 1);
    Serial.print("°  Speed: ");
    Serial.print(avgDPS, 3);
    Serial.print("°/s  Turns: ");
    Serial.println(totalTurns);
    lastSerialUpdate = now;
  }
  
  lastAngle = angle;
  lastTime = now;
  
  delay(100); // 10 Hz sampling
}