#include "max6675.h"
#include <Wire.h>
#include <SPI.h>
#include <FS.h>

// Shared CLK for all MAX6675s
const int thermoCLK = 4;

// Individual CS and DO pins for each thermocouple
const int thermo1_CS = 12, thermo1_DO = 13;
const int thermo2_CS = 27, thermo2_DO = 14;
const int thermo3_CS = 26, thermo3_DO = 25;
const int thermo4_CS = 33, thermo4_DO = 32;

// Initialize MAX6675s
MAX6675 thermocouple1(thermoCLK, thermo1_CS, thermo1_DO);
MAX6675 thermocouple2(thermoCLK, thermo2_CS, thermo2_DO);
MAX6675 thermocouple3(thermoCLK, thermo3_CS, thermo3_DO);
MAX6675 thermocouple4(thermoCLK, thermo4_CS, thermo4_DO);

float temp1;
float temp2;
float temp3;
float temp4;

// Variables for timing
unsigned long previousMillis = 0;
const long interval = 60*1000; // 1 minute in milliseconds
unsigned long minutesElapsed = 0;

void setup() {
  Serial.begin(9600);
  delay(500); // MAX6675 stabilization
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    minutesElapsed++;
    
    // Read temperatures
    temp1 = thermocouple1.readCelsius()-0.46;
    delay(250);
    temp2 = thermocouple2.readCelsius()-4.18;
    delay(250);
    temp3 = thermocouple3.readCelsius();
    delay(250);
    temp4 = thermocouple4.readCelsius();
    delay(250);
    
    // Print formatted data: [minutes],[temp1],[temp2],[temp3],[temp4]
    Serial.print(minutesElapsed); Serial.print("\t");
    Serial.print(temp1); Serial.print("\t");
    Serial.print(temp2); Serial.print("\t");
    Serial.print(temp3); Serial.print("\t");
    Serial.println(temp4);
  }
  
  // You can add other non-blocking code here if needed
}