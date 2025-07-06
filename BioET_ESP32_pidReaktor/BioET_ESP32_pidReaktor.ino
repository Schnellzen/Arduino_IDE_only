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

//set PWM for PID control
const int PWM_CHANNEL = 0;    // ESP32 has 16 channels which can generate 16 independent waveforms
const int PWM_FREQ = 1;     // Official ESP32 example uses 5,000Hz, use 1 Hz since SSR is kinda slow
const int PWM_RESOLUTION = 8; // use standard 8 bits (8 bits, 0-255)

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
int setpointBoiler = 50;
int outputhigh = 120;
int outputkeep = 5; // the calc result actually 21.675 for the same effect of 70VAC set in dimmer, it say that this give 0.18g/min evaporation rate, need to recheck
float thermistorTempHE;

// PID Variables (Reactor)
double setpoint = 350.0; // Desired temperature in Celsius
double thermocoupleTemp = 0;     // Measured temperature from thermocouple
double output = 0;       // PID output (PWM value)
// PID Tuning Parameters
double Kp = 1.6;  // Reduced from 2.0
double Ki = 0.3;  // Reduced from 0.5
double Kd = 2.0;  // Increased from 1.0
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
  ledcAttach(outputPin, PWM_FREQ, PWM_RESOLUTION); // Set the output pin as PWM using custom freq, and res
  pinMode(potPinReactor, INPUT);   // Set the potentiometer pin as input

  //////  BOILER 
  ledcAttach(pwmPinBoiler, PWM_FREQ, PWM_RESOLUTION);
  //pinMode(pwmPinBoiler, OUTPUT); // Set PWM output limits (0-255)
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
  setpoint = map(potValueReactor, 4095, 0, 250, 500); // ESP32 ADC resolution is 12-bit (0-4095)
  // Compute PID output
  myPID.Compute();
  // Write the PID output to the output pin (PWM)
  analogWrite(outputPin, output);
  

  /////////////BOILER
  // Read the thermistor temperature
  thermistorTempBoiler = readThermistor(thermistorPinBoiler);
  // Read the potentiometer value and map it to the setpoint range (40°C to 90°C)
  int potValueBoiler = analogRead(potPinBoiler);
  setpointBoiler = map(potValueBoiler, 0, 4095, 30, 90); // ESP32 ADC resolution is 12-bit (0-4095)
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
   if(thermistorTempBoiler < setpointBoiler){
    analogWrite(pwmPinBoiler,outputhigh);
  } else {
    analogWrite(pwmPinBoiler,outputkeep);
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