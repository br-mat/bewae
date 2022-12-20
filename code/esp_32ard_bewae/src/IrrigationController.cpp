////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This file contains the functionality for the IrrigationController class, which is responsible for managing and
// controlling an irrigation system. It includes functions for creating and configuring irrigation controllers,
// updating the controller based on current conditions, and determining when it is time to water.
//
// Dependencies:
// - Arduino.h
// - IrrigationController.h
// - SPIFFS.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include <IrrigationController.h>
#include <SPIFFS.h>

#define DEBUG

// Define constants
// Define the number of solenoids to create
//const int maxSolenoids = max_groups;

// Define the number of solenoids to create
//const int maxPumps = MAX_PUMPS;
/*
void IrrigationHardware(int maxSolenoids, int maxPumps){
  // create hardware components and asign corresponding pins

  // Declare the array of Solenoid objects
  Solenoid solenoids[maxSolenoids];
  // Iterate over the array and initialize each Solenoid object
  for (int i = 0; i < maxSolenoids; i++) {
    solenoids[i].setup(i);
  }
  // Declare the array of Solenoid objects
  Pump pumps[maxPumps];
  // Iterate over the array and initialize each Solenoid object
  for (int i = 0; i < maxPumps; i++) {
    pumps[i].setup(i);
  }
}
*/

void Solenoid::setup(int pin){
  // Initialize solenoid
  this->pin = pin;
  this->lastActivation = 0;
  }

void Pump::setup(int pin){
  // Initialize pumps
  this->pin = pin;
  this->lastActivation = 0;
}

IrrigationController::IrrigationController(const char path[], 
                        Solenoid* solenoid, //attatched solenoid
                        Pump* pump //attatched pump
                        ){
  this->solenoid = solenoid;
  this->pump = pump;
  bool cond = this->loadFromConfig(path, solenoid->pin);
  if(cond){
    return;
  }
  //else set values to 0
  this->is_set = false;
  this->name = "NaN";
  this->timetable = 0;
  this->watering_default = 0;
  this->watering_mqtt = 0;
  this->water_time = 0;
  }


void IrrigationController::createNewController(
                        bool is_set, //activate deactivate group
                        String name, //name of the group
                        unsigned long timetable,
                        int watering_default, //defualt value of watering amount set for group (should not be changed)
                        Solenoid* solenoid, //attatched solenoid
                        Pump* pump, //attatched pump
                        int watering_mqtt, //base value of watering time sent from RaspPi
                        int water_time //holds value of how long it should water
                        ){
  this->is_set = is_set;
  this->name = name;
  this->timetable = timetable;
  this->watering_default = watering_default;
  this->solenoid = solenoid;
  this->pump = pump;
  this->solenoid_pin = solenoid->pin;
  this->pump_pin = pump->pin;
  this->watering_mqtt = watering_mqtt;
  this->water_time = water_time;
  }

  int IrrigationController::readyToWater(int currentHour) {
  //INPUT: currentHour is the int digits of the full hour
  //Function:
  //It checks if the irrigation system is ready to water.
  //If the system is ready,
  //it calculates the remaining watering time and updates the waterTime variable and returns the active time.
  //It returns 0 if the system is not ready to water.

  //to prevent unexpected behaviour its important to pass only valid hours

  if(!is_set){ //return 0 if the group is not set
    #ifdef DEBUG
    Serial.println("not set");
    #endif
    return 0;
  }

    // check if the corresponding bit is set in the timetable value
  if ((timetable & (1UL << currentHour)) == 0) {
    #ifdef DEBUG
    Serial.println("not correct time");
    #endif
    return 0; // not ready to water
  }

  // get the current time in milliseconds
  //unsigned long currentMillis = millis();
  unsigned long currentMillis = millis();

  // check if the solenoid has been active for too long, max_axtive_time_sec have to be multiplied as lastActivation is ms
  if (currentMillis - solenoid->lastActivation < SOLENOID_COOLDOWN) {
    #ifdef DEBUG
    Serial.println("solenoid not cooled down");
    #endif
    return 0; // not ready to water
  }

  // check if the pump has been active for too long
  if (currentMillis - pump->lastActivation < pump_cooldown_sec) {
    #ifdef DEBUG
    Serial.println("pump not cooled down");
    #endif
    return 0; // not ready to water
  }

  // check if there is enough water time left
  if (water_time <= 0) {
    #ifdef DEBUG
    Serial.println("no time left");
    #endif
    return 0; // not ready to water
  }

// both the solenoid and pump are ready, so calculate the remaining watering time,
// return seconds using min function
// to minimize stress to hardware part,
// ignore how long the part was active previously and let it cd always the full time!
int remainingTime = min(water_time, max_active_time_sec);
remainingTime = max(remainingTime, (int)0); //avoid values beyond 0
this->water_time -= remainingTime; // update the water time

// update the last activation time of the solenoid and pump
solenoid->lastActivation = currentMillis;
pump->lastActivation = currentMillis;

#ifdef DEBUG
Serial.print("remaining time: "); Serial.println(remainingTime);
#endif
return remainingTime; //return active time
}

bool IrrigationController::loadFromConfig(const char path[], int pin) {
  //INPUT: group_index refers to the hardware pin of the corresponding solenoid
  // Read the config file
  File configFile = SPIFFS.open(path, "r");
  if (!configFile) {
    #ifdef DEBUG
    Serial.println("Failed to open config file");
    #endif
    return false;
  }

  // Parse the JSON from the config file
  DynamicJsonDocument jsonDoc(1024);
  DeserializationError error = deserializeJson(jsonDoc, configFile);

  // Close the config file and open it again later to prevent problems in error case
  configFile.close();

  if (!error) {
    #ifdef DEBUG
    Serial.println("Failed to parse config file");
    #endif
    return false;
  }
  // Set the values of the private member variables
  // using the values from the JSON document
  this->is_set = jsonDoc["group"][String(pin)]["is_set"].as<int16_t>();
  this->name = jsonDoc["group"][String(pin)]["name"].as<String>();
  this->timetable = jsonDoc["group"][String(pin)]["timetable"].as<uint32_t>();
  this->watering_default = jsonDoc["group"][String(pin)]["watering_default"].as<int16_t>();
  this->watering_mqtt = jsonDoc["group"][String(pin)]["watering_mqtt"].as<int16_t>();
  this->water_time = jsonDoc["group"][String(pin)]["water_time"].as<int16_t>();

  //just store pin information as its identical with the index of the solenoids
  this->solenoid_pin = jsonDoc["group"][String(pin)]["solenoid_pin"].as<int16_t>();
  this->pump_pin = jsonDoc["group"][String(pin)]["pump_pin"].as<int16_t>();

  return true;
}

bool IrrigationController::saveToConfig(const char path[], int pin) {
  // Open the config file for reading
  File configFile = SPIFFS.open(path, "r");
  if (!configFile) {
    #ifdef DEBUG
    Serial.println("Failed to open config file for reading");
    #endif
    configFile.close();
    return false;
  }

  // Load the JSON data from the config file
  DynamicJsonDocument jsonDoc(1024);
  DeserializationError error = deserializeJson(jsonDoc, configFile);
  if (!error) {
    #ifdef DEBUG
    Serial.println("Failed to parse config file");
    #endif
    configFile.close();
    return false;
  }
  configFile.close();

  // Check if the group and pin exist in the JSON data
  if (!jsonDoc["group"].containsKey(String(pin))) {
    // If the group and pin do not exist, return error
    #ifdef DEBUG
    Serial.println(F("Error: missing json object, no data found under provided keys"));
    #endif
    return false;
  }


  // TODO: create new json file with group and pin for easier reading and addapt code below

  // Update the JSON data with the irrigation controller's information
  jsonDoc["group"][String(pin)]["is_set"] = this->is_set;
  jsonDoc["group"][String(pin)]["name"] = this->name;
  jsonDoc["group"][String(pin)]["timetable"] = this->timetable;
  jsonDoc["group"][String(pin)]["watering_default"] = this->watering_default;
  jsonDoc["group"][String(pin)]["watering_mqtt"] = this->watering_mqtt;
  jsonDoc["group"][String(pin)]["water_time"] = this->water_time;
  jsonDoc["group"][String(pin)]["solenoid_pin"] = this->solenoid->pin;
  jsonDoc["group"][String(pin)]["pump_pin"] = this->pump->pin;

  // Open the config file for writing
  File newFile = SPIFFS.open(path, "w");
  if (!newFile) {
    #ifdef DEBUG
    Serial.println("Failed to open config file for writing");
    #endif
    newFile.close();
    return false;
  }
  
  // Serialize the JSON object to the config file
  if (serializeJson(jsonDoc, newFile) == 0) {
    newFile.close();
    #ifdef DEBUG
    Serial.println("Failed to serialize JSON data to config file");
    #endif
    return false;
  }

  newFile.close();
  return true;
}

/*
bool IrrigationController::saveToConfig(const char path[], int pin) {
  //TODO: rework function, first open config and load json data, then check if group and pins is available
  //      and check all the other posible keys like is_set , name ... 
  //      the next step is to update these keys like shown below ...

  // Create a JSON object to hold the irrigation controller's information
  DynamicJsonDocument jsonDoc(1024);
  jsonDoc["group"][String(pin)]["is_set"] = this->is_set;
  jsonDoc["group"][String(pin)]["name"] = this->name;
  jsonDoc["group"][String(pin)]["timetable"] = this->timetable;
  jsonDoc["group"][String(pin)]["watering_default"] = this->watering_default;
  jsonDoc["group"][String(pin)]["watering_mqtt"] = this->watering_mqtt;
  jsonDoc["group"][String(pin)]["water_time"] = this->water_time;
  jsonDoc["group"][String(pin)]["solenoid_pin"] = this->solenoid->pin;
  jsonDoc["group"][String(pin)]["pump_pin"] = this->pump->pin;

  // Open the config file for writing
  File configFile = SPIFFS.open(path, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  
  // Serialize the JSON object to the config file
  serializeJson(jsonDoc, configFile);
  configFile.close();
  return true;
}
*/

void IrrigationController::updateController(){
  //TODO: implement function to update controller status data regarding watering system
  // call this function once per hour it uses timetable to determine if it should set water_time variable with content either
  // watering_default or watering_mqtt depending on a modus variable past with the function (this can be an integer about 0-5)
  // water time variable holds information about the water cycle for the active hour if its 0 theres nothing to do
  // if its higher then it should be recognized by the readyToWater function, this value 
}