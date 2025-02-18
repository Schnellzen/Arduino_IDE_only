#include <DHT.h> 
#include <LCD_I2C.h> 
///plesee ilang
#define DHTTYPE DHT11 
#define DHTPIN 5
DHT dht(DHTPIN, DHTTYPE); 
int sensor_pin_1 = A0; // Soil Sensor input at Analog PIN A0 
int sensor_pin_2 = A1; // Soil Sensor input at Analog PIN A1
int sensor_pin_3 = A2; // Soil Sensor input at Analog PIN A2
int sensor_pin_4 = A3; // Soil Sensor input at Analog PIN A3
int output_value_1, output_value_2, output_value_3, output_value_4;   //read for each analogpin
int m1, m2, m3, m4, p1, p2, p3, p4; // mn for moisture yang kebaca di sensor n, pn for persamaan untuk sensor n
//float humid;
//float temp;

LCD_I2C lcd(0x27, 16, 2); 

//two point callib

#define callib //uncomment this for using your own calibration data (edit from line 22 to 31, replace it with you own data)

//void-voidan 
// void dht_data(){ //uncomment this if only dht data update needed
//   float humid = dht.readHumidity(); 
//   float temp = dht.readTemperature();
// }

void dht_data_lcd(){
  float humid = dht.readHumidity(); 
  float temp = dht.readTemperature();
  lcd.clear();
  lcd.setCursor(0,0);  
  lcd.print("SUHU : "); 
  lcd.print(temp); 
  lcd.print((char)223); 
  lcd.print("C"); 
   
  lcd.setCursor(0,1);  
  lcd.print("Humidity: "); 
  lcd.print(humid); 
  lcd.print("%"); 
}

void mc_data_exc(){
  output_value_1 = analogRead(sensor_pin_1); 
  p1 = -3.34 + (0.0253 * output_value_1) + (-4.08e-5 * pow(output_value_1, 2)); // Corrected exponentiation 1
  m1 = p1*100-10;

  output_value_2 = analogRead(sensor_pin_2); 
  p2 = -3.34 + (0.0253 * output_value_2) + (-4.08e-5 * pow(output_value_2, 2)); // Corrected exponentiation 2
  m2 = p2*100-10;
  
  output_value_3 = analogRead(sensor_pin_3); 
  p3 = -3.34 + (0.0253 * output_value_3) + (-4.08e-5 * pow(output_value_3, 2)); // Corrected exponentiation 3
  m3 = p3*100-10;

  output_value_4 = analogRead(sensor_pin_4); 
  p4 = -3.34 + (0.0253 * output_value_4) + (-4.08e-5 * pow(output_value_4, 2)); // Corrected exponentiation 4
  m4 = p4*100-10;
}

void mc_lcd_1(){
  lcd.clear();

  lcd.setCursor(0,0); 
  lcd.print("M1:"); 
  lcd.print(m1); 
  lcd.print("% "); 

  lcd.setCursor(0,1); 
   lcd.print("M2:"); 
  lcd.print(m2); 
  lcd.print("% "); 

  lcd.setCursor(8,0); // set placement for M3 in LCD, since it 16char with 2 line, so set to (8,0)
  lcd.print("M3:"); 
  lcd.print(m3); 
  lcd.print("% "); 

  lcd.setCursor(8,1); // set placement for M3 in LCD, since it 16char with 2 line, so set to (8,0)
  lcd.print("M4:"); 
  lcd.print(m4); 
  lcd.print("% "); 

}

void setup(){ 
  lcd.begin(); 
  Serial.begin(9600); 
  lcd.backlight(); 
  pinMode(sensor_pin_1, INPUT); 
  pinMode(sensor_pin_2, INPUT);
  pinMode(sensor_pin_3, INPUT); 
  pinMode(sensor_pin_4, INPUT); 
  dht.begin();
} 
 
void loop(){ 
  mc_data_exc();
//  dht_data();


 if(millis()%2000==0){
    if(millis()%4000==0){
      mc_lcd();
    }
    else{
      dht_data_lcd();
    }
  }
//  delay(200);
}

