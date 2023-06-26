////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This file contains the functionality for the IrrigationController class, which is responsible for managing and
// controlling an irrigation system. It includes functions for creating and configuring irrigation controllers,
// updating the controller based on current conditions, and determining when it is time to water.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <IrrigationController.h>

#define DEBUG

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HARDWARE STRUCTS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor takes pin as input
LoadDriverPin::LoadDriverPin(int pin) {
  this->pin = pin;
  this->lastActivation = 0;
  pinMode(pin, OUTPUT);
}

// Getter for pin
int LoadDriverPin::getPin() const {
  return pin;
}

// Getter for lastActivation
long LoadDriverPin::getLastActivation() const {
  return lastActivation;
}

// Setter for lastActivation
void LoadDriverPin::setLastActivation() {
  lastActivation = millis();
}

// initialize Hardware pins
LoadDriverPin controller_pins[max_groups] =
{
  {0},
  {1},
  {2}, //group 0
  {3}, //group 1
  {4},
  {5},
  {6},
  {7},
  {8}, //group 2
  {9}, //group 3
  {10}, //group 4
  {11}, //group 5
  {12},
  {13},
  {14},
  {15}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS IrrigationController
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// NEW Constructor:
IrrigationController::IrrigationController(
                      const char path[PATH_LENGTH],
                      const char keyName[MAX_GROUP_LENGTH]
): is_set(false), timetable(0), watering(0), water_time(0), driver_pins(nullptr), name("NV"){
  // Set the name of the group
  strncpy(name, keyName, MAX_GROUP_LENGTH - 1);
  name[MAX_GROUP_LENGTH - 1] = '\0';  // Ensure null-termination

  // try set rest of the class
  bool cond = this->loadScheduleConfig(path, name);
  if(cond){
    return;
  }

  // else set empty
  this->is_set = false;
  this->driver_pins = nullptr;
  this->timetable = 0;
  //this->watering = 0;
  this->water_time = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEFAULT Constructor seting an empty class
IrrigationController::IrrigationController(): is_set(false), timetable(0), watering(0), water_time(0), driver_pins(nullptr), name("NV"){
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEFAULT Destructor free up dynamic allocated memory
// In case of problems just use jsonArray instead of dynamic arrays via pointers
IrrigationController::~IrrigationController() {
  delete[] driver_pins;  // Free the memory for driver_pins array
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int IrrigationController::readyToWater(int currentHour) {
  // INPUT: currentHour is the int digits of the full hour
  // Function:
  // It checks if the irrigation system is ready to water.
  // If the system is ready,
  // it calculates the remaining watering time and updates the waterTime variable and returns the active time.
  // It returns 0 if the system is not ready to water.

  // to prevent unexpected behaviour its important to pass only valid hours
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
  unsigned long currentMillis = millis();
  int len = sizeof(driver_pins)/sizeof(int);

  // check each
  for(int i=0; i<len; i++){
    int vPin = driver_pins[i];

    // check if vpin is configured
    if(vPin<(int)max_groups){
      #ifdef DEBUG
      Serial.print(F("Selected pin not configured: ")); Serial.println(vPin);
      #endif
      return 0; // pin not configured
    }

    // check if configured pin is ready
    if(millis() - controller_pins[vPin].getLastActivation() > DRIVER_COOLDOWN){ //TODO: BUILD VPIN STRUCT INTO CLASS FILE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      #ifdef DEBUG
      Serial.print(F("Selected pin needs cooldown, ... skipping. Last activation (in sec): "));
      Serial.println((int)(millis() - controller_pins[vPin].getLastActivation())/1000);
      #endif
      return 0; // pin need cooldown
    }
  }

  // check if the solenoid has been active for too long, max_axtive_time_sec have to be multiplied as lastActivation is ms
  /*
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
  }*/

  // check if there is enough water time left
  if (this->water_time <= 0) {
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

  // save water time variable
  if (!saveScheduleConfig(CONFIG_FILE_PATH, this->name)){
    return 0; // saving variable error
  }

  // update the last activation time of the solenoid and pump
  // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  //solenoid->lastActivation = currentMillis;
  //pump->lastActivation = currentMillis;

  #ifdef DEBUG
  Serial.print("remaining time: "); Serial.println(remainingTime);
  #endif
  return remainingTime; //return active time
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads the JSON file at the specified file path and parses it to retrieve values for a number of member variables in the IrrigationController class.
// The member variables include is_set, name, timetable, watering, water_time, solenoid_pin, and pump_pin.
// Returns true if the file was read and parsed successfully, false if the file path is invalid or if there is an error reading or parsing the file.
bool IrrigationController::loadScheduleConfig(const char path[PATH_LENGTH], const char grp_name[MAX_GROUP_LENGTH]) {
  DynamicJsonDocument jsonDoc = Helper::readConfigFile(path); // read the config file
  if (jsonDoc.isNull()) {
    return false;
  }

  // Set the values of the member variables using the values from the JSON document
  this->is_set = jsonDoc["group"][grp_name]["is_set"].as<bool>();
  this->timetable = jsonDoc["group"][grp_name]["timetable"].as<uint32_t>();
  this->watering = jsonDoc["group"][grp_name]["watering"].as<int16_t>();
  this->water_time = jsonDoc["group"][grp_name]["water_time"].as<int16_t>();

  // dynamically allocate memory and fill up array
  JsonArray pinsArray = jsonDoc["group"][grp_name]["vpins"].as<JsonArray>();
  int numPins = pinsArray.size();
  this->driver_pins = new int[numPins];  // Allocate memory for the driver_pins array

  // Copy the pin values from the JSON array to the driver_pins array
  for (int i = 0; i < numPins; i++) {
    this->driver_pins[i] = pinsArray[i].as<int>();
  }

  //TODO REWORK PIN ARRAY

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates the values of a number of member variables in the IrrigationController class in the JSON file at the specified file path.
// Returns true if the file was updated successfully, false if the file path is invalid or if there is an error reading or writing the file.
bool IrrigationController::saveScheduleConfig(const char path[PATH_LENGTH], const char grp_name[MAX_GROUP_LENGTH]) {
  DynamicJsonDocument jsonDoc =  Helper::readConfigFile(path); // read the config file
  if (jsonDoc.isNull()) {
    return false;
  }

  // Update the values in the configuration file with the values of the member variables
  jsonDoc["group"][grp_name]["is_set"] = this->is_set;
  jsonDoc["group"][grp_name]["timetable"] = this->timetable;
  jsonDoc["group"][grp_name]["watering"] = this->watering;
  jsonDoc["group"][grp_name]["water_time"] = this->water_time;

  //jsonDoc["group"][grp_name]["vpins"] = this->vpins;

  return Helper::writeConfigFile(jsonDoc, path); // write the updated JSON data to the config file
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Resets all member variables to their default values
void IrrigationController::reset() {
  is_set = false;
  timetable = 0;
  watering = 0;
  water_time = 0;
  driver_pins = nullptr;
  delete[] driver_pins;  // Free the memory for driver_pins array
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO CHANGE TO WORK WITH PIN ARRAY
void IrrigationController::activatePWM(int time_s) {
  // Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  // FUNCTION PARAMETER:
  // time        -- time in seconds, max 60 seconds                                    int
  //------------------------------------------------------------------------------------------------
  // check if time is within range
  time_s = max(0, time_s);
  time_s = min(time_s, max_active_time_sec);

  //initialize state
  //uint8_t vent_pin = this->solenoid_pin;
  //uint8_t pump_pin = this->pump_pin;
  digitalWrite(sw_3_3v, LOW);
  digitalWrite(sh_cp_shft, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(data_shft, LOW);
  #ifdef DEBUG
  Serial.print(F("uint time:")); Serial.println(time_s);
  #endif
  digitalWrite(sw_3_3v, HIGH);
  unsigned long time_ms = (unsigned long)time_s * 1000UL;
  if (time_ms > (unsigned long)max_active_time_sec * 1000UL){
    time_ms = (unsigned long)max_active_time_sec * 1000UL;
    #ifdef DEBUG
    Serial.println(F("time warning: time exceeds sec"));
    #endif
  }

  // seting shiftregister to 0
  Helper::shiftvalue8b(0);
  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(this->name); Serial.print(F("time: ")); Serial.println(time_ms);
  #endif
  // perform actual function
  // byte value = (1 << vent_pin) + (1 << pump_pin);

  //byte value = solenoid == nullptr ? 0 : ((1 << solenoid->pin) | (pump == nullptr ? 0 : (1 << pump->pin)));

  byte value = 0; // TEMPORARY DUMMY VALUE

  #ifdef DEBUG
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif
  Helper::shiftvalue8b(value);
  delay(100); //wait balance load after pump switches on
  // control this PWM pin by changing the duty cycle:
  // ledcWrite(PWM_Ch, DutyCycle);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 1);
  delay(2);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.96);
  delay(2);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.92);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.9);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.88);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.87);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.86);
  delay(time_ms);

  // reset
  // seting shiftregister to 0
  Helper::shiftvalue8b(0);

  digitalWrite(sh_cp_shft, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(data_shft, LOW);
  digitalWrite(sw_3_3v, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO CHANGE FUNCTION TO WORK WITH NEW PIN ARRAY
void IrrigationController::activate(int time_s) {
  // Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  // FUNCTION PARAMETER:
  // time        -- time in seconds, max 40 seconds                                    int
  //------------------------------------------------------------------------------------------------
  // check if time is within range
  time_s = max(0, time_s);
  time_s = min(time_s, max_active_time_sec);

  //initialize state
  //uint8_t vent_pin = this->solenoid_pin;
  //uint8_t pump_pin = this->pump_pin;
  digitalWrite(sw_3_3v, LOW);
  digitalWrite(sh_cp_shft, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(data_shft, LOW);
  #ifdef DEBUG
  Serial.print(F("uint time:")); Serial.println(time_s);
  #endif
  digitalWrite(sw_3_3v, HIGH);
  unsigned long time_ms = (unsigned long)time_s * 1000UL;
  if (time_ms > (unsigned long)max_active_time_sec * 1000UL){
    time_ms = (unsigned long)max_active_time_sec * 1000UL;
    #ifdef DEBUG
    Serial.println(F("time warning: time exceeds sec"));
    #endif
  }

  // seting shiftregister to 0
  Helper::shiftvalue8b(0);
  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(this->name); Serial.print(F("time: ")); Serial.println(time_ms);
  #endif
  // perform actual function
  //byte value = (1 << vent_pin) + (1 << pump_pin);

  //byte value = solenoid == nullptr ? 0 : ((1 << solenoid->pin) | (pump == nullptr ? 0 : (1 << pump->pin)));

  byte value = 0; // TEMPORARY DUMMY VARIABLE

  #ifdef DEBUG
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif
  Helper::shiftvalue8b(value);

  delay(100); //wait balance load after pump switches on

  // Wait for the specified time
  delay(time_ms);

  // reset
  // seting shiftregister to 0
  Helper::shiftvalue8b(0);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Member function to start watering process
int IrrigationController::waterOn(int hour) {
  Serial.print("asd");
  // Function description: Starts the irrigation procedure after checking if the hardware is ready
  // FUNCTION PARAMETER:
  // currentHour - the active hour to check the with the timetable

  // Check if hardware is ready to water
  int active_time = readyToWater(hour);
  Serial.print("asd");
  if (active_time > 0) {
    // Activate watering process
    activatePWM(active_time);
    return 1;
  }
  return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define the implementation of the combineTimetables() static member function
long IrrigationController::combineTimetables()
{
  // Initialize the combined timetable to 0.
  long combinedTimetable = 0;

  // load config
  DynamicJsonDocument doc(CONF_FILE_SIZE);
  doc = Helper::readConfigFile(CONFIG_FILE_PATH);

  // Access the "groups" object
  JsonObject groups = doc["group"];
  doc.clear();

  for (JsonObject::iterator it = groups.begin(); it != groups.end(); ++it) {
    // validate if key is something that could be used
    JsonObject groupItem = it->value();
    bool condition = groupItem["is_set"];
    if(condition){
      continue; // skip if not set
    }
    long timetable = groupItem["timetable"];
    combinedTimetable |= timetable;
  }
  
  return combinedTimetable;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////