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

// Create new solenoid struct and initialize their corresponding virtual pins
Solenoid controller_sollenoids[6] =
{
  {0}, //solenoid 0
  {1}, //solenoid 1
  {2}, //solenoid 2
  {3}, //solenoid 3
  {6}, //solenoid 4
  {7}, //solenoid 5
};

// Create a new Pump struct and initialize their corresponding virtual pins
Pump pump_main[2] = 
{
  pump1,
  pump2,
};

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
// measurement function
bool measure_sensors(){
  JsonObject muxPins = getJsonObjects("test", CONFIG_FILE_PATH);
  // check for valid object
  if (muxPins.isNull()) {
    return false;
  }
  int num = muxPins.size();


  //VpinController configured_sensors[num];
/*
// setup virtual measurementpins (additional pins at the MUX)
VpinController vPin_mux[16] =
{
  {"v00", 0},
  {"v01", 1},
  {"v02", 2},
  {"v03", 3},
  {"v04", 4},
  {"v05", 5},
  {"v06", 6},
  {"v07", 7},
  {"v08", 8},
  {"v09", 9},
  {"v10", 10},
  {"v11", 11},
  {"v12", 12},
  {"v13", 13},
  {"v14", 14}, //battery voltage - needs calculation
  {"v15", 15}, //photo resistor - needs calculation
};

// setup bme measurment
MeasuringController bme_sensor[3] =
{
  {"bme_temp", [&]() { return bme.readTemperature(); }},
  {"bme_hum", [&]() { return bme.readHumidity(); }},
  {"bme_pres", [&]() { return bme.readPressure(); }}
};
*/
return true;
}

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
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
  #endif

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
  ledcSetup(pwm_ch0, 30000, pwm_ch0_res);
  // PWM changing the duty cycle:
  // ledcWrite(pwm_ch0, pow(2, pwm_ch0_res) * fac);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init time and date
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time
  //set_time(01,59,16,02,30,06,22);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear
  #ifdef DEBUG
  Serial.println(F("debug statement"));
  #endif
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
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  wakeModemSleep();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init Irrigation Controller instance and update config
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef RasPi
// setup empty class instance
IrrigationController controller;
// update the config file stored in spiffs
// in order to work a RasPi with node-red and configured flow is needed
controller.updateController();
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

Wire.beginTransmission(DS3231_I2C_ADDRESS);
byte rtc_status = Wire.endTransmission();
//delay(200);
byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;

// check real time clock module
// check for hour change and update config
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
  Serial.println(F("rtc found"));
}

if (!(bool)rtc_status)
{
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1); // update current timestamp
}
  #ifdef DEBUG
  delay(1);
  Serial.print(F("Time hour: ")); Serial.print(hour1); Serial.print(F(" last ")); Serial.println(hour_);
  Serial.print("rtc status: "); Serial.println(rtc_status); Serial.println(!(bool) rtc_status);
  #endif

if((hour_ != hour1) & (!(bool)rtc_status)){
//if(true){
  // check for hour change
  up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); // refresh the up_time every hour, no need for extra function or lib to calculate the up time
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_); // update long time timestamp

  // setup empty class instance
  IrrigationController controller;

  #ifdef RasPi
  // look for config updates
  wakeModemSleep();
  delay(1);
  // update the config file stored in spiffs with a file from the local network
  // in order to work a RasPi with node-red and configured flow is needed
  controller.updateController();
  #endif

  // combine timetables
  timetable = controller.combineTimetables();

  // check if current hour is in timetable
  //if(true){
  if(bitRead(timetable, hour1)){
    // get switches
    JsonObject switches = getJsonObjects("switches", CONFIG_FILE_PATH);

    thirsty = true; //initialize watering phase

    #ifdef DEBUG
    if(sw0)
    {
      Serial.println(F("Watering ON"));
    }
    else{
      Serial.println(F("Watering OFF"));
    }
    #endif
    
  }
}
#ifdef DEBUG
Serial.println(F("Config: ")); Serial.print(F("Bewae switch: ")); Serial.println(sw0);
Serial.print(F("Value override: ")); Serial.println(sw1);
Serial.print(F("timetable override: ")); Serial.println(sw2);
Serial.print(F("Timetable: ")); Serial.print(timetable, BIN);
#endif

//update global time related variables
unsigned long loop_t = millis();
unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL; //time in ms, comming from rtc module
                                                                                                //give acurate values despite temperature changes

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// collect & save or send data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
Serial.print(F("Datalogphase: ")); Serial.println(actual_time-last_activation > measure_intervall);
Serial.println(actual_time); Serial.println(last_activation); Serial.println(measure_intervall-500000UL);
Serial.println((float)((float)actual_time-(float)last_activation)); Serial.println(up_time);
#endif
//if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall-500000UL)) //replace for debuging
if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall))
//if(true)
//if(false)
{
  last_activation = actual_time; //first action refresh the time
  #ifdef DEBUG
  Serial.println(F("enter datalog phase"));
  #endif

  //turn on sensors
  delay(1);
  digitalWrite(sw_sens, HIGH);   //activate mux & SD
  digitalWrite(sw_sens2, HIGH);  //activate sensor rail
  delay(1000);
  
  //TODO ADD MEASURING CODE OF VPINCLASS HERE
/*
  //measure all moisture sensors
  int len = sizeof(measure_point)/sizeof(measure_point[0]);
  struct sensors* m_ptr = measure_point;
  for (int i=0; i<len; i++, m_ptr++ ) {
    if(m_ptr->is_set){ //convert moisture reading to relative moisture and clip bad data with constrain
    ....
*/
 

  // BME280 sensor (default)
  #ifdef BME280
  if (!bme.begin(BME280_I2C_ADDRESS)) {
    #ifdef DEBUG
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    #endif
  }
  // TODO ADD MEASURING CODE OF BME HERE:


  #endif

  // DHT11 sensor (alternative)
  #ifdef DHT
  //possible dht solution --- CURRENTLY NOT IMPLEMENTED!
  #endif

  // log to INFLUXDB (default)
  #ifdef RasPi
  if(post_DATA){
    wakeModemSleep();
    delay(1);

    //TODO ADD POSTING MEASUREMENT POINTS
    #ifdef BME280
    //TODO ADD POSTING MEASUREMENT POINTS
    #endif
  }
  #endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

    // Update config
    IrrigationController controller;
    // update the config file stored in spiffs
    // in order to work a RasPi with node-red and configured flow is needed
    controller.updateController();

    JsonObject groups = getJsonObjects("groups", CONFIG_FILE_PATH);
    // check for valid object
    if (groups.isNull()) {
      #ifdef DEBUG
      Serial.println(F("Error: No 'Group' found in file!"));
      #endif
      break; // break loop and continue programm
    }
    int numgroups = groups.size();
    
    IrrigationController Group[numgroups];
    int j = 0;
    // Init schedule of each group
    // Iterate through all keys in the "groups" object and initialize the class instance (kv = key value pair)
    for(JsonPair kv : groups){
      // check if group is present and get its index
      String key = kv.key().c_str();
      if (key.toInt() != 0 || key == "0"){
        int solenoid_index = key.toInt();
        // load watering schedule of corresponding solenoid and/or pump (vpin)
        bool result = Group[j].loadScheduleConfig(CONFIG_FILE_PATH, solenoid_index);
        // check if group was valid and reset false ones
        if (!result) {
          Group[j].reset();
        }
      }
      else{
        // reset the group to take it out of process
        Group[j].reset();
      }
      j++;
    }

    // Iterate over all irrigation controller objects in the Group array
    // This process will trigger multiple loop iterations until thirsty is set false
    int status = 0;
    for(int i = 0; i < numgroups; i++){
      // ACTIVATE SOLENOID AND/OR PUMP
      // this will return some int numer as long as it needs to be watered
      // it will check if the instance is ready for watering (cooldown)
      // if not it will return early, else it will use delay to wait untill the process has finished
      status += Group[i].waterOn(hour1);
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sleep
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if (loop_t + measure_intervall > millis()){

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
      break; //stop loop to start measuring
    }
  }
}

#ifdef DEBUG
Serial.println(F("End loop!"));
#endif
}