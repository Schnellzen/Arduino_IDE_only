//program Etilen Generator
#include "max6675.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>


// Set the LCD address to 0x27 for a 16 chars and 4 line display
LiquidCrystal_I2C lcd(0x27, 16, 4);

//Inisialisasi pin thermocouple (Reaktor)
int thermoDO = 19;
int thermoCS = 5;
int thermoCLK = 16;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

//inisialisasi pin Thermistor (Boiler & Uap)
int ThermistoraPin = 13;
int ThermistorbPin = 14; 
int Voa, Vob;
float R1a = 9000, R1b = 9000;
float logR2a, R2a, tBoiler, logR2b, R2b, tUap, setBoiler;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

void setup() {
  Serial.begin(9600);
  Serial.println("Etilen Generator");
  // wait for MAX chip to stabilize
  delay(500);
  //inisialisasi pin relay dan SSR sebagai pin output
  pinMode(1, OUTPUT); //pin SSR (TX) Boiler
  pinMode(2, OUTPUT); //pin blue relay Boiler
  pinMode(18, OUTPUT); //PIN SSR (18) Reaktor
  pinMode(26, OUTPUT); //pin blue relay Reaktor

}

void loop() {
  setBoiler = analogRead(33);
  //set temperatur boiler
  Voa = analogRead(ThermistoraPin);
  R2a = R1a * (1023.0 / (float)Voa - 1.0);
  logR2a = log(R2a);
  tBoiler = (1.0 / (c1 + c2*logR2a + c3*logR2a*logR2a*logR2a));
  tBoiler = tBoiler - 273.15;
  
  //Kontrol Blue Relay Boiler
  if(tBoiler < 35){
    digitalWrite(2, LOW);
  }else{
    digitalWrite(2, HIGH);
  }

  //Kontrol SSR boiler
  if(tBoiler < 35){
    digitalWrite(1, HIGH);
  }else{
    digitalWrite(1, LOW);
  }

  //menampilkan suhu boiler di serial
  Serial.print("Temperature Boiler: "); 
  Serial.print(tBoiler);
  Serial.println(" C"); 
  
 // menampilkan suhu reaktor di serial
  Serial.print("Suhu Reaktor = "); 
  float tReactor = thermocouple.readCelsius();
  Serial.println(tReactor);

  //Kontrol blue relay reaktor 
  if(tReactor < 500.00){
    digitalWrite(18, LOW);
  } else {
    digitalWrite(18, HIGH);
  }  

  //Kontrol SSR reaktor 
  if(tReactor < 500.00){
    digitalWrite(26, HIGH);
  } else {
    digitalWrite(26, LOW);
  }  

  //set temperature uap
  Vob = analogRead(ThermistorbPin);
  R2b = R1b * (1023.0 / (float)Vob - 1.0);
  logR2b = log(R2b);
  tUap = (1.0 / (c1 + c2*logR2b + c3*logR2b*logR2b*logR2b));
  tUap = tUap - 273.15;

  //menampilkan suhu uap di serial
  Serial.print("Temperature Uap: "); 
  Serial.print(tUap);
  Serial.println(" C"); 
  
  // initialize the LCD
	lcd.begin();
  // Turn on the blacklight and print a message.
	lcd.backlight();

  //Menampilkan tiga nilai suhu di LCD (tBoiler, tReactor, tUap)
  lcd.setCursor(0,0);
  lcd.print("Etilen Generator");
  lcd.setCursor(0,1);
  lcd.print("T.Reactor (C): ");
  lcd.print(tReactor);
  lcd.setCursor(0,2);
  lcd.print("T.Boiler (C): ");
  lcd.print(tBoiler);
  lcd.setCursor(0,3);
  lcd.print("T.Uap (C): ");
  lcd.print(tUap);
 

  //6675 to update, you must delay AT LEAST 250ms between reads!
  //Data refresh setiap 250 ms
  delay(250);
}