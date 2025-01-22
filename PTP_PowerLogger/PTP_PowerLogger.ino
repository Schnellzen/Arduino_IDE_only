// DATA lOGGER PAK TIRTO

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

// section nanti taronya rapiin
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "DESKTOP-LU47COE 9850";
const char* password = "Wk<24126";
bool wifiStatus = false; // for checking wifi 

String serverName = "http://akademik.che.itb.ac.id/ptp/api/simpan_data.php";

File TimeFile;
File VoltFile;
File CurFile;

/*
Uncomment and set up if you want to use custom pins for the SPI communication
#define REASSIGN_PINS
int sck = -1;
int miso = -1;
int mosi = -1;
int cs = -1;
*/

//#include <Arduino.h>
/* 0- General */
int decimalPrecision = 2;                   // decimal places for all values shown in LED Display & Serial Monitor

/* 1- AC Current Measurement */

int currentAnalogInputPin = 34;             // Which pin to measure Current Value (A0 is reserved for LCD Display Shield Button function)
int calibrationPin = 35;                    // Which pin to calibrate offset middle value
float manualOffset = 3.00;                  // Key in value to manually offset the initial value
float mVperAmpValue = 12.5;                 // If using "Hall-Effect" Current Transformer, key in value using this formula: mVperAmp = maximum voltage range (in milli volt) / current rating of CT
                                            // For example, a 20A Hall-Effect Current Transformer rated at 20A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 20A = 31.25mV/A 
                                            // For example, a 50A Hall-Effect Current Transformer rated at 50A, 2.5V +/- 0.625V, mVperAmp will be 625 mV / 50A = 12.5 mV/A
float supplyVoltage = 3300;                 // Analog input pin maximum supply voltage, Arduino Uno or Mega is 5000mV while Arduino Nano or Node MCU is 3300mV
float offsetSampleRead = 0;                 /* to read the value of a sample for offset purpose later */
float currentSampleRead  = 0;               /* to read the value of a sample including currentOffset1 value*/
float currentLastSample  = 0;               /* to count time for each sample. Technically 1 milli second 1 sample is taken */
float currentSampleSum   = 0;               /* accumulation of sample readings */
float currentSampleCount = 0;               /* to count number of sample. */
float currentMean ;                         /* to calculate the average value from all samples, in analog values*/ 
float RMSCurrentMean ;                      /* square roof of currentMean, in analog values */   
float FinalRMSCurrent ;                     /* the final RMS current reading*/

// re- stating the variable for internet thingy
float nilai;
float nilai2;
float nilai3;
float nilai4;

/* 1.a- Voltage Measurement */
float Van = 0; 
        
        /* 2 - LCD Display  */
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C LCD(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
unsigned long startMicrosLCD;                   /* start counting time for LCD Display */
unsigned long currentMicrosLCD;                 /* current counting time for LCD Display */
const unsigned long periodLCD = 1000000;        // refresh every X seconds (in seconds) in LED Display. Default 1000000 = 1 second 

// Def def
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char *path) {
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char *path) {
  Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
  } else {
    Serial.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char *path) {
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2) {
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("File renamed");
  } else {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path) {
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path)) {
    Serial.println("File deleted");
  } else {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path) {
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file) {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len) {
      size_t toRead = len;
      if (toRead > 512) {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %lu ms\n", flen, end);
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++) {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %lu ms\n", 2048 * 512, end);
  file.close();
}


void dataLog(){
  TimeFile = SD.open("/TIME.txt", FILE_APPEND);
  if (!TimeFile) {
    Serial.println("TF Failed to open file for writing");
    return;
  }
  if (TimeFile.print(currentMicrosLCD/1000000)) {
    TimeFile.print("\n");
    Serial.print(currentMicrosLCD);
    Serial.println(" us File written");

    LCD.setCursor(13,3);    //mark in lcd
    LCD.print("TFnoted");
    
  } else {
    Serial.println("TF Write failed");

    LCD.setCursor(13,3);    //mark in lcd
    LCD.print("TF del");    
  }
  TimeFile.close();  


  VoltFile = SD.open("/VOLT.txt", FILE_APPEND);
  if (!VoltFile) {
    Serial.println("VF Failed to open file for writing");
    return;
  }
  if (VoltFile.print(Van,decimalPrecision)){
    VoltFile.print("\n");
    Serial.print(Van,decimalPrecision);
    Serial.println(" V File written");

    LCD.setCursor(13,1);    //mark in lcd
    LCD.print("VFnoted");

  } else {
    Serial.println("VF write failed");

    LCD.setCursor(13,1);    //mark in lcd
    LCD.print("VF del");  
  }
  VoltFile.close();

  CurFile = SD.open("/CUR.txt", FILE_APPEND);  // in case lu lupa, GARIS MIRING SEBELUM NAMA FILE ITU ADA ARTINYA
  if (!CurFile) {
    Serial.println("CF Failed to open file for Writing");
    return;
  }
  if (CurFile.print(FinalRMSCurrent,decimalPrecision)){
    CurFile.print("\n");
    Serial.print(FinalRMSCurrent,decimalPrecision);
    Serial.println(" A File written");

    LCD.setCursor(13,0);
    LCD.print("CFnoted");
  } else {
    Serial.println("CF Write failed");

    LCD.setCursor(13,0);    
    LCD.print("CF del");
  }
  CurFile.close();

}


void displaylcd(){
    LCD.clear();
    LCD.setCursor(0,0);                                                                               /* Set cursor to first colum 0 and second row 1  */
    LCD.print("I=");
    LCD.print(FinalRMSCurrent,decimalPrecision);                                                      /* display current value in LCD in first row  */
    LCD.setCursor(7,0);      
    LCD.print("A");

    LCD.setCursor(0,1);
    LCD.print("V=");
    LCD.print(Van,decimalPrecision);                                                      /* display current value in LCD in first row  */
    LCD.setCursor(7,1); 
    LCD.print("V");

    LCD.setCursor(0,2);
    LCD.print("P=");
    LCD.print(FinalRMSCurrent*Van);                                                      /* display current value in LCD in first row  */
    LCD.print("Watt");

    LCD.setCursor(0,3);
    LCD.print("t=");
    LCD.print(currentMicrosLCD/1000000);                                                      /* display current value in LCD in first row  */
//  LCD.setCursor(7,3);     
    LCD.print(" Sec");

//Wifi Check
    if(wifiStatus == false){
      LCD.setCursor(16,2);                         
      LCD.print("!net");  
    }

// internet thingy (1)
    nilai = FinalRMSCurrent,decimalPrecision;
    nilai2 = Van,decimalPrecision;
    nilai3 = FinalRMSCurrent*Van;
    nilai4 = currentMicrosLCD/1000000;

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
      
      if (httpResponseCode>0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
      wifiStatus = false;
      WiFi.begin(ssid, password); //reconnect Wifi
      
    } 
}


// end Def def 


void setup()                                              /*codes to run once */
{                                      
/* 0- General */

Serial.begin(9600);                               /* to display readings in Serial Monitor at 9600 baud rates */
  while (!Serial) {
    delay(10);
  }

/* 2 - LCD Display  */
LCD.begin();                      // initialize the lcd 
// Print a message to the LCD.
LCD.backlight();
//LCD.begin(16,2);                                  /* Tell Arduino that our LCD has 16 columns and 2 rows*/
LCD.setCursor(0,0);                               /* Set LCD to start with upper left corner of display*/  
startMicrosLCD = micros();                        /* Start counting time for LCD display*/

// SD CARD
// pinMode(pinCS, OUTPUT);

// if (SD.begin(pinCS))
// {
//   Serial.println("SD card is ready to use.");
// } else
// {
//   Serial.println("SD card initialization failed");
//   return;
// }

#ifdef REASSIGN_PINS
  SPI.begin(sck, miso, mosi, cs);
  if (!SD.begin(cs)) {

#else
  if (!SD.begin()) {
#endif
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

// SD CARD test
//   listDir(SD, "/", 0);
//   createDir(SD, "/mydir");
//   listDir(SD, "/", 0);
//   removeDir(SD, "/mydir");
//   listDir(SD, "/", 2);
//   writeFile(SD, "/hello.txt", "Hello ");
//   appendFile(SD, "/hello.txt", "World!\n");
//   readFile(SD, "/hello.txt");
//   deleteFile(SD, "/foo.txt");
//   renameFile(SD, "/hello.txt", "/foo.txt");
//   readFile(SD, "/foo.txt");
//   testFileIO(SD, "/test.txt");
//   Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
//   Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

// Wifi Section
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  
  while(wifiStatus == false) {

    if(WiFi.status() == WL_CONNECTED){
      wifiStatus = true;
    }

    LCD.setCursor(0,1);
    LCD.print(ssid);

    LCD.setCursor(0,0);                    // Set cursor to first colum 0 and second row 1  
    LCD.print("Connecting.");
    delay(200);
    LCD.setCursor(0,0);                         
    LCD.print("Connecting..");
    delay(200);
    LCD.setCursor(0,0);        
    LCD.print("Connecting...");
    delay(200);
    LCD.clear();

    Serial.print(".");

    if(micros()>=30*1000000){    // wait for 30s, if wifi still unavailable, then continue
      wifiStatus = true;
    }


  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

}


void loop()                                                                                                   /*codes to run again and again */
{  
/* 1- AC & DC Current Measurement */

if(micros() >= currentLastSample + 200)                                                                /* every 0.2 milli second taking 1 reading */
  { 
    currentSampleRead = analogRead(currentAnalogInputPin)-analogRead(calibrationPin);                  /* read the sample value including offset value*/
    currentSampleSum = currentSampleSum + sq(currentSampleRead) ;                                      /* accumulate total analog values for each sample readings*/
    currentSampleCount = currentSampleCount + 1;                                                       /* to count and move on to the next following count */  
    currentLastSample = micros();                                                                      /* to reset the time again so that next cycle can start again*/ 
  }

if(currentSampleCount == 4000)                                                                        /* after 4000 count or 800 milli seconds (0.8 second), do this following codes*/
  { 
    currentMean = currentSampleSum/currentSampleCount;                                                /* average accumulated analog values*/
    RMSCurrentMean = sqrt(currentMean);                                                               /* square root of the average value*/
    FinalRMSCurrent = (((RMSCurrentMean /1023)* supplyVoltage) /mVperAmpValue)- manualOffset;          /* calculate the final RMS current*/
    if(FinalRMSCurrent <= (625/mVperAmpValue/100))                                                    /* if the current detected is less than or up to 1%, set current value to 0A*/
    { FinalRMSCurrent =0;
    }
    Serial.print(" The Current RMS value is: ");
    Serial.print(FinalRMSCurrent,decimalPrecision);
    Serial.println(" A ");
    currentSampleSum =0;                                                                              /* to reset accumulate sample values for the next cycle */
    currentSampleCount=0;                                                                             /* to reset number of sample for the next cycle */
  }

/* 1.a- Voltage Measurement */
Van = analogRead(4)*0.0187 + 2.17;

/* 2 - LCD Display  */

currentMicrosLCD = micros();                                                                          /* Set counting time for LCD Display*/
if (currentMicrosLCD - startMicrosLCD >= periodLCD)                                                   /* for every x seconds, run the codes below*/
  {
    displaylcd();
    dataLog();   //datalog sharing the same refresh rate -- bisa dibikin beda, tapi nanti deh
//    LCD.setCursor(0,1); 
//    LCD.print("                   ");                                                                 /* display nothing in LCD in second row */
    startMicrosLCD = currentMicrosLCD ;                                                               /* Set the starting point again for next counting time */
  }   

/* 3 - Data Save  */
//dataLog(); //pindahin ke dalam lcd display

}