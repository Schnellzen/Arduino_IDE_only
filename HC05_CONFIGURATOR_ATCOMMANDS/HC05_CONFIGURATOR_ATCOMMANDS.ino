void setup() {
  pinMode(10, OUTPUT);    /* this pin will pull the HC-05 pin 34 (KEY pin) HIGH to switch module to AT mode */
  digitalWrite(10, HIGH); 

  Serial.begin(9600);
  Serial.println("Enter AT Commands:");
  Serial3.begin(9600);  // HC-05 default speed in AT command mode
}

void loop() {

//The code below allows for commands and messages to be sent from COMPUTER (serial monitor) -> HC-05
  if (Serial.available())           // Keep reading from Arduino Serial Monitor 
    Serial3.write(Serial.read());  // and send to HC-05

//The code below allows for commands and messages to be sent from HC-05 -> COMPUTER (serial monitor)    
  if (Serial3.available())         // Keep reading from HC-05 and send to Arduino 
    Serial.write(Serial3.read());  // Serial Monitor
}