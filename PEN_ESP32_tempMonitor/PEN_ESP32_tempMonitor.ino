#include "max6675.h"
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "DESKTOP-LU47COE 9850";
const char* password = "qweasdzxc123qwe";
bool wifiStatus = false; // for checking wifi 

String serverName = "http://akademik.che.itb.ac.id/ptp/api/simpan_data.php";

// for re- stating the variable for internet thingy
float nilai;
float nilai2;
float nilai3;
float nilai4;

// Shared CLK for all MAX6675s
const int thermoCLK = 4;

// Individual CS and DO pins for each thermocouple
const int thermo1_CS = 12, thermo1_DO = 13;  // Your original pins
const int thermo2_CS = 27, thermo2_DO = 14;  // New pins
const int thermo3_CS = 26, thermo3_DO = 25;  // New pins
const int thermo4_CS = 33, thermo4_DO = 32;  // New pins

// Initialize MAX6675s
MAX6675 thermocouple1(thermoCLK, thermo1_CS, thermo1_DO);
MAX6675 thermocouple2(thermoCLK, thermo2_CS, thermo2_DO);
MAX6675 thermocouple3(thermoCLK, thermo3_CS, thermo3_DO);
MAX6675 thermocouple4(thermoCLK, thermo4_CS, thermo4_DO);

  float temp1;
  float temp2;
  float temp3;
  float temp4;


void updateData(){
//Wifi Check
    if(wifiStatus == false){                        
      Serial.println("!net");  
    }

// internet thingy (1)
    nilai = 1,1;
    nilai2 = 1,1;
    nilai3 = 1,1;
    nilai4 = 1,1;

    if(WiFi.status()== WL_CONNECTED){
      wifiStatus=true;

      HTTPClient http;

      String serverPath = serverName + "?nilai=" + nilai + "&nilai2="+ nilai2 + "&nilai3=" + nilai3 + "&nilai4=" + nilai4 + "&nilai5=0&nilai6=0&nilai7=0&nilai8=0";
      
      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());
      
      // If you need Node-RED/server authentication, insert user and password below
      //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
      
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      // if (httpResponseCode>0) {
      //   Serial.print("HTTP Response code: ");
      //   Serial.println(httpResponseCode);
      //   String payload = http.getString();
      //   Serial.println(payload);
      // }
      // else {
      //   Serial.print("Error code: ");
      //   Serial.println(httpResponseCode);
      // }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
      wifiStatus = false;
      WiFi.begin(ssid, password); //reconnect Wifi
      
    } 
}

void setup() {
// General  
  Serial.begin(9600);
  delay(500); // MAX6675 stabilization

// Wifi 
  WiFi.begin(ssid, password);
  Serial.print("ssid: ");
  Serial.println(ssid);
  Serial.println("Connecting");

  while(wifiStatus == false) {

    if(WiFi.status() == WL_CONNECTED){
      wifiStatus = true;
    }

    Serial.print(".");

    if(millis()>=30*1000){    // wait for 30s, if wifi still unavailable, then continue
      wifiStatus = true;
    }


  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
   temp1 = thermocouple1.readCelsius();
  delay(250);
   temp2 = thermocouple2.readCelsius()-0.8;
  delay(250);
   temp3 = thermocouple3.readCelsius()-5;
  delay(250);
   temp4 = thermocouple4.readCelsius();
  delay(250);

  Serial.print(temp1); Serial.print(", ");
  Serial.print(temp2); Serial.print(", ");
  Serial.print(temp3); Serial.print(", ");
  Serial.println(temp4);
  updateData();

  //delay(1000); // MAX6675 requires ≥250ms between reads
}