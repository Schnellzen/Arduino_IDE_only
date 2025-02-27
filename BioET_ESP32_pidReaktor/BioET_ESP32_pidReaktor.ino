#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <PID_v1.h>

// Define the pins for MAX6675
int thermoDO = 19;
int thermoCS = 5;
int thermoCLK = 18;

// Define the PID output pin
const int outputPin = 4; // GPIO 4 for PID output (PWM)

// Define the potentiometer pin
const int potPin = 32; // GPIO 34 for potentiometer (analog input)

// Define the thermistor pin
const int thermistorPin = 35; // GPIO 35 for thermistor (analog input)

// Thermistor parameters
const int seriesResistor = 10000; // Series resistor value (10k ohms)
const float thermistorNominal = 10000; // Resistance at nominal temperature (10k ohms)
const float temperatureNominal = 25; // Nominal temperature (25°C)
const float betaCoefficient = 3950; // Beta coefficient of the thermistor
const float kelvinOffset = 273.15; // Kelvin to Celsius offset

// Initialize the MAX6675 library
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// Initialize the LCD library
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display

// PID Variables
double setpoint = 350.0; // Desired temperature in Celsius
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

  // Set the thermistor pin as input
  pinMode(thermistorPin, INPUT);
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

  // Read the thermistor temperature
  float thermistorTemp = readThermistor();

  // Display data on the LCD
  datashow(thermistorTemp);

  // Print the temperature, setpoint, and PID output to the Serial Monitor
  Serial.print("Thermocouple Temp: ");
  Serial.print(input);
  Serial.print(" C, Setpoint: ");
  Serial.print(setpoint);
  Serial.print(" C, PID Output: ");
  Serial.print(output);
  Serial.print(", Thermistor Temp: ");
  Serial.print(thermistorTemp);
  Serial.println(" C");

  // Wait for a short time before the next reading
  delay(100);
}

// Function to read the thermistor temperature
float readThermistor() {
  // Read the analog value from the thermistor
  int adcValue = analogRead(thermistorPin);

  // Convert the analog value to resistance
  float resistance = (4095.0 / adcValue) - 1.0;
  resistance = seriesResistor / resistance;

  // Calculate temperature using the Steinhart-Hart equation
  float steinhart = log(resistance / thermistorNominal) / betaCoefficient;
  steinhart += 1.0 / (temperatureNominal + kelvinOffset);
  steinhart = 1.0 / steinhart;
  steinhart -= kelvinOffset; // Convert from Kelvin to Celsius

  return steinhart;
}

// Function to display data on the LCD
void datashow(float thermistorTemp) {
  // Print the thermocouple temperature, setpoint, and PID output in a single line
  lcd.setCursor(0, 1); // Second line of the LCD
  lcd.print("T: ");
  lcd.print(input,0);
  lcd.print(" (");
  lcd.print(setpoint,0);
  lcd.print(") [");
  lcd.print(output);
  lcd.print("]   "); // Clear any leftover characters

  // Print the thermistor temperature on the third line
  lcd.setCursor(0, 2); // Third line of the LCD
  lcd.print("Thermistor: ");
  lcd.print(thermistorTemp);
  lcd.print(" C   ");
}