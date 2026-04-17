////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// see gitHub for author info
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
#include <ArduinoJson.h>
#include <DallasTemperature.h>

//custom
#include <connection.h>
#include <Helper.h>
#include <IrrigationController.h>
#include <SensorController.h>
#include <SwitchController.h>
#include <config.h>
#include <LogFire.h>

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

// initialise Hardware Helper class — board selected via HW_BOARD in config.h
HW_BOARD HWHelper;

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
// check server for manual override requests and fire them
void overrideTask();

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SETUP -----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

void setup() {
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// init
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //setCpuFrequencyMhz(80);
  // configure low power timer
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// configure pin mode
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  HWHelper.setPinModes();

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize serial
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  delay(1);
  // Init LogFire early with Serial-only — HTTP enabled after WiFi connects
  LogFire.begin(DEVICE_NAME, "0.0.0.0");
  LogFire.localOnly(true);
  LogFire.log("setup start", 1);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize SPIFFS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (!SPIFFS.begin(true)) {
    LogFire.log("SPIFFS mount failed", 3);
    return;
  }
  LogFire.log("SPIFFS ok free=" + String(SPIFFS.totalBytes() - SPIFFS.usedBytes()) + "B", 1);

  /*
  fs::File file = SPIFFS.open(CONFIG_FILE_PATH);
  if(!file){
    #ifdef DEBUG
    Serial.println("Failed to open file for reading");
    #endif
    return;
  }
  file.close();*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize bme280 & ds18b20 sensor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  HWHelper.enablePeripherals();
  HWHelper.enableSensor();

  // test bme280
  delay(300); // giving some time to handle powerup of devices

  if (!bme.begin(BME280_I2C_ADDRESS)) {
    LogFire.log("BME280 not found", 3);
  } else {
    float temp_test = bme.readTemperature();
    LogFire.log("BME280 ok t=" + String(temp_test, 1) + "C", 1);
  }
  delay(10);

  // test ds18b20
  soilsensorGlobal.begin();
  delay(100);
  soilsensorGlobal.requestTemperatures();
  delay(5);
  float temperatureC = soilsensorGlobal.getTempCByIndex(0);
  if (temperatureC == -127.0) {
    LogFire.log("DS18B20 not found", 3);
  } else {
    LogFire.log("DS18B20 ok t=" + String(temperatureC, 1) + "C", 1);
  }

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// init time and date
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef OFFLINE_TEST
  // OFFLINE_TEST: skip NTP/WiFi time sync — leave oldtimeMark at zero so
  // checkSleepTask() detects an hour change on the first loop iteration.
  LogFire.log("bewae boot (offline)", 1);
  delay(30);
#else
  HWHelper.wakeModemSleep();
  delay(1);
  //uncomment if want to set the time (NOTE: only need to do this once not every time!)
  //HWHelper.set_time(00,21,23,01,23,8,24);
  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //struct tm localTime = HWHelper.readTimeNTP();

  // automatically set time (requires WIFI access!!)
  struct tm local = HWHelper.readTimeNTP();
  // Switch LogFire to HTTP now that WiFi is up
  LogFire.localOnly(false);
  LogFire.begin(DEVICE_NAME, SERVER);

  if(HWHelper.verifyTM(local)){
    LogFire.log("NTP synced", 1);
    //HWHelper.setTime(local); // TODO REWORK SOMETHING FAILS HERE
    HWHelper.set_time(local.tm_sec,local.tm_min,local.tm_hour,local.tm_wday,local.tm_mday,local.tm_mon,local.tm_year);
  }
  else{
    LogFire.log("NTP sync failed", 2);
  }
  delay(100);
  //initialize global time
  bool condition = HWHelper.readTime(&oldtimeMark);
  delay(30);

  LogFire.log("bewae boot", 1);
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init Irrigation Controller instance and update config
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef OFFLINE_TEST
  HWHelper.wakeModemSleep();
  delay(1);
  // update the config file stored in spiffs
  // in order to work a RasPi with node-red and configured flow is needed
  if(HWHelper.syncConfig()){
    LogFire.log("Config sync error in setup", 2);
  }
#else
  LogFire.log("OFFLINE_TEST: skipping syncConfig", 1);
#endif
  // TEST DEVICE CONFIGURATION
  SwitchController status_switches(&HWHelper); // initialize switch class

  {
    String filePath = String(IRRIG_CONFIG_PATH) + String(JSON_SUFFIX);
    DynamicJsonDocument jsonDoc = HWHelper.readConfigFile(filePath.c_str());
    if (jsonDoc.isNull()) {
      LogFire.log("irrig config missing on boot", 3);
    } else {
      LogFire.log("setup done heap=" + String(ESP.getFreeHeap()) + "B", 1);
    }
  }
  HWHelper.system_sleep(); //power down prepare sleep
  delay(100);

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
while(checkSleepTask()){ // uncomment for Test (TESTRUN FLAG)
}
SwitchController status_switches(&HWHelper); // initialize switch class
//status_switches.updateSwitches(); // get called when initialized

LogFire.log("loop", 1);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// collect & send data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(status_switches.getDataloggingSwitch() && millis() > nextActionTime){
  LogFire.log("sensors: running", 1);
  nextActionTime = sensoringTask() + measure_intervall;
  LogFire.log("sensors: done", 1);
}
else{
  String reason = !status_switches.getDataloggingSwitch() ? "switch off" : "too soon";
  LogFire.log("sensors: skip (" + reason + ")", 1);
  nextActionTime = millis() + measure_intervall * 2;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering - return true if finished
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(status_switches.getIrrigationSystemSwitch()){
  LogFire.log("irrig: check thirsty=" + String(thirsty ? "yes" : "no"), 1);
  irrigationTask();
  LogFire.log("irrig: check done", 1);
}
else{
  LogFire.log("irrig: skip (switch off)", 1);
}
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
  LogFire.log("sensors: start", 1);

  if(!PUBDATA){
    LogFire.log("sensoringTask skipped: PUBDATA=false", 2);
    return millis();
  }

  // turn on additional systems
  HWHelper.enablePeripherals();
  HWHelper.enableSensor();
  // Create data vector to collect and publish later
  std::vector<SensorData> dataVec;
  std::vector<SensorData> dataVecTest;

  DynamicJsonDocument configf(CONF_FILE_SIZE);
  String path = String(SENS_CONFIG_PATH) + String(JSON_SUFFIX);
  configf = HWHelper.readConfigFile(path.c_str());
  JsonObject obj;

  //float test = Sensors.onewirehandler();
  //Serial.print("ds18b20 temp: "); Serial.println(test);

  // handle analog pins
  obj = configf.as<JsonObject>();

  if(obj){
    LogFire.log("sensors: " + String(obj.size()) + " to read", 1);
    for (JsonObject::iterator it = obj.begin(); it != obj.end(); ++it){
      String id = it->key().c_str();
      JsonObject sensorConfig = it->value().as<JsonObject>();
      // create & fill data
      SensorData data = Sensors.measurePoint(&HWHelper, id, sensorConfig);
      LogFire.log("sensor \"" + data.name + "\": " + String(data.data) + " (" + data.field + ")", 0);
      // Add the new SensorData object to the vector
      dataVecTest.push_back(data);
    }
  }

  // Publish data vector
  bool success = Sensors.pubVector(&influx_client, dataVecTest);
  if(success){
    LogFire.log("Sensor data published", 1);
  }
  else{
    LogFire.log("InfluxDB publish failed", 2);
  }
  // shut down additional systems
  HWHelper.disablePeripherals();
  HWHelper.disableSensor();

  return millis(); // finishing timestamp
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering - return true if NOT fnished
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool irrigationTask(){
//thirsty = true; //uncoment for testing only (TESTRUN FLAG)
  HWHelper.enablePeripherals();
  if(thirsty) LogFire.log("irrigationTask start", 1);
  delay(30);

  // load config file
  String path = String(IRRIG_CONFIG_PATH) + String(JSON_SUFFIX);
  DynamicJsonDocument doc = HWHelper.getJsonDoc(path.c_str());
  JsonObject groups;
  // Check if the document is not null and contains a JsonObject
  if (doc.isNull() || !doc.is<JsonObject>()) {
    LogFire.log("irrigationTask: config invalid", 3);
    return false;
  }
  groups = doc.as<JsonObject>();
  int numgroups = groups.size();
  // sanity check
  if (numgroups > max_groups) {
    LogFire.log("irrigationTask: too many groups=" + String(numgroups), 3);
    return false;
  }
  // check for valid object
  if (groups.isNull()) {
    LogFire.log("irrigationTask: no groups in config", 3);
    return false; // break loop and continue programm
  }

  // use static array to avoid stack overflow from VLA with dynamic numgroups
  static IrrigationController Group[max_groups];
  // reset all entries to avoid stale state from previous calls
  for (int i = 0; i < max_groups; i++) {
    Group[i].reset();
  }
  int j = 0;

  // Iterate over each group and load the config
  for (JsonObject::iterator groupIterator = groups.begin(); groupIterator != groups.end(); ++groupIterator) {
    // Load schedule configuration for the current group
    if (String(groupIterator->key().c_str()) == String("checksum")){
      continue; // skip checksum (TODO: idea for rework of config files, then this "forbiden" name would be ok {"name"{config},"checksum":"asdfxcz"})
    }
    bool success = Group[j].loadScheduleConfig(*groupIterator);
    if (!success) { // if it fails reset class instance
      LogFire.log("irrigationTask: schedule load failed group=" + String(groupIterator->key().c_str()), 3);
      // Reset the class to an empty state
      Group[j].reset();
      break;
    }

    // Increment j for the next group
    j++;
  }
  if(thirsty){
    LogFire.log("watering: " + String(j) + "/" + String(numgroups) + " groups loaded", 1);
    // WiFi up for the whole watering session — needed for activate: logs
    // WiFi stack runs on separate core, no interference with shift register timing
    HWHelper.wakeModemSleep();
  }

  // --- Watering ---
  // description: trigger at specific time
  //              alternate the solenoids to avoid heat damage, let cooldown time if only one remains
  // Hints:  main mosfet probably get warm
  //         pause procedure when measure events needs to happen
  //         NEVER INTERUPT WHILE WATERING!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while((nextActionTime > millis()) && (thirsty)){
  //while(true){ // TEMP DEBUG ONLY!!!
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
      LogFire.log("irrigation complete", 1);
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
  // activate 3.3v supply
  HWHelper.enablePeripherals();
  delay(10);

  // check real time clock module
  struct tm newtimeMark;

  // check return status
  byte rtc_status = HWHelper.readTime(&newtimeMark); // update current timestamp

  #ifdef DEBUG_SPAM
  Serial.print(F("Info: Rtc Status: ")); Serial.println(!rtc_status);
  #endif

  // load switches from SPIFFS — no WiFi needed
  SwitchController controller_switches(&HWHelper);
  controller_switches.updateSwitches();

  // heartbeat — Serial only (WiFi off); goes remote once per hour during hour-change block below
  LogFire.log(
    String("cycle ") +
    String(newtimeMark.tm_hour) + ":" +
    (newtimeMark.tm_min < 10 ? "0" : "") + String(newtimeMark.tm_min) +
    " main=" + controller_switches.getMainSwitch() +
    " irrig=" + controller_switches.getIrrigationSystemSwitch() +
    " datalog=" + controller_switches.getDataloggingSwitch() +
    " heap=" + String(ESP.getFreeHeap()) + "B", 0);

  //oldtimeMark.tm_hour = 0; // DEBUG DEBUGING ONLY

  // check for hour change — only here do we need WiFi
  //if(true){ //DEBUGING ONLY (TESTRUN FLAG)
  if((newtimeMark.tm_hour != oldtimeMark.tm_hour) && (rtc_status) && (controller_switches.getMainSwitch())){
    HWHelper.readTime(&oldtimeMark); // update long time timestamp
    delay(5);

#ifndef OFFLINE_TEST
    // WiFi up for config sync and remote logs for the rest of this block
    HWHelper.wakeModemSleep();
    delay(1);
    if(HWHelper.syncConfig()){
      LogFire.log("Config sync error", 2);
    }
#else
    LogFire.log("OFFLINE_TEST: skipping syncConfig", 0);
#endif

    timetable = IrrigationController::combineTimetables();
    LogFire.log("timetable updated", 1);

    if(bitRead(timetable, newtimeMark.tm_hour)){
      if(controller_switches.getIrrigationSystemSwitch())
      {
        thirsty = true; //initialize watering phase
        LogFire.log(String("Irrigation triggered h=") + String(newtimeMark.tm_hour), 1);
      }
      else{
        thirsty = false;
        LogFire.log("Irrigation hour but switch off", 1);
      }
    }

  }

  // deactivate 3.3v supply
  HWHelper.disablePeripherals();

  // prepare sleep
  unsigned long breakTime = 0;
  if((nextActionTime > 600000UL + millis()) || (nextActionTime < millis())){
    breakTime = millis() + 600000UL; // reduce time to once per 10 min if intervall is bigger
  }
  else{
    breakTime = nextActionTime + 1;
  }

  // log sleep duration and wakeup time
  {
    int sleepMin = (int)((breakTime - millis()) / 60000);
    int wakeMin = (newtimeMark.tm_min + sleepMin) % 60;
    int wakeHour = (newtimeMark.tm_hour + (newtimeMark.tm_min + sleepMin) / 60) % 24;
    LogFire.log(
      String("sleeping ") + String(sleepMin) + "min until " +
      String(wakeHour) + ":" + (wakeMin < 10 ? "0" : "") + String(wakeMin), 1);
  }
#ifndef OFFLINE_TEST
  HWHelper.disableWiFi();
#endif

#ifdef OFFLINE_TEST
  // OFFLINE_TEST: skip the entire sleep loop (normally waits up to 10 min).
  // Fall straight through to the return statement.
#else
  delay(3);
  while(true){ // sleep until break
    if(breakTime < millis()){
      break; //break loop to start doing stuff
    }
    HWHelper.system_sleep(); //turn off all external transistors
    delayMicroseconds(500);
    esp_light_sleep_start();
    #ifdef DEBUG_SPAM
    Serial.print(F("Sleeping: Wifi status: ")); Serial.println(WiFi.status());
    #endif
  }
#endif
  // exit whole loop only if system is switched ON
  if(controller_switches.getMainSwitch()){ // condition get checked with little delay!
#ifndef OFFLINE_TEST
    HWHelper.wakeModemSleep();
    overrideTask(); // check for manual watering overrides
#endif
    LogFire.log("awake", 1);
    return false;
  }

  return true; // default
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// override — fetch manual watering requests, write to running.Json, let irrigationTask handle it
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void overrideTask() {
  // Fetch overrideConfig from server
  String webPath = String(WEB_PREFIX) + "?deviceName=" + DEVICE_NAME + "&fileType=overrideConfig";
  DynamicJsonDocument overrideDoc = HWHelper.getJSONConfig(SERVER, NODERED_PORT, webPath.c_str());

  if (overrideDoc.isNull() || overrideDoc.size() == 0) return;

  // Collect active overrides and build clear payload
  JsonObject overrides = overrideDoc.as<JsonObject>();
  DynamicJsonDocument clearDoc(256);
  JsonArray clearKeys = clearDoc.createNestedArray(DEVICE_NAME);

  for (JsonPair p : overrides) {
    JsonObject entry = p.value();
    if (entry.containsKey("active") && entry["active"].as<int>() == 1) {
      clearKeys.add(p.key().c_str());
    }
  }
  if (clearKeys.size() == 0) return;

  // Clear overrides on server BEFORE firing (safety: if ESP crashes mid-water, override won't re-fire)
  String clearPayload;
  serializeJson(clearDoc, clearPayload);
  if (!HWHelper.postJSON(SERVER, NODERED_PORT, "/bewae/clear-override", clearPayload)) {
    LogFire.log("override: clear failed, abort", 2);
    return;
  }
  LogFire.log("override: cleared " + String(clearKeys.size()), 1);

  // Validate override groups exist in local plantConfig
  String plantPath = String(IRRIG_CONFIG_PATH) + String(JSON_SUFFIX);
  DynamicJsonDocument plantDoc = HWHelper.readConfigFile(plantPath.c_str());
  if (plantDoc.isNull()) return;
  JsonObject groups = plantDoc.as<JsonObject>();

  // Write override durations to running.Json with ovr flag
  DynamicJsonDocument runDoc = HWHelper.readConfigFile(RUNNING_FILE_PATH);
  bool hasValid = false;

  for (JsonPair p : overrides) {
    JsonObject entry = p.value();
    if (!entry.containsKey("active") || entry["active"].as<int>() != 1) continue;
    String key = p.key().c_str();
    int duration = entry["duration"].as<int>();

    if (!groups.containsKey(key)) {
      LogFire.log("override: unknown group " + key, 2);
      continue;
    }

    runDoc[key]["dty"] = duration;
    runDoc[key]["ovr"] = 1;
    LogFire.log("override: queued " + key + " " + String(duration) + "s", 1);
    hasValid = true;
  }

  if (hasValid) {
    HWHelper.writeConfigFile(runDoc, RUNNING_FILE_PATH);
    thirsty = true;
  }
}