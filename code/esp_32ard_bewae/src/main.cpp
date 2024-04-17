////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This is the main source file for the irrigation system. It is responsible for monitoring all sensor values,
// handling the watering procedure, and communicating with the Raspberry Pi to get and send data.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Standard
#include <Arduino.h>
#include <Wire.h>
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

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- GLOBAL VARIABLES AND DEFINITIONS --------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################
//important global variables
//byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_; // containing time variables

struct tm oldtimeMark; // timemark

unsigned long nextActionTime = 0; // timestamp variable

bool thirsty = false; // marks if a watering cycle is finished

//timetable example
//                                             2523211917151311 9 7 5 3 1
//                                              | | | | | | | | | | | | |
//unsigned long int timetable_default = 0b00000000000000000000010000000000;
//                                               | | | | | | | | | | | | |
//                                              2422201816141210 8 6 4 2 0
unsigned long int timetable = 0; // holding info at which hour system needs to do something

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWireGlobal(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature soilsensorGlobal(&oneWireGlobal);

// use I2C interface
Adafruit_BME280 bme;

// initialise WiFi
WiFiClient wificlient;

// initialise Hardware Helper class
Helper_config1_Board5v5 HWHelper;
//Helper_config1_Board5v5 HWHelper;

// Create a BasicSensor instance
BasicSensor Sensors(&HWHelper, &bme, soilsensorGlobal);

// Create an instance of the InfluxDBClient class with the specified URL, database name and token
InfluxDBClient influx_client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_DB_NAME, INFLUXDB_TOKEN);

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SOME ADDITIONAL FUNCTIONS ---------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

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
    #ifdef DEBUG
    Serial.println("An Error has occurred while mounting SPIFFS");
    #endif
    return;
  }
  
  fs::File file = SPIFFS.open(CONFIG_FILE_PATH);
  if(!file){
    #ifdef DEBUG
    Serial.println("Failed to open file for reading");
    #endif
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
  //HWHelper.set_time(00,56,18,01,11,9,23);
  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  struct tm localTime = HWHelper.readTimeNTP();

  delay(100);
  //initialize global time
  bool condition = HWHelper.readTime(&oldtimeMark);
  delay(30);

  // automatically set time (requires WIFI access!!)
  struct tm local = HWHelper.readTimeNTP();
  if(HWHelper.verifyTM(local)){
    #ifdef DEBUG
    Serial.println(F("Info: Synched time!"));
    #endif
    HWHelper.setTime(local);
  }

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
  HWHelper.syncConfig();
// TEST DEVICE CONFIGURATION
Serial.print("Test DEVICE CONFIG!");
  SwitchController status_switches(&HWHelper); // initialize switch class
Serial.print("PASSED DEVICE CONFIG!");
  // Create a JSON object with the sensor data
DynamicJsonDocument doc(512);
doc["sensor"] = "temperature";
doc["value"] = 50;
doc["timestamp"] = "2024-04-07T16:16:03Z";

DynamicJsonDocument jsonDoc(CONF_FILE_SIZE); // create JSON doc, if an error occurs it will return an empty jsonDoc
JsonObject jsonobj;
Serial.println("TESTING FILE RETURNS!");
Serial.println(jsonDoc.isNull());
Serial.println(jsonobj.isNull());
Serial.println(jsonobj.size());

// Calculate the hash of the JSON object
String hash = HWHelper.calculateJSONHash(doc);
String testfile = "";
serializeJson(doc, testfile);
// Print the hash to the serial monitor
Serial.println();
Serial.println(testfile);
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

// manage sleep and updating of configuration
while(checkSleepTask()){
}
Serial.print("INIT SWITCHES:");
SwitchController status_switches(&HWHelper); // initialize switch class
//status_switches.updateSwitches(); // get called when initialized

// print system status
#ifdef DEBUG
Serial.println(F("Config: ")); Serial.print(F("Main switch: ")); Serial.println(status_switches.getMainSwitch());
Serial.print(F("Irrigation switch: ")); Serial.println(status_switches.getIrrigationSystemSwitch());
Serial.print(F("Measurement switch: ")); Serial.println(status_switches.getDatalogingSwitch());
Serial.print(F("Timetable: ")); Serial.println(timetable, BIN);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// collect & send data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
Serial.print(F("Datalogphase: ")); Serial.println(millis() > nextActionTime);
Serial.print(F("Next action time: ")); Serial.println(nextActionTime);
#endif
//if(true)
if((status_switches.getDatalogingSwitch()) && (millis() > nextActionTime))
{
  // Sensoring implementation
  nextActionTime = sensoringTask() + measure_intervall; // sensoring task returns finishing timestamp
}
else{
  nextActionTime = millis() + measure_intervall *2; // set a min delay (2 times longer)
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
Serial.println(F("--- End loop! ---"));
#endif
}
//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- MAIN PROGRAM END -------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sensoring - returns timestamp of next event
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
long sensoringTask(){
  #ifdef DEBUG
  Serial.println();
  Serial.println(F("enter datalog phase"));
  #endif

  if(!PUBDATA){
    #ifdef DEBUG
    Serial.println();
    Serial.println(F("Not mearuring Data! Flag set to false."));
    #endif
    return millis();
  }

  // turn on additional systems
  HWHelper.enablePeripherals();
  HWHelper.enableSensor();
  // shutdown wifi to free up adc2 pins!
  HWHelper.disableWiFi();
  delay(500); // give time to balance load

  // Create data vector to collect and publish later
  std::vector<SensorData> dataVec;
  std::vector<SensorData> dataVecTest;

  DynamicJsonDocument configf(CONF_FILE_SIZE);
  configf = HWHelper.readConfigFile(CONFIG_FILE_PATH);
  JsonObject obj;

  //float test = Sensors.onewirehandler();
  //Serial.print("ds18b20 temp: "); Serial.println(test);

  // handle analog pins
  //obj = configf["sensor"].as<JsonObject>(); // OLD CODE TODO: REMOVE WHEN TESTED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  obj = configf.as<JsonObject>();

  if(obj){
    for (JsonObject::iterator it = obj.begin(); it != obj.end(); ++it){
      String id = it->key().c_str();
      JsonObject sensorConfig = it->value().as<JsonObject>();
      // create & fill data
      SensorData data = Sensors.measurePoint(&HWHelper, id, sensorConfig);
      // Add the new SensorData object to the vector
      dataVecTest.push_back(data);
    }
  }

  // reconnect to wifi
  HWHelper.connectWifi();
  // Publish data vector
  bool success = Sensors.pubVector(&influx_client, dataVecTest);
  //bool success = false; //DEBUG
  #ifdef DEBUG
  if(success){
    Serial.println(F("All data published!"));
  }
  else{
    Serial.println(F("Warning: Problem occured while publishing data vector!"));
  }
  #endif

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
  JsonObject groups = HWHelper.getJsonObject(CONFIG_FILE_PATH, "group");
  int numgroups = groups.size();

  // sanity check
  if (numgroups > max_groups) {
    #ifdef DEBUG
    Serial.println(F("Warning: too many groups! Exiting procedure"));
    #endif
    return false;
  }
  // check for valid object
  if (groups.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Error: No 'Group' found in file!"));
    #endif
    return false; // break loop and continue programm
  }

  // load empty irrigationcontroller instances 
  IrrigationController Group[numgroups];
  int j = 0;

  // Iterate over each group and load the config
  for (JsonObject::iterator groupIterator = groups.begin(); groupIterator != groups.end(); ++groupIterator) {
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
// this function should be usable in a while loop returning true most of the time and false if there is something
bool checkSleepTask(){
  #ifdef DEBUG
  Serial.println(F("--- Start loop! ---"));
  #endif

  // activate 3.3v supply
  HWHelper.enablePeripherals();
  delay(10);

  // check real time clock module
  struct tm newtimeMark;

  // check return status
  byte rtc_status = HWHelper.readTime(&newtimeMark); // update current timestamp

  #ifdef DEBUG_SPAM
  Serial.print(F("Rtc Status: ")); Serial.println(!rtc_status);
  #endif
  #ifdef DEBUG
  String time = HWHelper.timestampNTP();
  Serial.println(time);
  #endif

  // look for config updates once an hour should be good
  HWHelper.wakeModemSleep();
  delay(1);
  // update config
  // in order to work a RasPi with node-red and configured flow is needed
  HWHelper.syncConfig();

  // load update configuration
  SwitchController controller_switches(&HWHelper);
  controller_switches.updateSwitches();

  //hour_ = 0; //DEBUG
  // check for hour change and update config
  //if(true){
  if((newtimeMark.tm_hour != oldtimeMark.tm_hour) && (rtc_status) && (controller_switches.getMainSwitch())){
    // check for hour change
    HWHelper.readTime(&oldtimeMark); // update long time timestamp

    // setup empty class instance & check timetables
    delay(5);
    // combine timetables
    timetable = IrrigationController::combineTimetables();

    #ifdef DEBUG
    Serial.print(F("Combined timetable:"));
    Serial.println(timetable, BIN);
    #endif

    // check if current hour is in timetable
    //if(true){
    if(bitRead(timetable, newtimeMark.tm_hour)){
      #ifdef DEBUG
      if(controller_switches.getIrrigationSystemSwitch())
      {
        thirsty = true; //initialize watering phase
        Serial.println(F("Watering ON: starting"));
      }
      else{
        thirsty = false;
        Serial.println(F("Watering OFF: do nothing"));
      }
      #endif
    }
  }

  // deactivate 3.3v supply
  HWHelper.disablePeripherals();

  #ifdef DEBUG
  Serial.println(F("Info: Sleeping!"));
  delay(10);
  #endif

  // prepare sleep
  unsigned long breakTime = 0;
  if(nextActionTime > 600000UL + millis()){
    breakTime = millis() + 600000UL; // reduce time to once per hour if intervall is bigger
  }
  else{
    breakTime = nextActionTime;
  }

  delay(3);
  while(true){ // sleep until break
    if(breakTime < millis()){
      #ifdef DEBUG
      Serial.println(F("Break loop!"));
      #endif
      break; //break loop to start doing stuff
    }
    HWHelper.system_sleep(); //turn off all external transistors
    delayMicroseconds(500);
    esp_light_sleep_start();
    #ifdef DEBUG_SPAM
    Serial.print(F("Sleeping: Wifi status: ")); Serial.println(WiFi.status());
    #endif
  }

  // exit whole loop only if system is switched ON
  if(controller_switches.getMainSwitch()){ // condition get checked with little delay!
    return false;
  }

  return true; // default
}