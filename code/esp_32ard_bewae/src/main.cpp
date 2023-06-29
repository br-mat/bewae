////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This is the main source file for the irrigation system. It is responsible for monitoring all sensor values,
// handling the watering procedure, and communicating with the Raspberry Pi to get and send data.
//
// Dependencies:
// - Arduino.h
// - Wire.h
// - SPI.h
// - SD.h
// - SPIFFS.h
// - Adafruit_Sensor.h
// - Adafruit_BME280.h
// - WiFi.h
// - PubSubClient.h
// - ArduinoJson.h
// - connection.h
// - Helper.h
// - IrrigationController.h
// - SensorController.h
// - SwitchController.h
// - config.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Standard
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "SPIFFS.h"

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

//custom
#include <connection.h>
#include <Helper.h>
#include <IrrigationController.h>
#include <SensorController.h>
#include <SwitchController.h>
#include <config.h>

using namespace std;

#ifndef DEBUG
#define DEBUG 1
#endif

using namespace Helper;

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- GLOBAL VARIABLES AND DEFINITIONS --------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################


//important global variables
byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_;
unsigned long up_time = 0;    //general estimated uptime
unsigned long last_activation = 0; //timemark variable
bool thirsty = false; //marks if a watering cycle is finished
bool config = false;  //handle wireless configuration

bool post_DATA = true; //controlls if data is sent back to pi or not
bool msg_stop = false; //is config finished or not
bool sw6 = 1;  //system switch (ON/OFF) NOT IMPLEMENTED YET
bool sw0 = 1; //bewae switch (ON/OFF)
bool sw1 = 0; //water time override condition
bool sw2 = 0; //timetable override condition

Adafruit_BME280 bme; // use I2C interface

//limitations & watering & other
//group limits and watering (some hardcoded stuff)                          
//          type                  amount of plant/brushes    estimated water amount in liter per day (average hot +30*C)
//group 1 - große tom             5 brushes                  ~8l
//group 2 - Chilli, Paprika       5 brushes                  ~1.5l
//group 3 - Kräuter (trocken)     4 brushes                  ~0.2l
//group 4 - Hochbeet2             10 brushes                 ~4l
//group 5 - kleine tom            4 brushes                  ~3.5l
//group 6 - leer 4 brushes +3 small?        ~0.5l

//NEW IMPLEMENTATION
// The idea is to use the config file to controll the watering groups. This way 
// with sending the JSON data we can adjust and initialize new groups over the air.
// The switch conditions and timetables get reduced too as there is no need for handling outages of
// wifi controll as the controller will then use what is stored locally in the config file.

//IrrigationController
// No need for initialization as it will be loaded from config file


//timetable example
//                                             2523211917151311 9 7 5 3 1
//                                              | | | | | | | | | | | | |
//unsigned long int timetable_default = 0b00000000000000000000010000000000;
//                                               | | | | | | | | | | | | |
//                                              2422201816141210 8 6 4 2 0
unsigned long int timetable = 0; //initialize on default


//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SOME ADDITIONAL FUNCTIONS ---------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

// Initialise the WiFi and MQTT Client objects
WiFiClient wificlient;

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SETUP -----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

void setup() {
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set clock speed
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //setCpuFrequencyMhz(80);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// configure pin mode                                                             ESP32 port
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(sw_sens, OUTPUT);       //switch mux, SD etc.                           GPIO04()
  pinMode(sw_sens2, OUTPUT);      //switch sensor rail                            GPIO25()
  pinMode(sw_3_3v, OUTPUT);       //switch 3.3v output (with shift bme, rtc)      GPIO23()
  pinMode(s0_mux_1, OUTPUT);      //mux controll pin & s0_mux_1                   GPIO19()
  pinMode(s1_mux_1, OUTPUT);      //mux controll pin & s1_mux_1                   GPIO18()
  pinMode(s2_mux_1, OUTPUT);      //mux controll pin & s2_mux_1                   GPIO17()
  pinMode(s3_mux_1, OUTPUT);      //mux controll pin & s3_mux_1                   GPIO16()
  pinMode(sh_cp_shft, OUTPUT);    //74hc595 SH_CP                                 GPIO27()
  pinMode(st_cp_shft, OUTPUT);    //74hc595 ST_CP                                 GPIO26()
  pinMode(data_shft, OUTPUT);     //74hc595 Data                                  GPIO33()
  pinMode(vent_pwm, OUTPUT);      //vent pwm output                               GPIO32()
  //pinMode(chip_select, OUTPUT);   //SPI CS                                        GPIO15()
  //hardware defined SPI pin        //SPI MOSI                                      GPIO13()
  //hardware defined SPI pin        //SPI MISO                                      GPIO12()
  //hardware defined SPI pin        //SPI CLK                                       GPIO14()
  //pinMode(A0, INPUT);             //analog in?                                  GPIO()
  pinMode(sig_mux_1, INPUT);      //mux sig in                                    GPIO39()
  pinMode(en_mux_1, OUTPUT);      //mux enable                                    GPIO05() bei boot nicht high!
  //hardware defined IIC pin      //A4  SDA                                       GPIO21()
  //hardware defined IIC pin      //A5  SCL                                       GPIO22()
  //input only                                                                    (N)GPIO22(34)
  //input only 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize serial
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  delay(1);
  #ifdef DEBUG
  Serial.println("Hello!");
  #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize SPIFFS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  File file = SPIFFS.open(CONFIG_FILE_PATH);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  #ifdef DEBUG
  //Serial.println("File Content:");
  //while(file.available()){
  //  Serial.write(file.read());
  //}
  
  // DEBUG ONLY
  //getJsonObjects("group", CONFIG_FILE_PATH);
  //getJsonObjects("sensor", CONFIG_FILE_PATH);
  //getJsonObjects("switch", CONFIG_FILE_PATH);
  #endif
  file.close();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize bme280 sensor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  digitalWrite(sw_3_3v, HIGH); delay(5);
  shiftvalue8b(0);
  digitalWrite(sw_sens, HIGH);
  digitalWrite(sw_sens2, HIGH);

  Serial.println(F("BME280 Sensor event test"));
  if (!bme.begin(BME280_I2C_ADDRESS)) {
    #ifdef DEBUG
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    #endif
    int it = 0;
    while (1){
      delay(25);
      it++;
      #ifdef DEBUG
      Serial.println(F("."));
      #endif
      if(it > 100){
        break;
      }
    }
  }
  #ifdef DEBUG
  else{
    float temp_test = bme.readTemperature();
    Serial.print(F("Temperature reading: ")); Serial.println(temp_test);
  }
  #endif
  delay(500);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize PWM:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //  ledcAttachPin(GPIO_pin, PWM_channel);
  ledcAttachPin(vent_pwm, pwm_ch0);
  //Configure this PWM Channel with the selected frequency & resolution using this function:
  // ledcSetup(PWM_Ch, PWM_Freq, PWM_Res);
  ledcSetup(pwm_ch0, 21000, pwm_ch0_res);
  // PWM changing the duty cycle:
  // ledcWrite(pwm_ch0, pow(2, pwm_ch0_res) * fac);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// init time and date
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time (NOTE: only need to do this once not every time!)
  //set_time(30,15,15,04,14,06,23);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear

  //initialize global time
  Helper::read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  #ifdef DEBUG
  Serial.print("Time: "); Serial.print(hour_);
  Serial.println(F("debug 0"));
  #endif

  delay(500);  //make TX pin Blink 2 times
  Serial.print(measure_intervall);
  delay(500);
  Serial.println(measure_intervall);
  
  system_sleep(); //turn off all external transistors
  delay(500);
  
  // configure low power timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //init Irrigation Controller instance and update config
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  #ifdef RasPi
  wakeModemSleep();
  delay(1);
  // update the config file stored in spiffs
  // in order to work a RasPi with node-red and configured flow is needed
  updateConfig(CONFIG_FILE_PATH);
  #endif
  //hour_ = 0;
  //disableWiFi();

  system_sleep(); //turn off all external transistors

}

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- MAIN PROGRAM LOOP -----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

unsigned long startLoop = millis();
void loop(){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// start loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
digitalWrite(sw_3_3v, HIGH); delay(10);
shiftvalue8b(0);
delay(10);
#ifdef DEBUG
Serial.println(F("start loop setup"));
#endif

// check real time clock module
Wire.beginTransmission(DS3231_I2C_ADDRESS);
byte rtc_status = Wire.endTransmission();
//delay(200);
byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;

// check return status
if(rtc_status != 0){
  #ifdef DEBUG
  Serial.println(F("Error: rtc device not available!"));
  #endif
  int i = 0;
  while(rtc_status != 0){
    //loop as long as the rtc module is unavailable!
    #ifdef DEBUG
    Serial.print(F(" . "));
    #endif
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    rtc_status = Wire.endTransmission();

    //reactivate 3.3v supply
    digitalWrite(sw_3_3v, HIGH); delay(10);
    shiftvalue8b(0);
    delay(1000);
    if(i > (int)10){
      break;
    }
    i++;
  }
  //store into variable available for all following functions
}
else{
  #ifdef DEBUG
  Serial.println(F("rtc found"));
  #endif
}

if (!(bool)rtc_status)
{
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1); // update current timestamp
}
else{ // TODO: REMOVE THIS LATER AS IT CAN CAUSE SOME PROBLEMS!!!!!!!!!!!!!!!!!!!
  hour1 = 19;
}

#ifdef DEBUG
delay(1);
Serial.print(F("Time hour: ")); Serial.print(hour1); Serial.print(F(" last ")); Serial.println(hour_);
Serial.print("rtc status: "); Serial.println(rtc_status); Serial.println(!(bool) rtc_status);
#endif

// update configuration
SwitchController status_switches;
status_switches.updateSwitches();

// check for hour change and update config
//if((hour_ != hour1) & (!(bool)rtc_status)){
if(true){
  // check for hour change
  up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); // refresh the up_time every hour, no need for extra function or lib to calculate the up time
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_); // update long time timestamp

  // setup empty class instance & check timetables
  IrrigationController controller;

  #ifdef RasPi
  // look for config updates
  wakeModemSleep();
  delay(1);

  // update whole config
  // in order to work a RasPi with node-red and configured flow is needed
  updateConfig(CONFIG_FILE_PATH);
  #endif

  // combine timetables
  timetable = controller.combineTimetables();

  Serial.print(F("Combined timetable:"));
  Serial.println(timetable, BIN);
  // check if current hour is in timetable
  //if(true){
  if(bitRead(timetable, hour1)){

    #ifdef DEBUG
    if(status_switches.getIrrigationSystemSwitch())
    {
      thirsty = true; //initialize watering phase
      Serial.println(F("Watering ON"));
    }
    else{
      thirsty = false;
      Serial.println(F("Watering OFF"));
    }
    #endif
    
  }
}
#ifdef DEBUG
Serial.println(F("Config: ")); Serial.print(F("Bewae switch: ")); Serial.println(sw0);
Serial.print(F("Value override: ")); Serial.println(sw1);
Serial.print(F("Timetable override: ")); Serial.println(sw2);
Serial.print(F("Timetable: ")); Serial.println(timetable, BIN);
#endif

//update global time related variables
unsigned long loop_t = millis();
unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL; //time in ms, comming from rtc module
                                                                                                //give acurate values despite temperature changes

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// collect & send data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
Serial.print(F("Datalogphase: ")); Serial.println(actual_time-last_activation > measure_intervall);
Serial.println(actual_time); Serial.println(last_activation); Serial.println(measure_intervall-500000UL);
Serial.println((float)((float)actual_time-(float)last_activation)); Serial.println(up_time);
#endif
if(((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall)) && (status_switches.getDatalogingSwitch()))
{
  last_activation = actual_time; //first action refresh the time
  #ifdef DEBUG
  Serial.println(F("enter datalog phase"));
  #endif

  // turn on sensors
  delay(1);
  digitalWrite(sw_sens, HIGH);   //activate mux & SD
  digitalWrite(sw_sens2, HIGH);  //activate sensor rail
  delay(500);

  // Perform measurement of every configured vpin sensor (mux)
  JsonObject sens = getJsonObjects("sensor", CONFIG_FILE_PATH);
  // check for valid object
  if (sens.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Error: No 'Group' found in file!"));
    #endif
  }
  else{
    int numsens = sens.size();
    // iterate over json object, find all configured sensors measure and post the results
    for(JsonPair kv : sens){
      bool status = kv.value()["is_set"].as<bool>();
      // check status
      if (!status) {
        continue; // skip iteration if sensor is not set
      }
      int pin = String(kv.key().c_str()).toInt();
      String name = kv.value()["name"].as<String>();
      int low = kv.value()["llim"].as<int>();
      int high = kv.value()["hlim"].as<int>();
      float factor = kv.value()["fac"].as<float>();

      // create measurement instance
      VpinController sensor(name, pin, low, high, factor);
      float result;

      // check if relative or standard value is wanted
      if ((low != 0) && (high != 0)){
        result = sensor.measureRel();
      }
      else{
        result = sensor.measure();
      }

      // pub gathered datapoint
      bool cond = pubInfluxData(name, String(INFLUXDB_FIELD), result);

      #ifdef DEBUG
      // info pub data
      if (!cond) {
        Serial.print(F("Problem occured while publishing data to InfluxDB!"));
      }
      Serial.print(F("Publishing data from: ")); Serial.println(name);
      Serial.print(F("Value: ")); Serial.println(result);
      #endif
    }
  }

  // BME280 sensor (default)
  #ifdef BME280
  if (!bme.begin(BME280_I2C_ADDRESS)) {
    #ifdef DEBUG
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    #endif
  }
  #ifdef DEBUG
  Serial.print(F("Start measuring BME280"));
  #endif

  // setup bme measurment
  MeasuringController bme_sensor[3] =
  {
    {"bme_temp", [&]() { return bme.readTemperature(); }},
    {"bme_hum", [&]() { return bme.readHumidity(); }},
    {"bme_pres", [&]() { return bme.readPressure(); }}
  };

  for(int i=0; i < 3; i++){
    float result = bme_sensor->measure();
    bool cond = pubInfluxData(bme_sensor->getSensorName(), String(INFLUXDB_FIELD), result);
    #ifdef DEBUG
    if (!cond){
    Serial.println(F("Problem publishing BME!"));
    }
    #endif
  }
  #endif //bme280

  // DHT11 sensor (alternative)
  #ifdef DHT
  //possible dht solution --- CURRENTLY NOT IMPLEMENTED!
  #endif //dht
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
thirsty = true;
if(thirsty){
  digitalWrite(sw_3_3v, HIGH); delay(10); //switch on shift register! and logic?
  shiftvalue8b(0);
  #ifdef DEBUG
  Serial.println(F("start watering phase"));
  #endif
  delay(30);

  // --- Watering ---
  //description: trigger at specific time
  //             alternate the solenoids to avoid heat damage, let cooldown time if only one remains
  //Hints:  main mosfet probably get warm
  //        pause procedure when measure events needs to happen
  //        NEVER INTERUPT WHILE WATERING!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while((loop_t + measure_intervall > millis()) & (thirsty)){
    // process will trigger multiple loop iterations until thirsty is set false
    // this should allow a regular measure intervall and give additional time to the water to slowly drip into the soil

    // TODO: Change config file and "watering" value. There is an issue,
    //        when the file gets updated it's internal "watering" values will get set back to 0

    // Update config of controller
    //updateConfig(CONFIG_FILE_PATH);
    delay(15);

    JsonObject groups = getJsonObjects("group", CONFIG_FILE_PATH);
    // check for valid object
    if (groups.isNull()) {
      #ifdef DEBUG
      Serial.println(F("Error: No 'Group' found in file!"));
      #endif
      break; // break loop and continue programm
    }

    // get number of kv pairs within JSON object
    int numgroups = groups.size();

    // sanity check
    if (numgroups > max_groups) {
      #ifdef DEBUG
      Serial.println(F("Warning: too many groups! Exiting procedure"));
      #endif
      thirsty = false;
      break;
    }

    // load empty irrigationcontroller instances
    IrrigationController Group[numgroups];
    int j = 0;

    // Iterate over each group
    for (JsonObject::iterator groupIterator = groups.begin(); groupIterator != groups.end(); ++groupIterator) {
      // Check if j exceeds the maximum number of groups
      if (j >= numgroups) {
        #ifdef DEBUG
        Serial.println(F("Warning: Too many groups! Exiting procedure"));
        #endif
        thirsty = false;
        break;
      }

      // Load schedule configuration for the current group
      bool success = Group[j].loadScheduleConfig(*groupIterator);
      if (!success) {
        #ifdef DEBUG
        Serial.println(F("Error: Failed to load schedule configuration for group"));
        #endif

        // Reset the class to an empty state
        Group[j].reset();

        break;
      }

      // Increment j for the next group
      j++;
    }

/*
    IrrigationController Group[numgroups];
    int j = 0;
    // Init schedule of each group
    // Iterate through all keys in the "groups" object and initialize the class instance (kv = key value pair)
    for (JsonPair kv : groups) {
      // check if group is present and get its index
      const char* key = kv.key().c_str();

      JsonVariant groupValue = kv.value(); // Get the value of the JSON pair

      if (groupValue.is<JsonObject>()) {
Serial.println("groupValue.is<JsonObject>()");
        // load watering schedule of corresponding solenoid and/or pump (vpin)
        bool result = Group[j].loadScheduleConfig(CONFIG_FILE_PATH, key);
        delay(100); // give short delay to prevent issues with loading
        #ifdef DEBUG
        Serial.print(F("Group name: ")); Serial.println(key);
        #endif
Serial.print("if result json: "); Serial.println(!result);
        // check if group was valid and reset false ones
        if (!result) {
          #ifdef DEBUG
          Serial.println(F("Group reset, JSON not found."));
          #endif
          Group[j].reset();
        }
      }
      else {
        // reset the group to take it out of the process
Serial.println("group resetet");
        Group[j].reset();
      }
      j++;
    }*/

    // Iterate over all irrigation controller objects in the Group array
    // This process will trigger multiple loop iterations until thirsty is set false
    int status = 0;
    for(int i = 0; i < numgroups; i++){

      // ACTIVATE SOLENOID AND/OR PUMP
      // this will return some int numer as long as it needs to be watered
      // it will check if the instance is ready for watering (cooldown)
      // if not it will return early, else it will use delay to wait untill the process has finished
      hour1=19;
      status += Group[i].waterOn(hour1);
      // TODO: FIX ISSUE HERE:
    }
    // check if all groups are finished and reset status
    if(!status){
      thirsty = false;
    }
  }
  shiftvalue8b(0);
  delay(10);
  digitalWrite(sw_3_3v, LOW); //switch OFF logic gates (5V) and shift register
}
Serial.print("before sleep");

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sleep
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// start low power mode if not thirsty and not exceeding measure intervall
if ((loop_t + measure_intervall > millis()) && (!thirsty)){

  //sleep till next measure point
  int sleep_ = (long)(loop_t + measure_intervall - millis())/(TIME_TO_SLEEP * 1000UL);

  #ifdef DEBUG
  if(sleep_ < (int)measure_intervall / (TIME_TO_SLEEP * 1000)){
  Serial.println(F("Warning: sleep time calculation went wrong value too high!"));
  }
  Serial.print(F("sleep in s: ")); Serial.println((float)sleep_ * TIME_TO_SLEEP);
  #endif

  for(int i = 0; i < sleep_; i++){
    system_sleep(); //turn off all external transistors
    esp_light_sleep_start();
    if(loop_t + measure_intervall < millis()){
      break; //break loop to start measuring
    }
  }
}

#ifdef DEBUG
Serial.println(F("End loop!"));
#endif
}