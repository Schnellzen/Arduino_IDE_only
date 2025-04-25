#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <max6675.h>

// OLED Display Settings
#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);

// MAX6675 pins
#define MAX6675_CLK 14
#define MAX6675_MISO 12
#define MAX6675_CS 15

MAX6675 thermocouple(MAX6675_CLK, MAX6675_CS, MAX6675_MISO);

// Motorized Valve Control
#define LEAD_PIN 0     // GPIO 0 - Main power control
#define DIR_PIN 2      // GPIO 2 - Direction control
#define VALVE_TRAVEL_TIME 10000  // 10 seconds full travel

#define RelayLowOn //comment out this line if the rellay is high on

// Temperature Settings
#define TARGET_TEMP 90.0f       // Your 90°C target
#define TEMP_HYSTERESIS 2.0f    // ±2°C deadband
#define MIN_VALVE_POSITION 20.0f // 15% = fully closed
#define MAX_VALVE_POSITION 100.0f // 100% = fully open

// System Variables
float currentTemp = 0.0f;
float prevTemp = 0.0f;
float deltaTemp = 0.0f;
float valvePosition = 0.0f;      // 0-100% open
unsigned long valveActionStart = 0;
unsigned long valveActionStop = 0;
bool isValveMoving = false;
bool valveDirection = false;     // false=opening, true=closing

// Custom max/min functions for float comparisons
float float_max(float a, float b) { return (a > b) ? a : b; }
float float_min(float a, float b) { return (a < b) ? a : b; }

void setup() {
  Serial.begin(9600);
  // Initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  
  // Initialize Valve Control Pins
  pinMode(LEAD_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  // Display startup
  displayStartup();

  // Initialize valve to known closed position
  openValve(); 
  delay(VALVE_TRAVEL_TIME*1.5);
  stopValve();
  valvePosition = MAX_VALVE_POSITION;
  closeValve();
  delay(VALVE_TRAVEL_TIME/2);
  stopValve();  
  valvePosition = MAX_VALVE_POSITION/2;

  delay(2000);

  Serial.println("setup done");
}

void loop() {
  Serial.println("-----");
  prevTemp = currentTemp;
  currentTemp = readTemperature();
  
  controlValveBasedOnTemp(currentTemp,prevTemp);
  updateValvePosition();
  checkValveLimits();
  
  
  updateDisplay(currentTemp, valvePosition);
  delay(1000); // Main loop delay
}

float readTemperature() {
  float temp = thermocouple.readCelsius();
  if(isnan(temp)) {
    displayError();
    Serial.println("temp fail-0");
    return -1.0f;
  }
  return temp;
}

void controlValveBasedOnTemp(float currentTemp, float prevTemp) {
  if(currentTemp == -1.0f) {
  Serial.println("temp fail-1");
  return; // Skip if temperature reading failed
  }
  Serial.println("hehe-1");
  Serial.println(currentTemp);
  deltaTemp = (currentTemp - prevTemp)*2;
  Serial.println(deltaTemp);

  if(TARGET_TEMP - currentTemp > 10*TEMP_HYSTERESIS) {
      if(valvePosition > MIN_VALVE_POSITION) {
        closeValve();
        delay(500); //cek nanti 1
        stopValve();
        Serial.println("hehe delau");
      }
  }

    if(currentTemp + deltaTemp < TARGET_TEMP + TEMP_HYSTERESIS) {
      // Temperature too low - close valve (reduce flow)
      if(valvePosition > MIN_VALVE_POSITION) {
        closeValve(); 
        delay(500); //cek nanti
        stopValve();  
        Serial.println("hehe buka");
      }
    }

    else if (currentTemp + deltaTemp > TARGET_TEMP - TEMP_HYSTERESIS) {
      // Temperature too high - open valve (increase flow)
      if(valvePosition < MAX_VALVE_POSITION) {
        openValve();
        delay(500); //cek nanti
        stopValve();
      }
    }

}

void openValve() {
  if(valvePosition >= MAX_VALVE_POSITION) return;
  
  #ifdef RelayLowOn
    digitalWrite(DIR_PIN, HIGH);    // Set direction to OPEN
    digitalWrite(LEAD_PIN, LOW);  // Activate movement
  #else
    digitalWrite(DIR_PIN, LOW);    // Set direction to OPEN
    digitalWrite(LEAD_PIN, HIGH);  // Activate movement
  #endif

  valveActionStart = millis();
  isValveMoving = true;
  valveDirection = false;
  Serial.println("Opening valve...");

}

void closeValve() {
  if(valvePosition <= MIN_VALVE_POSITION) return;
  
  #ifdef RelayLowOn
    digitalWrite(DIR_PIN, LOW);    // Set direction to CLOSE
    digitalWrite(LEAD_PIN, LOW);  // Activate movement
  #else
    digitalWrite(DIR_PIN, HIGH);    // Set direction to CLOSE
    digitalWrite(LEAD_PIN, HIGH);  // Activate movement
  #endif

  valveActionStart = millis();
  Serial.println(valveActionStart);
  isValveMoving = true;
  valveDirection = true;
  Serial.println("Closing valve...");
}

void stopValve() {
  if(isValveMoving==false) return;
  #ifdef RelayLowOn
    digitalWrite(DIR_PIN, HIGH);    // Stop movement 
    digitalWrite(LEAD_PIN, HIGH);  
  #else
    digitalWrite(DIR_PIN, LOW); // Stop movement   
    digitalWrite(LEAD_PIN, LOW);  
  #endif
  
  isValveMoving = false;
  valveActionStop= millis();
  Serial.print("Valve stopped");
  Serial.println("%");
}

void updateValvePosition() {
  unsigned long elapsed = valveActionStop - valveActionStart;
  float positionChange = (elapsed / (VALVE_TRAVEL_TIME / 100.0f));
  Serial.println("masuk sini");
  if(valveDirection) { // Closing
    valvePosition = valvePosition - positionChange;
  } else { // Opening
    valvePosition = valvePosition + positionChange;
  }

  valveActionStart = valveActionStop;

  Serial.print("Position: ");
  Serial.println(valvePosition);
  // Auto-stop when reaching limits
  if((valveDirection && valvePosition <= MIN_VALVE_POSITION) || 
     (!valveDirection && valvePosition >= MAX_VALVE_POSITION)) {
    stopValve();
  }

}

void checkValveLimits() {
  if(isValveMoving && digitalRead(LEAD_PIN) == LOW) {
    // Movement stopped unexpectedly (likely by limit switch)
    isValveMoving = false;
    if(valveDirection) {
      valvePosition = MIN_VALVE_POSITION; // Fully closed
    } else {
      valvePosition = MAX_VALVE_POSITION; // Fully open
    }
    Serial.println("Valve stopped by limit switch");
  }
}

void updateDisplay(float temp, float position) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Header
  display.setCursor(0, 0);
  display.println("<FLOW CONTROL>");
  
  // Temperature
  display.setCursor(0, 13);
  display.print("Temp: ");
  display.print(temp, 1);
  display.print("C");
  
  // Target
  display.setCursor(0, 22);
  display.print("Target: ");
  display.print(TARGET_TEMP, 0);
  display.print("C");
  
  // Valve Status
  display.setCursor(0, 34);
  display.print("Valve: ");
  if(isValveMoving) {
    display.print(valveDirection ? "CLOSING " : "OPENING ");
    display.print(position, 0);
    display.print("%");
  } else {
    display.print(position, 0);
    display.print("% ");
    display.print(temp < (TARGET_TEMP - TEMP_HYSTERESIS) ? "(LOW)" : 
                  temp > (TARGET_TEMP + TEMP_HYSTERESIS) ? "(HIGH)" : "(OK)");
  }
  
  display.display();
}

void displayStartup() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("<SYSTEM START>");
  display.setCursor(0, 13);
  display.println("Target: 90C");
  display.setCursor(0, 22);
  display.println("Valve: INIT");
  display.setCursor(0, 34);
  display.println("Travel: 30s");
  display.display();
}

void displayError() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("!SENSOR ERROR!");
  display.setCursor(0, 13);
  display.println("Check Thermocouple");
  display.setCursor(0, 34);
  display.print("Valve: ");
  display.print(valvePosition, 0);
  display.print("% HOLD");
  display.display();
}