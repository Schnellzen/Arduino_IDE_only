#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Pin Definitions
#define dirPin 12       // Stepper direction pin
#define stepPin 13      // Stepper step pin
#define CLK_En 14       // Encoder CLK pin
#define DT_En 16        // Encoder DT pin
#define SW_En 15        // Encoder switch pin

// OLED Display
#define OLED_RESET 0
Adafruit_SSD1306 display(OLED_RESET);

// Variables
int currentStateCLK, lastStateCLK;
unsigned long lastButtonPress = 0;
float rpm = 10;                // Initial RPM value
unsigned long stepDelay = 9;   // Delay between steps (calculated from RPM)
unsigned long stepsPerRevolution = 200 * 16; // Steps per revolution (200 steps/rev * 16 microsteps)
float totalRotations = 0;      // Total rotations completed

void encoder() {
  currentStateCLK = digitalRead(CLK_En);
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
    if (digitalRead(DT_En) != currentStateCLK) {
      rpm -= 1; // Decrease RPM
    } else {
      rpm += 1; // Increase RPM
    }
    // Constrain RPM to a reasonable range
    rpm = constrain(rpm, 1, 100);
  }
  lastStateCLK = currentStateCLK;

  // Update display only when the encoder button is pressed
  int btnState = digitalRead(SW_En);
  if (btnState == LOW) { // Debounce delay
    dataShow(); // Update the display
    //lastButtonPress = millis();
  }
}

void dataShow() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("<EBP 2025>");

  // Display RPM
  display.setCursor(0, 13);
  display.print("RPM: ");
  display.print(rpm, 1);

  // Display Total Rotations
  display.setCursor(0, 22);
  display.print("Rot: ");
  display.print(totalRotations, 2); // Display rotations with 2 decimal places

  display.display();
}

void setup() {
  Serial.begin(9600);

  // Initialize OLED display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  // Initialize stepper pins
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  digitalWrite(dirPin, LOW); // Set direction (LOW = forward)

  // Initialize encoder pins
  pinMode(CLK_En, INPUT);
  pinMode(DT_En, INPUT);
  pinMode(SW_En, INPUT_PULLUP);
  lastStateCLK = digitalRead(CLK_En);

  delay(2000);
}

void loop() {
  // Update RPM based on encoder input
  encoder();

  // Calculate step delay based on RPM
  stepDelay = 60000000 / (stepsPerRevolution * rpm); // Delay in microseconds

  // Step the motor
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(stepDelay / 2); // Half the delay for HIGH
  digitalWrite(stepPin, LOW);
  delayMicroseconds(stepDelay / 2); // Half the delay for LOW

  // Update total rotations
  totalRotations += 1.0 / stepsPerRevolution;
}