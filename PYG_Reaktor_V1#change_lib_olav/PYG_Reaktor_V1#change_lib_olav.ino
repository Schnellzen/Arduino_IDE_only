#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <HX711_ADC.h>

#if defined(ESP8266) || defined(ESP32) || defined(AVR)
#include <EEPROM.h>
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

#elif defined(ESP32)
// ESP32 DOIT
#define thermoDO    14
#define thermoCS    13
#define thermoCLK   27
#define DT_HX711    15
#define SCK_HX711   2

#else
// Fallback (other Arduino boards)
#define thermoDO    4
#define thermoCS    5
#define thermoCLK   6
#define DT_HX711    7
#define SCK_HX711   8
#endif

// HX711 setup
HX711_ADC LoadCell(DT_HX711, SCK_HX711);
const int calVal_eepromAdress = 0;
unsigned long t = 0;

// MAX6675 setup
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
float thermocoupleTemp = 0.0;
float force = 0.0; // Changed from long to float

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println("Starting...");

  // Initialize LCD
  lcd.begin();
  lcd.backlight();

  // HX711 calibration
  float calibrationValue = 1.0; // ⚠️ Replace with your actual calibration value!
  
  // Uncomment below if using EEPROM (and write value once!)
  // #if defined(ESP8266) || defined(ESP32)
  //   EEPROM.begin(512);
  // #endif
  // EEPROM.get(calVal_eepromAdress, calibrationValue);

  LoadCell.begin();
  unsigned long stabilizingtime = 2000;
  boolean _tare = true;
  LoadCell.start(stabilizingtime, _tare);

  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    while (1); // Halt if critical error
  } else {
    LoadCell.setCalFactor(calibrationValue);
    Serial.println("Startup is complete");
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

  if (LoadCell.getSPS() < 7) {
    Serial.println("!! Sampling rate too low – check wiring");
  } else if (LoadCell.getSPS() > 100) {
    Serial.println("!! Sampling rate too high – check wiring");
  }
}

void loop() {
  static unsigned long lastSampleTime = 0;
  const unsigned long sampleInterval = 5000;

  if (millis() - lastSampleTime >= sampleInterval) {
    lastSampleTime = millis();

    //LoadCell.powerUp();
    delay(100);

    // Temporarily use fewer samples for faster single reading
    LoadCell.setSamplesInUse(32);  // Fast, but still filtered

    while (!LoadCell.update());
    force = LoadCell.getData();

    thermocoupleTemp = thermocouple.readCelsius();
    if (isnan(thermocoupleTemp)) thermocoupleTemp = 0.0;

    // TSV output (no units)
    Serial.print(force, 0);
    Serial.print(",");
    Serial.println(thermocoupleTemp, 0);

    //LoadCell.powerDown();
  }

  // Handle tare command
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell.powerUp();
      delay(100);
      LoadCell.tare(); // Uses default SAMPLES (e.g., 16) for accurate tare
      LoadCell.powerDown();
      Serial.println("Tare complete");
    }
  }

  datashow(); // Update LCD
}

// Function to display data on the LCD without flicker
void datashow() {
  // Line 0: Title
  lcd.setCursor(0, 0);
  lcd.print("Reaktor Bang Gusti");

  // Line 1: Temperature
  lcd.setCursor(0, 1);
  lcd.print("   Suhu:");
  lcd.print(thermocoupleTemp, 0); // No decimal places
  lcd.print(" C ");
  // Add trailing space to overwrite longer previous values (e.g., "123" → "12 ")

  // Line 2: Weight
  lcd.setCursor(0, 2);
  lcd.print("   Berat:");
  lcd.print(force, 0); // Display as integer grams
  lcd.print(" g ");
  // Trailing space helps clear old digits
}