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

bool thirsty = false; // marks if a watering cycle is needed

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

// sensoring implementation, returns timestamp of next event in UL
long sensoringTask();
// fetch manual watering overrides from server, write to running.Json, set thirsty
// hour_changed: true if Phase 1 detected a new hour (enables scheduled+override combination)
void checkOverrides(bool hour_changed, int current_hour, int current_day);

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
  LogFire.mirrorSerial(true);
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
  // OFFLINE_TEST: skip NTP/WiFi time sync. loop() starts with last_hour=255,
  // so the first iteration always detects an hour change and runs Phase 2 sync.
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
  // Persistent state — statics survive across loop() calls (light sleep preserves RAM)
  static byte last_hour = 255;            // 255 = sentinel: force hour-change on first boot
  static unsigned long last_sense_ms = 0; // tracks last sensor read timestamp

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Phase 1: WAKE — read time, load switches, detect hour change
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  HWHelper.enablePeripherals();
  delay(10);

  struct tm now;
  HWHelper.readTime(&now);

  SwitchController switches(&HWHelper);
  bool hour_changed = (now.tm_hour != last_hour);

  LogFire.log(
    String("wake ") +
    String(now.tm_hour) + ":" + (now.tm_min < 10 ? "0" : "") + String(now.tm_min) +
    " main=" + String(switches.getMainSwitch()) +
    " irrig=" + String(switches.getIrrigationSystemSwitch()) +
    " data=" + String(switches.getDataloggingSwitch()) +
    " heap=" + String(ESP.getFreeHeap()) + "B", 0);

  if (!switches.getMainSwitch()) {
    LogFire.log("sleep: main off", 0);
    HWHelper.disablePeripherals();
    HWHelper.system_sleep();
#ifndef OFFLINE_TEST
    Serial.flush();
    unsigned long sleepTarget = millis() + measure_intervall;
    while (millis() < sleepTarget) {
      delayMicroseconds(500);
      esp_light_sleep_start();
    }
#endif
    return;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Phase 2: SYNC — on hour change: WiFi up, sync config, check overrides
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (hour_changed) {
#ifndef OFFLINE_TEST
    HWHelper.wakeModemSleep();
    LogFire.log("sync: config update h=" + String(now.tm_hour), 1);
    if (HWHelper.syncConfig()) LogFire.log("sync: config error", 2);
#else
    LogFire.log("OFFLINE_TEST: skipping syncConfig", 0);
#endif
    last_hour = now.tm_hour;
    if (switches.getIrrigationSystemSwitch()) thirsty = true;
  }

#ifndef OFFLINE_TEST
  // Override check every wake (~10 min), not just on hour change
  if (WiFi.status() != WL_CONNECTED) HWHelper.wakeModemSleep();
  checkOverrides(hour_changed, now.tm_hour, now.tm_mday);
  if (!thirsty) HWHelper.disableWiFi();
#endif

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Phase 3: SENSE — if datalogging on and interval elapsed, read sensors
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (switches.getDataloggingSwitch() && (millis() - last_sense_ms > measure_intervall)) {
    LogFire.log("sense: reading sensors", 1);
#ifndef OFFLINE_TEST
    if (WiFi.status() != WL_CONNECTED) HWHelper.wakeModemSleep();
#endif
    sensoringTask();
    last_sense_ms = millis();
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Phase 4: WATER — if thirsty and irrigation on, run watering loop to completion
  // Safety: activate() blocks for full duration; NEVER interrupted; cooldowns enforced by readyToWater()
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if (thirsty && switches.getIrrigationSystemSwitch()) {
    String path = String(IRRIG_CONFIG_PATH) + String(JSON_SUFFIX);
    DynamicJsonDocument doc = HWHelper.getJsonDoc(path.c_str());

    if (doc.isNull() || !doc.is<JsonObject>()) {
      LogFire.log("water: config invalid", 3);
    } else {
      JsonObject groups = doc.as<JsonObject>();
      int numgroups = groups.size();

      if (numgroups > max_groups) {
        LogFire.log("water: too many groups=" + String(numgroups), 3);
      } else {
        // Use static array to avoid stack overflow; reset all entries each cycle
        static IrrigationController Group[max_groups];
        for (int i = 0; i < max_groups; i++) Group[i].reset();
        int j = 0;

        for (JsonObject::iterator it = groups.begin(); it != groups.end(); ++it) {
          if (String(it->key().c_str()) == String("checksum")) continue;
          if (!Group[j].loadScheduleConfig(*it)) {
            LogFire.log("water: schedule load failed group=" + String(it->key().c_str()), 3);
            Group[j].reset();
            break;
          }
          j++;
        }

        LogFire.log("water: starting cycle h=" + String(now.tm_hour) + ", " + String(j) + " groups loaded", 1);

#ifndef OFFLINE_TEST
        if (WiFi.status() != WL_CONNECTED) HWHelper.wakeModemSleep();
#endif
        HWHelper.enablePeripherals();

        // Read RTC once — shared across all group calls (avoids N I2C reads per loop iteration)
        struct tm waterTime;
        HWHelper.readTime(&waterTime);

        bool still_watering = true;
        while (still_watering) {
          delay(15);
          int finStatus = 0;
          for (int i = 0; i < j; i++) {
            finStatus += Group[i].watering_task_handler(waterTime);
            delay(500);
          }
          if (!finStatus) {
            thirsty = false;
            still_watering = false;
          } else {
            Serial.flush();
            esp_light_sleep_start();
          }
        }
        LogFire.log("water: complete", 1);
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // Phase 5: SLEEP — WiFi down, peripherals off, sleep ~10 min
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  HWHelper.disableWiFi();
  HWHelper.disablePeripherals();

  {
    int sleepMin = (int)(measure_intervall / 60000);
    int wakeMin = (now.tm_min + sleepMin) % 60;
    int wakeHour = (now.tm_hour + (now.tm_min + sleepMin) / 60) % 24;
    LogFire.log(
      String("sleep: ") + String(sleepMin) + "min until " +
      String(wakeHour) + ":" + (wakeMin < 10 ? "0" : "") + String(wakeMin), 1);
  }

  HWHelper.system_sleep();
#ifndef OFFLINE_TEST
  Serial.flush();
  unsigned long sleepTarget = millis() + measure_intervall;
  while (millis() < sleepTarget) {
    delayMicroseconds(500);
    esp_light_sleep_start();
  }
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
// override — fetch manual watering requests, write to running.Json, let Phase 4 handle it
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void checkOverrides(bool hour_changed, int current_hour, int current_day) {
  // Fetch overrideConfig from server
  String webPath = String(WEB_PREFIX) + "?deviceName=" + DEVICE_NAME + "&fileType=overrideConfig";
  DynamicJsonDocument overrideDoc = HWHelper.getJSONConfig(SERVER, NODERED_PORT, webPath.c_str());

  if (overrideDoc.isNull() || overrideDoc.size() == 0) {
    LogFire.log("override: none pending", 1);
    return;
  }

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
  if (clearKeys.size() == 0) {
    LogFire.log("override: none pending", 1);
    return;
  }

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

    // If this wake is also a scheduled hour that hasn't fired yet, combine both amounts.
    // "Same time" = hour_changed is true AND the group's timetable includes this hour.
    // Any other wake = override amount only.
    int combined = duration;
    if (hour_changed && groups.containsKey(key)) {
      JsonObject gdata = groups[key];
      uint32_t timetable = gdata["wt"].as<uint32_t>();
      if ((timetable & (1u << current_hour)) != 0) {
        bool already_watered = false;
        if (!runDoc.isNull() && runDoc.containsKey(key)) {
          JsonVariant upVar = runDoc[key]["up"];
          if (upVar.is<JsonArray>()) {
            JsonArray up = upVar.as<JsonArray>();
            if (up.size() >= 2 && up[0].as<int>() == current_hour && up[1].as<int>() == current_day)
              already_watered = true;
          }
        }
        if (!already_watered) {
          float wm = gdata.containsKey("wm") ? constrain(gdata["wm"].as<float>(), 0.0f, 2.0f) : 1.0f;
          int scheduled = max(0, (int)(gdata["pw"].as<int>() * wm));
          combined += scheduled;
          // Stamp the hour so the scheduled path doesn't re-fire this hour
          runDoc[key].remove("up");
          JsonArray up_arr = runDoc[key].createNestedArray("up");
          up_arr.add(current_hour);
          up_arr.add(current_day);
          LogFire.log("override: queued " + key + " ovr=" + String(duration) + "s sched=" + String(scheduled) + "s total=" + String(combined) + "s", 1);
        } else {
          LogFire.log("override: queued " + key + " " + String(duration) + "s (sched already ran)", 1);
        }
      } else {
        LogFire.log("override: queued " + key + " " + String(duration) + "s", 1);
      }
    } else {
      LogFire.log("override: queued " + key + " " + String(duration) + "s", 1);
    }
    runDoc[key]["dty"] = combined;
    runDoc[key]["ovr"] = 1;
    hasValid = true;
  }

  if (hasValid) {
    HWHelper.writeConfigFile(runDoc, RUNNING_FILE_PATH);
    thirsty = true;
  }
}