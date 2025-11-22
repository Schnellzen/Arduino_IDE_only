#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <HX711_ADC.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif

// Bluetooth Serial (ESP32 only)
#if defined(ESP32)
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
#endif

// Initialize the LCD library
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display

// --- Pin Definitions --- //
#if defined(ESP8266)
// Wemos D1 mini (ESP8266)
#define thermoDO    12
#define thermoCS    15
#define thermoCLK   14
#define DT_HX711    0
#define SCK_HX711   2
#define NTC_PIN     A0

#elif defined(ESP32)
// ESP32 DOIT
#define thermoDO    14
#define thermoCS    13
#define thermoCLK   27
#define DT_HX711    15
#define SCK_HX711   2
#define NTC_PIN     33  // Your NTC analog pin

#else
// Fallback (other Arduino boards)
#define thermoDO    4
#define thermoCS    5
#define thermoCLK   6
#define DT_HX711    7
#define SCK_HX711   8
#define NTC_PIN     A0
#endif

// HX711 setup
HX711_ADC LoadCell(DT_HX711, SCK_HX711);
const int calVal_eepromAdress = 0;
unsigned long t = 0;

// MAX6675 setup
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
float thermocoupleTemp = 0.0;
float force = 0.0; // Changed from long to float

// NTC setup - CORRECTED FOR REVERSED WIRING
const float FIXED_RESISTOR = 10000.0;    // 10kΩ fixed resistor
const float RESISTANCE_AT_25 = 15000.0;   // 5.4kΩ at 25°C
const float BETA_VALUE = 3435.0;         // Typical beta value
const float NOMINAL_TEMP = 25.0;
const int ADC_RESOLUTION = 4095;
const float VREF = 3.3;
float ntcTemp = 0.0;
float ntcResistance = 0.0;
bool ntcWiringReversed = false;  // SET THIS TO TRUE FOR REVERSED WIRING

// Bluetooth device name
const char* btDeviceName = "ReaktorBangGusti";

// Tare flag to handle tare in main loop
bool tareRequested = false;

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Starting...");

  // Initialize Bluetooth Serial (ESP32 only)
  #if defined(ESP32)
  if (!SerialBT.begin(btDeviceName)) {
    Serial.println("Bluetooth failed to initialize!");
  } else {
    Serial.println("Bluetooth initialized successfully");
    Serial.print("Device name: ");
    Serial.println(btDeviceName);
  }
  #endif

  // Initialize LCD
  lcd.begin();
  lcd.backlight();

  // Initialize NTC pin
  analogReadResolution(12);
  #if defined(ESP32)
  analogSetAttenuation(ADC_11db);
  #endif

  // HX711 calibration
  float calibrationValue = 1.0; // ⚠️ Replace with your actual calibration value!
  
  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);

  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    #if defined(ESP32)
    SerialBT.println("Timeout, check MCU>HX711 wiring and pin designations");
    #endif
    while (1); // Halt if critical error
  } else {
    LoadCell.setCalFactor(calibrationValue);
    Serial.println("Startup is complete");
    #if defined(ESP32)
    SerialBT.println("Startup is complete");
    #endif
  }

  while (!LoadCell.update()); // Wait for first reading

  Serial.print("Calibration value: ");
  Serial.println(LoadCell.getCalFactor());
  Serial.print("HX711 conversion time (ms): ");
  Serial.println(LoadCell.getConversionTime());
  Serial.print("HX711 sampling rate (Hz): ");
  Serial.println(LoadCell.getSPS());
  Serial.print("HX711 settling time (ms): ");
  Serial.println(LoadCell.getSettlingTime());
  Serial.println("Note: Settling time may increase if you use delay()!");

  #if defined(ESP32)
  SerialBT.print("Calibration value: ");
  SerialBT.println(LoadCell.getCalFactor());
  SerialBT.print("HX711 sampling rate (Hz): ");
  SerialBT.println(LoadCell.getSPS());
  #endif

  if (LoadCell.getSPS() < 7) {
    Serial.println("!! Sampling rate too low – check wiring");
    #if defined(ESP32)
    SerialBT.println("!! Sampling rate too low – check wiring");
    #endif
  } else if (LoadCell.getSPS() > 100) {
    Serial.println("!! Sampling rate too high – check wiring");
    #if defined(ESP32)
    SerialBT.println("!! Sampling rate too high – check wiring");
    #endif
  }

  // Display NTC info
  Serial.println("NTC Sensor: 5.4kΩ at 25°C, Beta=3950");
  Serial.println("NTC Wiring: REVERSED (software corrected)");
  #if defined(ESP32)
  SerialBT.println("NTC Sensor: 5.4kΩ at 25°C, Beta=3950");
  SerialBT.println("NTC Wiring: REVERSED (software corrected)");
  #endif
}

void loop() {
  static unsigned long lastPrintTime = 0;
  static unsigned long lastNTCReadTime = 0;
  const unsigned long printInterval = 5000; // print every 5s
  const unsigned long ntcReadInterval = 1000; // read NTC every 1s
  const float alfa = 1;

  // Handle tare command first (before updating load cell)
  handleTareCommand();

  // --- continuously update HX711 filter ---
  if (LoadCell.update()) {
    force = (1-alfa) * force + alfa * LoadCell.getData(); // this runs many times per second
  }

  // --- Read NTC temperature (every 1 second) ---
  if (millis() - lastNTCReadTime >= ntcReadInterval) {
    lastNTCReadTime = millis();
    readNTCtemperature();
  }

  // Perform tare if requested (in main loop context)
  if (tareRequested) {
    performTare();
    tareRequested = false;
  }

  // --- read thermocouple and send data (every 5 seconds) ---
  if (millis() - lastPrintTime >= printInterval) {
    lastPrintTime = millis();
    thermocoupleTemp = thermocouple.readCelsius();
    if (isnan(thermocoupleTemp)) thermocoupleTemp = 0.0;

    // send data - now includes NTC temperature
    String dataString = String(force, 0) + "," + String(force/10, 1) + "," + String(thermocoupleTemp, 0) + "," + String(ntcTemp, 1);
    Serial.println(dataString);
    #if defined(ESP32)
    SerialBT.println(dataString);
    #endif
  }

  datashow();
}

// Function to read NTC temperature - CORRECTED FOR REVERSED WIRING
void readNTCtemperature() {
  int adcValue = analogRead(NTC_PIN);
  float voltage = (adcValue * VREF) / ADC_RESOLUTION;
  
  if (voltage > 0 && voltage < VREF) {
    if (ntcWiringReversed) {
      // CORRECTED CALCULATION FOR REVERSED WIRING
      // NTC is connected between VCC and ADC pin (fixed resistor to GND)
      ntcResistance = FIXED_RESISTOR * voltage / (VREF - voltage);
    } else {
      // Standard wiring: NTC between ADC pin and GND (fixed resistor to VCC)
      ntcResistance = FIXED_RESISTOR * (VREF / voltage - 1);
    }
    ntcTemp = calculateNTCtemperature(ntcResistance);
  }
}

// Function to calculate NTC temperature
float calculateNTCtemperature(float resistance) {
  if (resistance <= 0) return -273.15;
  
  float steinhart = resistance / RESISTANCE_AT_25;     // (R/Ro)
  steinhart = log(steinhart);                         // ln(R/Ro)
  steinhart /= BETA_VALUE;                            // 1/B * ln(R/Ro)
  steinhart += 1.0 / (NOMINAL_TEMP + 273.15);        // + (1/To)
  steinhart = 1.0 / steinhart;                       // Invert
  steinhart -= 273.15;                               // Convert to Celsius
  
  return steinhart;
}

// Function to handle tare commands from both Serial and Bluetooth
void handleTareCommand() {
  // Check USB Serial
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't' || inByte == 'T') {
      tareRequested = true;
      Serial.println("Tare requested...");
    }
    // Clear any remaining characters in the buffer
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
  
  // Check Bluetooth Serial (ESP32 only)
  #if defined(ESP32)
  if (SerialBT.available() > 0) {
    char inByte = SerialBT.read();
    if (inByte == 't' || inByte == 'T') {
      tareRequested = true;
      SerialBT.println("Tare requested...");
    }
    // Clear any remaining characters in the buffer
    while (SerialBT.available() > 0) {
      SerialBT.read();
    }
  }
  #endif
}

// Function to perform the tare operation
void performTare() {
  Serial.println("Performing tare...");
  #if defined(ESP32)
  SerialBT.println("Performing tare...");
  #endif
  
  // Simple and direct tare approach
  LoadCell.tare();
  delay(1000); // Give enough time for tare to complete
  
  // Force refresh
  LoadCell.update();
  force = LoadCell.getData();
  
  Serial.println("Tare complete");
  #if defined(ESP32)
  SerialBT.println("Tare complete");
  #endif
}

// Function to display data on the LCD without flicker
void datashow() {
  // Line 0: Title
  lcd.setCursor(0, 0);
  lcd.print("Reaktor Bang Gusti");

  // Line 1: Thermocouple Temperature
  lcd.setCursor(0, 1);
  lcd.print("TC:");
  lcd.print(thermocoupleTemp, 0);
  lcd.print("C NTC:");
  lcd.print(ntcTemp, 1);
  lcd.print("C  ");

  // Line 2: Weight
  lcd.setCursor(0, 2);
  lcd.print("Berat:");
  lcd.print(force, 0);
  lcd.print(" g  ");

  // Line 3: Status
  #if defined(ESP32)
  lcd.setCursor(0, 3);
  lcd.print("BT:");
  lcd.print(btDeviceName);
  if (ntcWiringReversed) {
    lcd.print("*R"); // Indicate reversed wiring correction
  }
  #else
  lcd.setCursor(0, 3);
  if (ntcWiringReversed) {
    lcd.print("NTC Reversed Corr");
  } else {
    lcd.print("Ready          ");
  }
  #endif
}