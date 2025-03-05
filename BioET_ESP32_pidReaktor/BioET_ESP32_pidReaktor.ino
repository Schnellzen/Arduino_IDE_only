////////////////////////////////////////////////INCLUDE LIBRARY
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <max6675.h>
#include <PID_v1.h>

// Initialize the LCD library
LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display


////////////////////////////////////////////////DEFINE PIN
// Define the pins for MAX6675
int thermoDO = 19;
int thermoCS = 5;
int thermoCLK = 18;
// Initialize the MAX6675 library
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

// Define pin for PID Reactor output
const int outputPin = 4; // GPIO 4 for Reactor PID output (PWM)

//Define pin for PWM Boiler
const int pwmPinBoiler = 27;

// Define pin for potentiometer (Reactor & Boiler)
const int potPinReactor = 32; // GPIO 33 for potentiometer reactor
const int potPinBoiler = 33; // GPIO 32 for potentiometer boiler

// Define pin thermistor (Boiler & HE)
const int thermistorPinHE = 34; // GPIO 35 for thermistor--- Heat exchanger
const int thermistorPinBoiler = 35; // GPIO 35 for thermistor--- Boiler


//////////////////////////////////////DEFINE VARIABLE
// Thermistor parameters (HE & Boiler)
const int seriesResistor = 10000; // Series resistor value (10k ohms)
const float thermistorNominal = 10000; // Resistance at nominal temperature (10k ohms)
const float temperatureNominal = 25; // Nominal temperature (25°C)
const float betaCoefficient = 3950; // Beta coefficient of the thermistor
const float kelvinOffset = 273.15; // Kelvin to Celsius offset
const float vcc = 3.3;
float thermistorTempBoiler;
int setpointBoiler = 55;
int outputhigh = 255;
int outputkeep = 50;
float thermistorTempHE;

// PID Variables (Reactor)
double setpoint = 350.0; // Desired temperature in Celsius
double thermocoupleTemp = 0;     // Measured temperature from thermocouple
double output = 0;       // PID output (PWM value)
// PID Tuning Parameters
double Kp = 2.0; // Proportional gain
double Ki = 0.5; // Integral gain
double Kd = 1.0; // Derivative gain
// Initialize PID
PID myPID(&thermocoupleTemp, &output, &setpoint, Kp, Ki, Kd, DIRECT);


/////////////////////////////////VOID SETUP
void setup() {
  // Initialize the LCD
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("BioET Generator");

  Serial.begin(115200); // Initialize the serial communication

  //////  REACTOR
  // Wait for MAX6675 to stabilize
  delay(100);
  myPID.SetMode(AUTOMATIC); // Turn on the PID
  myPID.SetOutputLimits(0, 255); // Set PWM output limits (0-255)
  pinMode(outputPin, OUTPUT);   // Set the output pin as PWM
  pinMode(potPinReactor, INPUT);   // Set the potentiometer pin as input

  //////  BOILER 
  pinMode(potPinBoiler, INPUT); // Set the potentiometer pin as input
  pinMode(thermistorPinBoiler, INPUT); // Set the thermistor pin as input

  ////// HE
  pinMode(thermistorPinHE, INPUT);  // Set the thermistor pin as input
}

////////////////////////////////// VOID LOOP
void loop() {

  //////// REACTOR
  // Read the temperature from the thermocouple
  thermocoupleTemp = thermocouple.readCelsius();
  // Read the potentiometer value and map it to the setpoint range (100°C to 350°C)
  int potValueReactor = analogRead(potPinReactor);
  setpoint = map(potValueReactor, 4095, 0, 100, 350); // ESP32 ADC resolution is 12-bit (0-4095)
  // Compute PID output
  myPID.Compute();
  // Write the PID output to the output pin (PWM)
  analogWrite(outputPin, output);
  

  /////////////BOILER
  // Read the thermistor temperature
  thermistorTempBoiler = readThermistor(thermistorPinBoiler);
  // Read the potentiometer value and map it to the setpoint range (40°C to 90°C)
  int potValueBoiler = analogRead(potPinBoiler);
  setpointBoiler = map(potValueBoiler, 0, 4095, 40, 90); // ESP32 ADC resolution is 12-bit (0-4095)
  //Write the output to output pin Boiler
  outputBoiler();
 

  //////////////HE
  // Read the thermistor temperature
  thermistorTempHE = readThermistor(thermistorPinHE);


  ////////////////// LCD
  datashow();

  ///////////////// Serial Monitor
  Serial.print("Thermocouple Temp: ");
  Serial.print(thermocoupleTemp);
  Serial.print(" C, Setpoint: ");
  Serial.print(setpoint);
  Serial.print(" C, PID Output: ");
  Serial.print(output);
  Serial.print(", Boiler Thermistor Temp: ");
  Serial.print(thermistorTempBoiler);
  Serial.print(" C, HE Thermistor Temp: ");
  Serial.print(thermistorTempHE);
  Serial.println(" C");
  // Wait for a short time before the next reading
  delay(500);

}

///////////////////////////////////FUNCTION & VOID-VOIDAN

// Function Steinhart-Hart Equation
float readThermistor(const int thermistorPin){
  // Read the analog value from the thermistor
  int adcValue = analogRead(thermistorPin);
  if (adcValue == 0) return -999;  
  float voltage = adcValue * (vcc / 4095.0);
  float resistance = seriesResistor * ((vcc / voltage) - 1.0);
  float steinhart = log(resistance / thermistorNominal) / betaCoefficient;
  steinhart += 1.0 / (temperatureNominal + kelvinOffset);
  steinhart = 1.0 / steinhart;
  steinhart -= kelvinOffset; 

  return steinhart;
}

//Function to write analog to PWM pin Boiler
void outputBoiler(){
   if(setpointBoiler >= thermistorTempBoiler){
    analogWrite(pwmPinBoiler,outputkeep);
  } else {
    analogWrite(pwmPinBoiler,outputhigh);
  }
}

//Function to print PWM analog Boiler to LCD
void printBoiler(){
  if(thermistorTempBoiler >= setpointBoiler){
    lcd.print(outputkeep,0);
  } else {
    lcd.print(outputhigh,0);
  }
}

// Function to display data on the LCD
void datashow() {
  // Print the temperature, setpoint, and PID output in a single line

  //display Boiler
  lcd.setCursor(0,1);
  lcd.print("   T.B: ");
  lcd.print(thermistorTempBoiler,0);
  lcd.print("(");
  lcd.print(setpointBoiler,0);
  lcd.print(")[");
  printBoiler();
  lcd.print("]   ");

  //Display Reaktor
  lcd.setCursor(0,2);
  lcd.print("   T.R:");
  lcd.print(thermocoupleTemp,0);
  lcd.print("(");
  lcd.print(setpoint,0);
  lcd.print(")[");
  lcd.print(output,0);
  lcd.print("]   ");

  //Display Heat Exchanger
  lcd.setCursor(0,3);
  lcd.print("   T.HE:");
  lcd.print(thermistorTempHE,0);
  lcd.print(" C");
  }

  ///////////////////END 