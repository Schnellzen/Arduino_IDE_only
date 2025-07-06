#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <HX711.h>

// Initialize the LCD library
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display

// Define the pins for MAX6675
#define thermoDO 12
#define thermoCS 15
#define thermoCLK 14

// Define the pins 
#define DT_HX711 0
#define SCK_HX711 2
HX711 LoadCell;

// Initialize the MAX6675 library
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
float thermocoupleTemp;
float force;
void setup() {
  // Initialize the LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  LoadCell.begin(DT_HX711, SCK_HX711);
  LoadCell.set_scale(-9.76);
  LoadCell.tare();
  Serial.begin(115200); // Initialize the serial communication

}

void loop() {
  // Read the temperature from the thermocouple
  thermocoupleTemp = thermocouple.readCelsius();
  force = LoadCell.get_units() / 1000;
  ////////////////// LCD
  datashow();

  ///////////////// Serial Monitor
  Serial.print("Thermocouple Temp: ");
  Serial.print(thermocoupleTemp);
  Serial.println(" C");

  Serial.print("Berat: ");
  Serial.print(force);
  Serial.println(" kg");

  delay(500);
  lcd.clear();
}

// Function to display data on the LCD
void datashow() {
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
  lcd.print(force);
  lcd.print (" kg");

  }