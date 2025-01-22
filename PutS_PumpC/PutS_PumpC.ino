#include <DHT.h> 
#include <LCD_I2C.h> 
///plesee ilang
#define DHTTYPE DHT11 
#define DHTPIN 5
DHT dht(DHTPIN, DHTTYPE); 
int sensor_pin_1 = A0; // Soil Sensor input at Analog PIN A0 
int sensor_pin_2 = A1;
int output_value_1;
int output_value_2; 
int relayPin1 = 6;
int relayPin2 = 3; 
//float humid;
//float temp;

LCD_I2C lcd(0x27, 16, 2); 

//two point callib

#define callib //uncomment this for using your own calibration data (edit from line 22 to 31, replace it with you own data)

#ifdef callib //tjis value will only used if line 20 is uncommented
  #define mc1_raw_U 1024 //raw value when mc1_value_U is readed, for example, if the soil moisture is 0%, the raw value usually arround 1000
  #define mc1_raw_D 450
  #define mc1_value_U 0 //moisture content in percent
  #define mc1_value_D 100

  #define mc2_raw_U 1024
  #define mc2_raw_D 370
  #define mc2_value_U 0
  #define mc2_value_D 100

#else 
  #define mc1_raw_U 1024
  #define mc1_raw_D 450
  #define mc1_value_U 0
  #define mc1_value_D 100

  #define mc2_raw_U 1024
  #define mc2_raw_D 450
  #define mc2_value_U 0
  #define mc2_value_D 100
#endif

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
  output_value_1 = map(analogRead(sensor_pin_1),mc1_raw_U,mc1_raw_D,mc1_value_U,mc1_value_D); 
  if(output_value_1<50){
    digitalWrite(relayPin1, LOW); 
    if(output_value_1<2){
      digitalWrite(relayPin1, HIGH);
    }
  }
  else{
    digitalWrite(relayPin1, HIGH); 
  }

  output_value_2 = map(analogRead(sensor_pin_2),mc2_raw_U,mc2_raw_D,mc2_value_U,mc2_value_D); 
  if(output_value_2<50){
    digitalWrite(relayPin2, LOW); 
    if(output_value_2<2){
      digitalWrite(relayPin2, HIGH);
    }
  }
  else{
    digitalWrite(relayPin2, HIGH); 
  }

}

void mc_lcd(){
  lcd.clear();

  lcd.setCursor(0,0); 
  lcd.print("M1:"); 
  lcd.print(output_value_1); 
  lcd.print("% "); 

  lcd.setCursor(0,1); 
  if(output_value_1<50){ 
    lcd.print("P1 ON"); 
  }
  else { 
    lcd.print("P1 OFF");        
  }

  lcd.setCursor(8,0); // set placement for 2nd motor in LCD, since it 16char with 2 line, so set to (8,0)
  lcd.print("M2:"); 
  lcd.print(output_value_2); 
  lcd.print("% "); 

  lcd.setCursor(8,1); // set placement for 2nd motor in LCD
  if(output_value_2<50){ 
    digitalWrite(relayPin2, LOW); 
    lcd.print("P2 ON"); 
  }
  else { 
    digitalWrite(relayPin2, HIGH); 
    lcd.print("P2 OFF");        
  }

}

void setup(){ 
  lcd.begin(); 
  Serial.begin(9600); 
  lcd.backlight(); 
  pinMode(sensor_pin_1, INPUT); 
  pinMode(relayPin1, OUTPUT); 
  digitalWrite(relayPin1, HIGH);
  pinMode(sensor_pin_2, INPUT);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin2, HIGH);
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
