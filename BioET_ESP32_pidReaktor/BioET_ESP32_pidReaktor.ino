#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <PID_v1.h>

// Define the pins for MAX6675
int thermoDO = 19;
int thermoCS = 23;
int thermoCLK = 18;

// Define the PID output pin
const int outputPin = 4; // GPIO 4 for PID output (PWM)

// Define the potentiometer pin
const int potPin = 34; // GPIO 34 for potentiometer (analog input)

// Initialize the MAX6675 library
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// Initialize the LCD library
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display

// PID Variables
double setpoint = 100.0; // Desired temperature in Celsius
double input = 0;        // Measured temperature from thermocouple
double output = 0;       // PID output (PWM value)

// PID Tuning Parameters
double Kp = 2.0; // Proportional gain
double Ki = 0.5; // Integral gain
double Kd = 1.0; // Derivative gain

// Initialize PID
PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

void setup() {
  // Initialize the LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("BioET Generator");

  // Initialize the serial communication
  Serial.begin(115200);

  // Wait for MAX6675 to stabilize
  delay(500);

  // Initialize PID
  myPID.SetMode(AUTOMATIC); // Turn on the PID
  myPID.SetOutputLimits(0, 255); // Set PWM output limits (0-255)

  // Set the output pin as PWM
  pinMode(outputPin, OUTPUT);

  // Set the potentiometer pin as input
  pinMode(potPin, INPUT);
}

void loop() {
  // Read the temperature from the thermocouple
  input = thermocouple.readCelsius();

  // Read the potentiometer value and map it to the setpoint range (100°C to 400°C)
  int potValue = analogRead(potPin);
  setpoint = map(potValue, 0, 4095, 100, 400); // ESP32 ADC resolution is 12-bit (0-4095)

  // Compute PID output
  myPID.Compute();

  // Write the PID output to the output pin (PWM)
  analogWrite(outputPin, output);

  // Display data on the LCD
  datashow();

  // Print the temperature, setpoint, and PID output to the Serial Monitor
  Serial.print("Temperature: ");
  Serial.print(input);
  Serial.print(" C, Setpoint: ");
  Serial.print(setpoint);
  Serial.print(" C, PID Output: ");
  Serial.println(output);

  // Wait for a short time before the next reading
  delay(100);
}

// Function to display data on the LCD
void datashow() {
  // Print the temperature, setpoint, and PID output in a single line
  lcd.setCursor(0, 1); // Second line of the LCD
  lcd.print("T: ");
  lcd.print(input,0);
  lcd.print(" (");
  lcd.print(setpoint,0);
  lcd.print(") [");
  lcd.print(output);
  lcd.print("]   "); // Clear any leftover characters
}