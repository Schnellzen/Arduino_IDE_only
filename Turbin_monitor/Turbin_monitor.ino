#include <Arduino.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_INA219.h>
//#include "SdFat.h"
#include <SD.h>
//const int chipSelect = 4;
#define SD_CS_PIN SS

// by default
// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_INA219 ina219;
//SdFat SD;

#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  24
static const unsigned char PROGMEM logo16_glcd_bmp[] = //later on will be used for logo display
{ B00000000, B11000000, B11000000,
  B00000001, B11000000, B11000000,
  B00000001, B11000000, B11000000,
  B00000011, B11100000, B11000000,
  B11110011, B11100000, B11000000,
  B11111110, B11111000, B11000000,
  B01111110, B11111111, B11000000,
  B00110011, B10011111, B11000000,
  B00011111, B11111100, B11000000,
  B00001101, B01110000, B11000000,
  B00011011, B10100000, B11000000,
  B00111111, B11100000, B11000000,
  B00111111, B11110000, B11000000,
  B01111100, B11110000, B11000000,
  B01110000, B01110000, B11000000,
  B00000000, B00110000, B11000000, };

#if (SSD1306_LCDHEIGHT != 48) //checking if wrong oled lcd used, since 64x48 is kinda rarelly used nowdays
#error("Height incorrect, please fix Adafruit_SSD1306.h!"); 
#endif

unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long interval = 100; //interval between meassurement
float shuntvoltage = 0;
float busvoltage = 0;
float current_mA = 0;
float loadvoltage = 0;
float power = 0;
float energy = 0;

// SD Card
File TimeFile;
File VoltFile;
File CurFile;

//Showing data in display (oled 64x48)
void dataShow () {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(6,0);
  // if(loadvoltage<0){
  //   display.setCursor(0,0);
  // }
  display.print(loadvoltage);
  display.println(" V");

  display.setCursor(6,8);
  if(current_mA<0){ 
    display.setCursor(0,8); //so negative value won't budge number position
  }
  display.print(current_mA);
  display.println(" mA");

  display.setCursor(6,16);
  if(power<0){
    display.setCursor(0,16); //so negative value won't budge number position
  }
  display.print(power);
  display.println(" mW");

  display.setCursor(6,24);
  // if(energy<0){
  //   display.setCursor(0,24);
  // }  
  display.print(energy);
  display.println(" mWh");

  display.setCursor(6,40);
  display.print(millis()/1000);
  // display.setCursor(30,40);
  display.print(" s");
 
  display.display();
  delay(200);
}

//ina219 value gather
void ina219values() {
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  power = loadvoltage*current_mA;
  energy = energy + loadvoltage * current_mA / 3600;

//serial for debugging
  Serial.print("Bus Voltage:   "); Serial.print(busvoltage); Serial.println(" V");
  Serial.print("Shunt Voltage: "); Serial.print(shuntvoltage); Serial.println(" mV");
  Serial.print("Load Voltage:  "); Serial.print(loadvoltage); Serial.println(" V");
  Serial.print("Current:       "); Serial.print(current_mA); Serial.println(" mA");
  Serial.print("Power:         "); Serial.print(power); Serial.println(" mW");
  Serial.println(millis()/1000);
}

void dataLog(){
  TimeFile = SD.open("TIME.txt", FILE_WRITE);
    if (TimeFile) {
      TimeFile.println(currentMillis);
      TimeFile.close();
    }

    VoltFile = SD.open("VOLT.txt", FILE_WRITE);
    if (VoltFile) {
      VoltFile.println(loadvoltage);
      VoltFile.close();
    }

    CurFile = SD.open("CUR.txt", FILE_WRITE);
    if (CurFile) {
      CurFile.println(current_mA);
      CurFile.close();
    }
}

void setup()   {
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.clearDisplay();
  display.display();

  SD.begin(SD_CS_PIN);
  ina219.begin();

  // if (!SD.begin(SD_CS_PIN)) {
  //   Serial.println("initialization failed!");
  //   return;
  // }

  // if (! ina219.begin()) {// TADINYA ADA TANDA SQUIGLE NYAA TAPI DIDISABLE HAHAHA
  //   Serial.println("Failed to find INA219 chip");
  //   while (1) { delay(10); }
  // }

  // test display
  // display.clearDisplay();
  // display.drawBitmap(30, 16,  logo16_glcd_bmp, 24, 16, 1);
  // display.display();
  // delay(5000);
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    ina219values();
    dataShow();
    dataLog();
  }
}