#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <HX711.h>

// Initialize the LCD library
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display

// --- Pin Definitions --- //
#if defined(ESP8266)
// Wemos D1 mini (ESP8266)
// MAX6675
#define thermoDO   12
#define thermoCS   15
#define thermoCLK  14
// HX711
#define DT_HX711   0
#define SCK_HX711  2

#elif defined(ESP32)
// ESP32 DOIT
// MAX6675
#define thermoDO   14
#define thermoCS   13
#define thermoCLK  27
// HX711
#define DT_HX711   15
#define SCK_HX711  2

#else
// Fallback (other Arduino boards)
#define thermoDO   4
#define thermoCS   5
#define thermoCLK  6
#define DT_HX711   7
#define SCK_HX711  8
#endif

HX711 LoadCell;

// Initialize the MAX6675 library
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
float thermocoupleTemp;
long force = 0;
void setup() {
  // Initialize the LCD
  lcd.begin(); // !!!Kadang "init" bergantung lib
  lcd.backlight();
  lcd.setCursor(0, 0);
  LoadCell.begin(DT_HX711, SCK_HX711,128);
  LoadCell.set_scale(); //397.5f itu val sebelumnya buat load cell 5kg
  LoadCell.tare();
  Serial.begin(115200); // Initialize the serial communication
  Serial.print("haha");
}

void loop() {
  // Read the temperature from the thermocouple
  thermocoupleTemp = thermocouple.readCelsius();

  // LoadCell.power_down();			        // put the ADC in sleep mode
  delay(200);
  // LoadCell.power_up();

  force = LoadCell.get_units(5);
  // force = map(force,670261,676119,0,276);
  //force = force/30;

  //// Data logger 
  Serial.print(force);
  Serial.print("\t");
  Serial.println(thermocoupleTemp,0);

  //// LCD
  datashow();

  //// Serial Monitor Debugging
  // Serial.print("Thermocouple Temp: ");
  // Serial.print(thermocoupleTemp);
  // Serial.println(" C");

  // Serial.print("Berat: ");
  // Serial.print(force);
  // Serial.println(" g");

  //delay(4000);

}

// Function to display data on the LCD
void datashow() {
  lcd.clear();
  // Print the temperature and weight
  lcd.print("Reaktor Bang Gusti");

  //Display Reaktor
  lcd.setCursor(0,1);
  lcd.print("   Suhu:");
  lcd.print(thermocoupleTemp);
  lcd.print(" C");
 
  //Display Timbangan
  lcd.setCursor(0,2);
  lcd.print("   Berat:");
  lcd.print(force,0);
  lcd.print (" g");

  }