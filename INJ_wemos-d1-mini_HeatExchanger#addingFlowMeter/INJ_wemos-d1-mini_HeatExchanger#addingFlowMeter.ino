//#define debug //uncomment this line when debugging

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
#define LEAD_PIN 0               // GPIO 0 - Main power control
#define DIR_PIN 2                // GPIO 2 - Direction control
#define VALVE_TRAVEL_TIME 10000  // 10 seconds full travel

//#define RelayLowOn  //comment out this line if the rellay is high on

// Temperature Settings
#define TARGET_TEMP 90.0f          // Your 90°C target
#define TEMP_HYSTERESIS 2.0f       // ±2°C deadband
#define MIN_VALVE_POSITION 20.0f   // 15% = fully closed
#define MAX_VALVE_POSITION 100.0f  // 100% = fully open

// Flow Meter
#define flowMeterPin 13 // borrowing MOSI pin from SPI

// System Variables
float currentTemp = 0.0f;
float tempOffset = -5.0f;
float prevTemp = 0.0f;
float deltaTemp = 0.0f;
float valvePosition = 0.0f;  // 0-100% open
unsigned long valveActionStart = 0;
unsigned long valveActionStop = 0;
bool isValveMoving = false;
bool valveDirection = false;  // false=opening, true=closing
uint64_t pulseCountTally = 0; //counting all pulse
float volPerPulse = 0.2144957248; // ml/pulse
int volPassed = 0; // total ammount of water have been passed in ml
float flowRate = 0;

// Timing Variables
long currentMillis = 0;
long previousMillis = 0;
int minDeltaTime = 1000;

// Interrupt Function and Var
volatile int pulseCount = 0;

void IRAM_ATTR pulseCounter(){
  pulseCount++;
}

// Custom max/min functions for float comparisons
float float_max(float a, float b) {return (a > b) ? a : b;}
float float_min(float a, float b) {return (a < b) ? a : b;}

void setup() {
  Serial.begin(9600);
  // Initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  // Initialize Valve Control Pins
  pinMode(LEAD_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  // Display startup
  displayStartup();
  display.display();

  // Flow Meter Startup
  pinMode(flowMeterPin, INPUT_PULLUP); 
  attachInterrupt(digitalPinToInterrupt(flowMeterPin), pulseCounter, FALLING);

  //Initialize valve to known closed position
  openValve();
  delay(VALVE_TRAVEL_TIME * 1.5);
  stopValve();
  valvePosition = MAX_VALVE_POSITION;
  closeValve();
  delay(VALVE_TRAVEL_TIME / 2);
  stopValve();
  valvePosition = MAX_VALVE_POSITION / 2;

  delay(2000);

  Serial.println("setup done");
}

void loop() {
  #ifdef debug
    Serial.println("-----");
  #endif
  
  prevTemp = currentTemp;
  currentTemp = readTemperature();

  controlValveBasedOnTemp(currentTemp, prevTemp);

  updateValvePosition();

  checkValveLimits();

  flowMeter();

  display.clearDisplay();
  updateCompactDisplay(currentTemp, valvePosition);

  printDataStream();

  delay(400);
}

void printDataStream(){
  Serial.print(currentMillis);
  Serial.print(",");
  Serial.print(currentTemp);
  Serial.print(",");
  Serial.print(valvePosition);
  Serial.print(",");
  Serial.print(flowRate);
  Serial.print(",");
  Serial.print(volPassed);
  Serial.print("\n");  
}


void flowMeter(){
  currentMillis = millis();
  if (currentMillis - previousMillis > minDeltaTime){
    int pulseCountSeg;
    noInterrupts();
    pulseCountSeg = pulseCount;
    pulseCount = 0; //reset pulse count
    interrupts();

    flowRate = (pulseCountSeg * volPerPulse) * 1000 / (currentMillis - previousMillis); //flow in ml/sec
    previousMillis = millis();

    pulseCountTally = pulseCountTally + pulseCountSeg;

    volPassed = pulseCountTally * volPerPulse ;

  }

}

float readTemperature() {
  float temp = thermocouple.readCelsius() + tempOffset;
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
  deltaTemp = (currentTemp - prevTemp)*2;

  if(TARGET_TEMP - currentTemp > 10*TEMP_HYSTERESIS) {
    if(valvePosition > MIN_VALVE_POSITION) {
      closeValve();
      delay(500); //cek nanti 1
      stopValve();
    }
  }

  if(currentTemp + deltaTemp < TARGET_TEMP + TEMP_HYSTERESIS) {
    // Temperature too low - close valve (reduce flow)
    if(valvePosition > MIN_VALVE_POSITION) {
      closeValve();
      delay(500); //cek nanti
      stopValve();
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
  if (valvePosition >= MAX_VALVE_POSITION) return;

  #ifdef RelayLowOn
    digitalWrite(DIR_PIN, HIGH);  // Set direction to OPEN
    digitalWrite(LEAD_PIN, LOW);  // Activate movement
  #else
    digitalWrite(DIR_PIN, LOW);    // Set direction to OPEN
    digitalWrite(LEAD_PIN, HIGH);  // Activate movement
  #endif

  valveActionStart = millis();
  isValveMoving = true;
  valveDirection = false;
  //Serial.println("Opening valve...");

}

void closeValve() {
  if (valvePosition <= MIN_VALVE_POSITION) return;

  #ifdef RelayLowOn
    digitalWrite(DIR_PIN, LOW);   // Set direction to CLOSE
    digitalWrite(LEAD_PIN, LOW);  // Activate movement
  #else
    digitalWrite(DIR_PIN, HIGH);   // Set direction to CLOSE
    digitalWrite(LEAD_PIN, HIGH);  // Activate movement
  #endif

  valveActionStart = millis();
  isValveMoving = true;
  valveDirection = true;
  //Serial.println("Closing valve...");
}

void stopValve() {
  if (isValveMoving == false) return;
#ifdef RelayLowOn
  digitalWrite(DIR_PIN, HIGH);  // Stop movement
  digitalWrite(LEAD_PIN, HIGH);
#else
  digitalWrite(DIR_PIN, LOW); // Stop movement
  digitalWrite(LEAD_PIN, LOW);
#endif

  isValveMoving = false;
  valveActionStop = millis();
  //Serial.print("Valve stopped");
}

void updateValvePosition() {
  unsigned long elapsed = valveActionStop - valveActionStart;
  float positionChange = (elapsed / (VALVE_TRAVEL_TIME / 100.0f));

  #ifdef debug
    Serial.println("void updateValvePosition()");
  #endif

  if (valveDirection) {  // Closing
    valvePosition = valvePosition - positionChange;
  } else {  // Opening
    valvePosition = valvePosition + positionChange;
  }

  valveActionStart = valveActionStop;

  #ifdef debug
    Serial.print("Position: ");
    Serial.println(valvePosition);
  #endif

  // Auto-stop when reaching limits
  if((valveDirection && valvePosition <= MIN_VALVE_POSITION) || 
  (!valveDirection && valvePosition >= MAX_VALVE_POSITION)) {
    stopValve();
  }
}

void checkValveLimits() {
  if (isValveMoving && digitalRead(LEAD_PIN) == LOW) {
    // Movement stopped unexpectedly (likely by limit switch)
    isValveMoving = false;
    if (valveDirection) {
      valvePosition = MIN_VALVE_POSITION;  // Fully closed
    } else {
      valvePosition = MAX_VALVE_POSITION;  // Fully open
    }
    #ifdef debug
      Serial.println("Valve stopped by limit switch");
    #endif
    
  }
}

void updateCompactDisplay(float temp, float position) {
  #ifdef debug
  Serial.println("updateCompactDisplay");
  #endif

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Line 1: Header
  display.setCursor(0, 0);
  display.println("<INJ_HE>");

  // Line 2: Current Temperature
  display.setCursor(0, 10);
  display.print("Tmp:");
  display.print(temp, 1);
  display.print("C");
  display.display();

  // Line 3: Set Temperature
  display.setCursor(0, 20);
  display.print("Set:");
  display.print(TARGET_TEMP, 1);
  display.print("C");

  // Line 4: Valve Status
  display.setCursor(0, 30);
  display.print("VLV:");
  if (isValveMoving) {
    display.print(valveDirection ? "CL " : "OP ");
  } else {
    display.print("-- ");
  }
  display.print(position, 0);
  display.print("%");

  // Add small indicator if temp is out of range
  // if (temp < (TARGET_TEMP - TEMP_HYSTERESIS)) {
  //   display.print(" L");  // Low
  // } else if (temp > (TARGET_TEMP + TEMP_HYSTERESIS)) {
  //   display.print(" H");  // High
  // }

  // Line 5: Flow Meter
  display.setCursor(0, 40);  
  display.print(flowRate);
  display.setCursor(36, 40);
  display.print(volPassed);
  display.display();
}

void displayStartup() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("<INJ_HE>");
  display.setCursor(0, 10);
  display.println("Booting...");
  display.setCursor(0, 20);
  display.print("Set:");
  display.print(TARGET_TEMP, 1);
  display.print("C");
  display.setCursor(0, 30);
  display.println("Valve:INIT");

  display.display();
}

void displayError() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.println("<INJ_HE>");
  display.setCursor(0, 10);
  display.println("SENSOR ERROR");
  display.setCursor(0, 20);
  display.println("Check Thermocouple");
  display.setCursor(0, 30);
  display.print("Valve:");
  display.print(valvePosition, 0);
  display.print("% HOLD");

  display.display();
}