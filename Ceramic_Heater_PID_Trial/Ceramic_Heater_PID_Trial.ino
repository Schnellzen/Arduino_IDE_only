//Board : LOLIN(WEMOS) D1 mini (clone)
#include <PID_v1.h>

// Pin Definitions
const int ThermistorPin = A0;  // Thermistor analog input
const int HeaterPWM = 3;       // Change to a PWM-supported pin

// PID Parameters
double Setpoint = 30;          // Desired temperature in Celsius
double Input, Output;
double Kp = 1, Ki = 2.0, Kd = 0.5; // PID tuning parameters

// Create PID object
PID myPID(&Input, &Output, &Setpoint, Kp, Ki, Kd, DIRECT);

// Thermistor Parameters
float R1 = 10000;              // Series resistor value
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

void setup() {
  Serial.begin(9600);

  // Initialize PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetSampleTime(500);  // Ensure PID updates at 500ms intervals

  // Set HeaterPWM as output
  pinMode(HeaterPWM, OUTPUT);
}

void loop() {
  // Read temperature
  Input = readThermistor();

  // Ensure valid ADC reading
  if (Input < -50 || Input > 150) {  // Sanity check
    Serial.println("Sensor Error!");
    return;
  }

  // Compute PID output
  myPID.Compute();

  // Write PWM output
  analogWrite(HeaterPWM, Output);

  // Debugging output
  Serial.print("Temperature: ");
  Serial.print(Input);
  Serial.print(" C, PWM Value: ");
  Serial.println(Output);

  delay(500);  // Match the sample time
}

// Function to read temperature from thermistor
float readThermistor() {
  int Vo = analogRead(ThermistorPin);
  
  if (Vo == 0) return -999;  // Prevent division by zero

  float R2 = R1 * (1023.0 / (float)Vo - 1.0);
  float logR2 = log(R2);
  float T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
  return T - 273.15; // Convert Kelvin to Celsius
}
