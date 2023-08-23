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
//#include <SPI.h>
//#include <SD.h>
#include <SPIFFS.h>
#include <OneWire.h>

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>

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

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- GLOBAL VARIABLES AND DEFINITIONS --------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################
//important global variables
byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_; // containing time variables
unsigned long up_time = 0;    //general estimated uptime
unsigned long last_activation = 0; //timestamp variable
unsigned long nextActionTime = 0; //timestamp variable
bool thirsty = false; //marks if a watering cycle is finished

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWireGlobal(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature soilsensorGlobal(&oneWireGlobal);

Adafruit_BME280 bme; // use I2C interface

Helper_config1_Board1v3838 HWHelper; // define Helper config

// Create a BasicSensor instance
BasicSensor Sensors(&HWHelper, &bme, soilsensorGlobal);

// Create an instance of the InfluxDBClient class with the specified URL, database name and token
InfluxDBClient influx_client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_DB_NAME, INFLUXDB_TOKEN);

//timetable example
//                                             2523211917151311 9 7 5 3 1
//                                              | | | | | | | | | | | | |
//unsigned long int timetable_default = 0b00000000000000000000010000000000;
//                                               | | | | | | | | | | | | |
//                                              2422201816141210 8 6 4 2 0

unsigned long int timetable = 0; // holding info at which hour system needs to do something

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SOME ADDITIONAL FUNCTIONS ---------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

// Initialise the WiFi and MQTT Client objects
WiFiClient wificlient;

// Task implementation can be found on bottom of code

// containing irrigation implementation, returns false if irrigation finished else true
bool irrigationTask();
// sensoring implementation, returns timestamp of next event in UL
long sensoringTask();
// powering down system, returning false when theres something to do else true!
bool checkSleepTask();

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
// configure pin mode
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  HWHelper.setPinModes();

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
  
  fs::File file = SPIFFS.open(CONFIG_FILE_PATH);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }

  file.close();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize bme280 & ds18b20 sensor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  HWHelper.enablePeripherals();
  HWHelper.enableSensor();

  // test bme280
  delay(300); // giving some time to handle powerup of devices

  #ifdef DEBUG
  Serial.println(F("BME280 Sensor event test"));
  if (!bme.begin(BME280_I2C_ADDRESS)) {
    Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
  }
  else{
    float temp_test = bme.readTemperature();
    Serial.print(F("Temperature reading: ")); Serial.println(temp_test);
  }
  delay(10);

  // test ds18b20
  soilsensorGlobal.begin();
  delay(100);

  // Request temperature
  soilsensorGlobal.requestTemperatures();
  delay(5);
  // Read temperature from DS18B20 sensor
  float temperatureC = soilsensorGlobal.getTempCByIndex(0);
  Serial.print("Test DS18B20: "); Serial.println(temperatureC);

  #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// init time and date
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time (NOTE: only need to do this once not every time!)
  //set_time(00,38,15,03,6,7,23);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear

  delay(100);
  //initialize global time
  HWHelper.read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  delay(30);

  HWHelper.system_sleep(); //power down prepare sleep
  delay(100);
  
  // configure low power timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init Irrigation Controller instance and update config
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    HWHelper.wakeModemSleep();
    delay(1);
    // update the config file stored in spiffs
    // in order to work a RasPi with node-red and configured flow is needed
    HWHelper.updateConfig(CONFIG_FILE_PATH);
  
    //hour_ = 0;
    //disableWiFi();

    HWHelper.system_sleep(); //power down prepare sleep
}

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- MAIN PROGRAM LOOP -----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################
void loop(){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// start & sleep loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
while(checkSleepTask()){
}
SwitchController status_switches(&HWHelper); // initialize switch class
status_switches.updateSwitches();

#ifdef DEBUG
Serial.println(F("Config: ")); Serial.print(F("Main switch: ")); Serial.println(status_switches.getMainSwitch());
Serial.print(F("Irrigation switch: ")); Serial.println(status_switches.getIrrigationSystemSwitch());
Serial.print(F("Measurement switch: ")); Serial.println(status_switches.getDatalogingSwitch());
Serial.print(F("Timetable: ")); Serial.println(timetable, BIN);
#endif

//update global time related variables
unsigned long loop_t = millis();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// collect & send data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
Serial.print(F("Datalogphase: ")); Serial.println(millis() > nextActionTime);
Serial.print(F("Next action time: ")); Serial.println(nextActionTime);
#endif
//if(true)
if((status_switches.getDatalogingSwitch()) && (millis() > nextActionTime)) // TODO FIX condition and work with status switches
{
  // Sensoring implementation
  nextActionTime = sensoringTask() + measure_intervall;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering - return true if finished
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(status_switches.getIrrigationSystemSwitch()){ // always enter checking, timing is handled by irrigation class
  irrigationTask();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// end loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
Serial.println(F("End loop!"));
#endif
}

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- MAIN PROGRAM END -------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################


// Task Implementation - to keep main loop readable

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sensoring - returns timestamp of next event
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
long sensoringTask(){
  #ifdef DEBUG
  Serial.println(F("enter datalog phase"));
  #endif

  // turn on additional systems
  HWHelper.enablePeripherals();
  HWHelper.enableSensor();
  // shutdown wifi to free up adc2 pins!
  HWHelper.disableWiFi();
  delay(500); // give time to balance load

  DynamicJsonDocument sensors(CONF_FILE_SIZE);
  sensors = HWHelper.readConfigFile(CONFIG_FILE_PATH);
  JsonObject obj;

  float test = Sensors.onewirehandler();
Serial.print("ds18b20 temp: "); Serial.println(test);
  // handle analog pins
Serial.println();
  obj = sensors["sensor"].as<JsonObject>();

  if(obj){
    Serial.println("obj found: ");
    for (JsonObject::iterator it = obj.begin(); it != obj.end(); ++it){
      String name = it->key().c_str();
      JsonObject sensorConfig = it->value().as<JsonObject>();
Serial.print("Obj name: "); Serial.println(name);
      float val;
      val = Sensors.measure(&HWHelper, name, sensorConfig);
      Sensors.pubData(&influx_client, val);
    }
  }

  // shut down addidional systems
  HWHelper.disablePeripherals();
  HWHelper.disableSensor();

  return millis(); // finishing timestamp
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering - return true if NOT fnished
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool irrigationTask(){
//thirsty = true; //uncoment for testing only
  HWHelper.enablePeripherals();

  #ifdef DEBUG
  Serial.println(F("Enter watering phase"));
  #endif
  delay(30);

  // load config file
  JsonObject groups = HWHelper.getJsonObjects("group", CONFIG_FILE_PATH);

  // check for valid object
  if (groups.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Error: No 'Group' found in file!"));
    #endif
    thirsty = false; // break loop and continue programm
  }

  // get number of kv pairs within JSON object
  int numgroups = groups.size();

  // sanity check
  if (numgroups > max_groups) {
    #ifdef DEBUG
    Serial.println(F("Warning: too many groups! Exiting procedure"));
    #endif
    thirsty = false;
  }

  // load empty irrigationcontroller instances 
  IrrigationController Group[numgroups];
  int j = 0;

  // Iterate over each group and load the config
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
    if (!success) { // if it fails reset class instance
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

  // --- Watering ---
  // description: trigger at specific time
  //              alternate the solenoids to avoid heat damage, let cooldown time if only one remains
  // Hints:  main mosfet probably get warm
  //         pause procedure when measure events needs to happen
  //         NEVER INTERUPT WHILE WATERING!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while((nextActionTime > millis()) && (thirsty)){
    // process will trigger multiple loop iterations until thirsty is set false
    // this should allow a regular measure intervall and give additional time to the water to slowly drip into the soil
    delay(15);

    // Iterate over all irrigation controller objects in the Group array
    // This process will trigger multiple loop iterations until thirsty is set false
    int finStatus = 0;
    for(int i = 0; i < numgroups; i++){
      // ACTIVATE SELECTED GROUP
      //hour1 = 19 // change for testing

      // start watering selected group
      // it will check if the instance is ready for watering (or on cooldown)
      finStatus += Group[i].watering_task_handler();
      delay(500); // give little delay
    }

    // check if all groups are finished and reset status
    if(!finStatus){
      thirsty = false;
    }
    esp_light_sleep_start(); // sleep one period
  }

  HWHelper.disablePeripherals();
  HWHelper.disableSensor();

  return thirsty;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// loop setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool checkSleepTask(){ // TODO REWORK TO WHILE LOOP!
  #ifdef DEBUG
  Serial.println(F("Start loop"));
  #endif

  // activate 3.3v supply
  HWHelper.enablePeripherals();

  // check real time clock module
  byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;
  // check return status
  byte rtc_status = HWHelper.readTime(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1); // update current timestamp

  // update configuration
  SwitchController controller_switches(&HWHelper);
  controller_switches.updateSwitches();

  // check for hour change and update config
  //if(true){
  if((hour_ != hour1) && (!(bool)rtc_status) && (controller_switches.getMainSwitch())){
    // check for hour change
    up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); // refresh the up_time every hour, no need for extra function or lib to calculate the up time
    HWHelper.readTime(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_); // update long time timestamp

    // look for config updates once an hour should be good
    HWHelper.wakeModemSleep();
    delay(1);
    // update config
    // in order to work a RasPi with node-red and configured flow is needed
    HWHelper.updateConfig(CONFIG_FILE_PATH);
    // setup empty class instance & check timetables
    IrrigationController controller;
    delay(30);
    // combine timetables
    timetable = controller.combineTimetables();

    #ifdef DEBUG
    Serial.print(F("Combined timetable:"));
    Serial.println(timetable, BIN);
    #endif

    // check if current hour is in timetable
    //if(true){
    if(bitRead(timetable, hour1)){
      #ifdef DEBUG
      if(controller_switches.getIrrigationSystemSwitch())
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

  // deactivate 3.3v supply
  HWHelper.disablePeripherals();

  // prepare sleep
  unsigned long breakTime = 0;
  if(nextActionTime > 600000UL + millis()){
    breakTime = millis() + 600000UL; // reduce time to once per hour if intervall is bigger
  }
  else{
    breakTime = nextActionTime;
  }

  while(true){ // sleep until break
    if(breakTime < millis()){
      #ifdef DEBUG
      Serial.println(F("Break loop!"));
      #endif
      break; //break loop to start doing stuff
    }
    Serial.print("millis: "); Serial.println(millis());
    Serial.print("break time: "); Serial.println(breakTime);
    HWHelper.system_sleep(); //turn off all external transistors
    delayMicroseconds(500);
    esp_light_sleep_start();
  }

  // exit whole loop only if system is switched ON
  if(controller_switches.getMainSwitch()){ // condition get checked with little delay!
    return false;
  }

  return true; // default
}