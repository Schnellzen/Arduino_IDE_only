#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HX711.h>
#include <PID_v1.h>

// Pin Definitions
// Encoder pin
#define CLK_En 14
#define DT_En 16
#define SW_En 15

// HX711 load cell
#define DT_HX711 0
#define SCK_HX711 2
HX711 LoadCell;

// OLED Display
#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);

// Thermistor and Heater
const int ThermistorPin = A0;
const int HeaterPWM = 3;

// PID Parameters
double Setpoint = 30; // Desired temperature in Celsius
double Input, Output;
double Kp = 1, Ki = 2.0, Kd = 0.5;
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

// Thermistor Parameters
float R1 = 10000;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

// Variables
int currentStateCLK, lastStateCLK;
unsigned long lastButtonPress = 0;
int displayRefreshRate = 500;

void dataShow() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("<TEMP_CTRL>");

  // Display Force
  display.setCursor(0, 13);
  display.print("F=");
  display.setCursor(18, 13);
  float force = LoadCell.get_units() / 1000;
  if (force < 0) display.setCursor(12, 13);
  display.print(force, 0);
  display.setCursor(42, 13);
  display.print("kgF");

  // Display Temperature Setpoint Adjusted by Encoder
  display.setCursor(0, 22);
  display.print("Trgt: ");
  display.print(Setpoint, 0);
  display.print("C");

  // Display Current Temperature
  display.setCursor(0, 31);
  display.print("Temp: ");
  display.print(Input, 0);
  display.print("C");

  // Display PWM Output Value
  display.setCursor(0, 40);
  display.print("PWM: ");
  display.print(Output, 0);

  display.display();
}

void encoder() {
  currentStateCLK = digitalRead(CLK_En);
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
    if (digitalRead(DT_En) != currentStateCLK) {
      Setpoint -= 1; // Decrease temperature setpoint
    } else {
      Setpoint += 1; // Increase temperature setpoint
    }
  }
  lastStateCLK = currentStateCLK;
}

float readThermistor() {
  int Vo = analogRead(ThermistorPin);
  if (Vo == 0) return -999;
  
  //float R2 = R1 * (1023.0 / (float)Vo - 1.0); //normal wiring
  float R2 = R1 * ((float)Vo / (1023.0 - Vo)); // for reversed wiring
  
  float logR2 = log(R2);
  float T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
  return T - 273.15;
}

void setup() {
  Serial.begin(9600);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  LoadCell.begin(DT_HX711, SCK_HX711);
  LoadCell.set_scale(-9.76);
  LoadCell.tare();

  pinMode(CLK_En, INPUT);
  pinMode(DT_En, INPUT);
  pinMode(SW_En, INPUT_PULLUP);
  lastStateCLK = digitalRead(CLK_En);

  pinMode(HeaterPWM, OUTPUT);
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetSampleTime(500);

  delay(2000);
}

void loop() {
  encoder();
  Input = readThermistor();
  if (Input < -50 || Input > 150) {
    Serial.println("Sensor Error!");
    return;
  }

  myPID.Compute();
  analogWrite(HeaterPWM, Output);

  if (millis() % displayRefreshRate == 0) dataShow();

  delay(1);
}
