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

// Calibration value: milliliters per degree of rotation
// Example: If 1 full revolution (360°) = 1000 ml, then CALIBRATION = 1000/360 = 2.7778 ml/°
float CALIBRATION_ML_PER_DEGREE = 1.0; // CHANGE THIS TO YOUR ACTUAL CALIBRATION VALUE

// Wemos D1 Mini pins: D2 = SDA, D1 = SCL
const int SDA_PIN = D2;
const int SCL_PIN = D1;

// Moving average filter configuration
const int FILTER_SIZE = 8;  // Number of samples for moving average
float mlpsBuffer[FILTER_SIZE];  // Buffer for ml/s values
int bufferIndex = 0;
bool bufferFilled = false;

unsigned long lastTime = 0;
uint16_t lastAngle = 0;
float totalML = 0.0;        // total milliliters since power-up
float instantMLPS = 0.0;    // Instantaneous milliliters per second
float avgMLPS = 0.0;        // Moving average milliliters per second
bool firstSample = true;
bool displayCalibration = false;
unsigned long calibrationDisplayTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("AS5600 with OLED Display - Calibrated Output");
  Serial.print("Calibration: ");
  Serial.print(CALIBRATION_ML_PER_DEGREE, 3);
  Serial.println(" ml/°");
  
  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);        // Text size 1 = 8 pixels tall
  display.setTextColor(WHITE);
  
  // Show startup message on OLED
  display.setCursor(0, 0);
  display.println("Calibrating...");
  display.setCursor(0, 8);
  display.print("C:");
  display.print(CALIBRATION_ML_PER_DEGREE, 3);
  display.print("ml/°");
  display.display();
  delay(1500);
  
  // Initialize I2C for AS5600
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
  // Initialize filter buffer
  for (int i = 0; i < FILTER_SIZE; i++) {
    mlpsBuffer[i] = 0.0;
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
  
  // Show calibration value for 3 seconds
  displayCalibration = true;
  calibrationDisplayTime = millis();
  
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
  mlpsBuffer[bufferIndex] = newValue;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  
  // Mark buffer as filled after first complete cycle
  if (bufferIndex == 0) {
    bufferFilled = true;
  }
  
  // Calculate average
  float sum = 0.0;
  int samples = bufferFilled ? FILTER_SIZE : bufferIndex;
  
  for (int i = 0; i < samples; i++) {
    sum += mlpsBuffer[i];
  }
  
  return sum / samples;
}

String formatNumberWithFixedNegative(float value, bool isFlowRate = true) {
  String suffix = isFlowRate ? " ml/s" : " ml";
  String result;
  bool isNegative = value < 0.0;
  float absValue = fabs(value);
  
  // Format with max 2 decimal places
  if (absValue >= 1000.0) {
    result = String(absValue, 0);
  } else if (absValue >= 100.0) {
    result = String(absValue, 1);
  } else if (absValue >= 10.0) {
    result = String(absValue, 2);
  } else if (absValue >= 1.0) {
    result = String(absValue, 2);
  } else if (absValue >= 0.1) {
    result = String(absValue, 2);
  } else if (absValue >= 0.01) {
    result = String(absValue, 2);
  } else {
    result = "0.00";
  }
  
  // Add negative sign if needed (keeps consistent positioning)
  if (isNegative) {
    result = "-" + result;
  } else {
    // Add space for positive numbers to maintain same character count
    // This keeps the text from shifting left/right
    result = " " + result;
  }
  
  return result + suffix;
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  
  if (displayCalibration) {
    // Show calibration value
    display.setCursor(0, 0);
    display.println("CALIBRATION:");
    display.setCursor(0, 8);
    display.print("C:");
    display.print(CALIBRATION_ML_PER_DEGREE, 3);
    display.print("ml/°");
    
    // Check if we should switch back to normal display
    if (millis() - calibrationDisplayTime > 3000) {
      displayCalibration = false;
    }
  } else {
    // Normal display - flow rate and total volume
    
    // Format values with fixed negative sign positioning
    String flowStr = formatNumberWithFixedNegative(avgMLPS, true);
    String totalStr = formatNumberWithFixedNegative(totalML, false);
    
    // Center calculations (with consistent character width)
    int flowStrLength = flowStr.length();
    int flowX = (64 - (flowStrLength * 6)) / 2;
    
    int totalStrLength = totalStr.length();
    int totalX = (64 - (totalStrLength * 6)) / 2;
    
    // Display flow rate on top line
    display.setCursor(flowX, 16);  // Y = 16 (center of first 16 pixels)
    display.print(flowStr);
    
    // Display total volume on bottom line
    display.setCursor(totalX, 32);  // Y = 32 (center of second 16 pixels)
    display.print(totalStr);
    
    // Show calibration indicator (small C in corner)
    display.setCursor(0, 0);
    display.print("C");
    display.print(CALIBRATION_ML_PER_DEGREE, 2);
    display.print("ml/°");
  }
  
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
  } else if (delta < -2048) {
    delta += 4096;
  }
  
  // Convert raw delta to degrees delta
  float deltaDegrees = delta * 360.0f / FULL_COUNT;
  
  // Convert degrees to milliliters
  float deltaML = deltaDegrees * CALIBRATION_ML_PER_DEGREE;
  
  // Update total milliliters
  totalML += deltaML;
  
  // Calculate instantaneous milliliters per second
  if (elapsedSec > 0.0f) {
    instantMLPS = deltaML / elapsedSec;
  } else {
    instantMLPS = 0.0f;
  }
  
  // Update moving average filter
  avgMLPS = updateMovingAverage(instantMLPS);
  
  // Update OLED display
  updateOLED();
  
  // Serial output (for debugging)
  static unsigned long lastSerialUpdate = 0;
  if (now - lastSerialUpdate > 500) {
    float currentDegrees = angle * 360.0f / FULL_COUNT;
    Serial.print("Angle: ");
    Serial.print(currentDegrees, 1);
    Serial.print("°  Flow: ");
    
    // Format for serial with sign
    if (avgMLPS >= 0) {
      Serial.print(" ");
    }
    Serial.print(avgMLPS, 2);
    
    Serial.print(" ml/s  Total: ");
    if (totalML >= 0) {
      Serial.print(" ");
    }
    Serial.print(totalML, 2);
    
    Serial.print(" ml  Cal: ");
    Serial.print(CALIBRATION_ML_PER_DEGREE, 4);
    Serial.println(" ml/°");
    lastSerialUpdate = now;
  }
  
  lastAngle = angle;
  lastTime = now;
  
  delay(100); // 10 Hz sampling
}