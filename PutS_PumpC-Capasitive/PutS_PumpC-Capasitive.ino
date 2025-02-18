float Moisture, SM = 0;
double x = 0;

void setup() {
  Serial.begin(9600);
}

void loop() {
  x = analogRead(A0);
  Moisture = -3.34 + (0.0253 * x) + (-4.08e-5 * pow(x, 2)); // Corrected exponentiation
  
  SM = Moisture*100-10;
  // if(SM < 0){
  //   SM = 0.00;
  //    Serial.println(SM);
  // }
  Serial.print(x);
  Serial.print("  ");
  Serial.println(SM);
  
  
  delay(1000); // Increased delay for stability
}
