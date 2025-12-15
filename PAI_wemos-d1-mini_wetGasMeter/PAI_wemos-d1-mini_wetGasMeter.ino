#include <Wire.h>

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
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
  // Initialize filter buffer
  for (int i = 0; i < FILTER_SIZE; i++) {
    dpsBuffer[i] = 0.0;
  }
  
  if (!probeAS5600()) {
    Serial.println("AS5600 not found! Check wiring.");
    while(1);
  }
  Serial.println("AS5600 OK - Output: Degrees/Second with Moving Average Filter");
  Serial.println("==============================================================");
  
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
    Serial.println("I2C read error");
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
  // 1 raw unit = 360/4096 = 0.087890625 degrees
  float deltaDegrees = delta * 360.0f / FULL_COUNT;
  
  // Calculate instantaneous degrees per second
  if (elapsedSec > 0.0f) {
    instantDPS = deltaDegrees / elapsedSec;
  } else {
    instantDPS = 0.0f;
  }
  
  // Update moving average filter
  avgDPS = updateMovingAverage(instantDPS);
  
  // Convert current raw angle to degrees (for display)
  float currentDegrees = angle * 360.0f / FULL_COUNT;
  
  // Output results with better formatting
  Serial.print("Angle: ");
  Serial.print(currentDegrees, 1);
  Serial.print("°  |  ");
  
  Serial.print("Speed: ");
  if (abs(instantDPS) < 0.1) {
    Serial.print("0.0");
  } else {
    Serial.print(instantDPS, 1);
  }
  Serial.print("°/s inst  |  ");
  
  Serial.print("Avg: ");
  if (abs(avgDPS) < 0.1) {
    Serial.print("0.0");
  } else {
    Serial.print(avgDPS, 1);
  }
  Serial.print("°/s  |  ");
  
  // Direction indicator
  Serial.print("Dir: ");
  if (avgDPS > 0.5) {
    Serial.print("FWD");
  } else if (avgDPS < -0.5) {
    Serial.print("REV");
  } else {
    Serial.print("STOP");
  }
  
  Serial.print("  |  Turns: ");
  Serial.print(totalTurns);
  
  Serial.println();
  
  lastAngle = angle;
  lastTime = now;
  
  delay(100); // 10 Hz sampling (adjust as needed)
}