  #include <Wire.h>
  #include <LiquidCrystal_I2C.h>
  #include <max6675.h>
  #include <HX711_ADC.h>

  #if defined(ESP8266)|| defined(ESP32) || defined(AVR)
  #include <EEPROM.h>
  #endif

  // Initialize the LCD library
  LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD address to 0x27 for a 20x4 display

  // --- Pin Definitions --- //
  #if defined(ESP8266)
  // Wemos D1 mini (ESP8266)
  // MAX6675
  #define thermoDO   12
  #define thermoCS   15
  #define thermoCLK  14
  // HX711
  #define DT_HX711   0
  #define SCK_HX711  2

  #elif defined(ESP32)
  // ESP32 DOIT
  // MAX6675
  #define thermoDO   14
  #define thermoCS   13
  #define thermoCLK  27
  // HX711
  #define DT_HX711   15
  #define SCK_HX711  2

  #else
  // Fallback (other Arduino boards)
  #define thermoDO   4
  #define thermoCS   5
  #define thermoCLK  6
  #define DT_HX711   7
  #define SCK_HX711  8
  #endif

  //HX711 constructor:
  HX711_ADC LoadCell(DT_HX711, SCK_HX711);
  const int calVal_calVal_eepromAdress = 0;
  unsigned long t = 0;

  // Initialize the MAX6675 library
  MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);
  float thermocoupleTemp;
  long force = 0;


  void setup() {
    Serial.begin(115200); delay(10); // Initialize the serial communication
    Serial.println();
    Serial.println("Starting...");

    // Initialize the LCD
    lcd.begin(); // !!!Kadang "init" bergantung lib
    lcd.backlight();
    lcd.setCursor(0, 0);

    // Init HX711
    float calibrationValue; // calibration value
    calibrationValue = 1.0; // uncomment this if you want to set this value in the sketch
  #if defined(ESP8266) || defined(ESP32)
    //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
  #endif
    //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch this value from eeprom

    LoadCell.begin();
    //LoadCell.setReverseOutput();
    unsigned long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
    boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
    LoadCell.start(stabilizingtime, _tare);
    if (LoadCell.getTareTimeoutFlag()) {
      Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    }
    else {
      LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
      Serial.println("Startup is complete");
    }
    while (!LoadCell.update());
    Serial.print("Calibration value: ");
    Serial.println(LoadCell.getCalFactor());
    Serial.print("HX711 measured conversion time ms: ");
    Serial.println(LoadCell.getConversionTime());
    Serial.print("HX711 measured sampling rate HZ: ");
    Serial.println(LoadCell.getSPS());
    Serial.print("HX711 measured settlingtime ms: ");
    Serial.println(LoadCell.getSettlingTime());
    Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
    if (LoadCell.getSPS() < 7) {
      Serial.println("!!Sampling rate is lower than specification, check MCU>HX711 wiring and pin designations");
    }
    else if (LoadCell.getSPS() > 100) {
      Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
    }
  }

  void loop() {
    //// Data Temp
    thermocoupleTemp = thermocouple.readCelsius();


    //// Data Force
    static boolean newDataReady = 0;
    const int serialPrintInterval = 500; //increase value to slow down serial print activity

    // check for new data/start next conversion:
    if (LoadCell.update()) {
      newDataReady = true;
      force = LoadCell.getData();
    }

    //// Data Log - get smoothed value from the dataset:
    if (newDataReady) {
      if (millis() > t + serialPrintInterval) {
        Serial.print(force);
        Serial.print("\t");
        if millis > creepStartTime
        Serial.println(thermocoupleTemp,0);
        newDataReady = 0;
        t = millis();
      }
    }

    //// Serial input
    // receive command from serial terminal, send 't' to initiate tare operation:
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') LoadCell.tareNoDelay();
    }

    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
      Serial.println("Tare complete");
    }

    //// LCD
    datashow();

    //// Serial Monitor Debugging
    // Serial.print("Thermocouple Temp: ");
    // Serial.print(thermocoupleTemp);
    // Serial.println(" C");

    // Serial.print("Berat: ");
    // Serial.print(force);
    // Serial.println(" g");

    //delay(4000);
  }

  // Function to display data on the LCD
  void datashow() {
    lcd.clear();
    // Print the temperature and weight
    lcd.print("Reaktor Bang Gusti");

    //Display Reaktor
    lcd.setCursor(0,1);
    lcd.print("   Suhu:");
    lcd.print(thermocoupleTemp);
    lcd.print(" C");
  
    //Display Timbangan
    lcd.setCursor(0,2);
    lcd.print("   Berat:");
    lcd.print(force,0);
    lcd.print (" g");

    }