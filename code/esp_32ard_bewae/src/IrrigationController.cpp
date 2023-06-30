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
    Serial.print("Creating LoadDriverPin object with pin: ");
  Serial.println(pin);
  vpin = pin;
  lastActivation = 0;
}

// Getter for pin
int LoadDriverPin::getPin() const {
  return vpin;
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
/*
// Alternative Constructor
IrrigationController::IrrigationController(const char path[PATH_LENGTH], const char keyName[MAX_GROUP_LENGTH])
    : is_set(false), timetable(0), watering(0), water_time(0) {
  // Set the name of the group
  strncpy(name, keyName, MAX_GROUP_LENGTH - 1);
  name[MAX_GROUP_LENGTH - 1] = '\0';  // Ensure null-termination

  // Load the schedule configuration for the specified group
  DynamicJsonDocument jsonDoc = Helper::readConfigFile(path); // read the config file
  JsonObject groups = jsonDoc["group"];

  // Check if the group data is valid
  if (groups.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Error: No 'Group' found in file!"));
    #endif
    return;
  }

  // Get the group data for the specified keyName
  JsonObject groupData = groups[keyName];

  // Check if the group data is valid and load the configuration
  if (!groupData.isNull() && loadScheduleConfig(groupData)) {
    return;
  }

  // If the configuration couldn't be loaded or the group data is invalid, set default values
  is_set = false;
  timetable = 0;
  driver_pins.clear();
  water_time = 0;
  watering = 0;
}*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEFAULT Constructor seting an empty class
IrrigationController::IrrigationController()
    : is_set(false), timetable(0), watering(0), water_time(0), name("NV") {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEFAULT Destructor free up dynamic allocated memory
// In case of problems just use jsonArray instead of dynamic arrays via pointers
IrrigationController::~IrrigationController() {
  // No need to delete[] driver_pins as it is handled automatically by std::vector
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

  // check if group is set
  if(!this->is_set){ //return 0 if the group is not set
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
  //int len = sizeof(driver_pins)/sizeof(int);
Serial.println("start looping over driver pin vector");

  // Accessing the elements of driver_pins using a range-based for loop
  for (const auto& pinValue : this->driver_pins) { // TODO: UNSIGNED INT FOR PINS FIX THIS ISSUE LATER
      // Check each pin
      if (pinValue > static_cast<int>(max_groups)) {
          #ifdef DEBUG
          Serial.print(F("Selected pin not configured: "));
          Serial.println(pinValue);
          #endif
          return 0; // Pin not configured
      }

      // Check for cooldown of the pin
      if (millis() - controller_pins[pinValue].getLastActivation() > DRIVER_COOLDOWN) {
          #ifdef DEBUG
          Serial.print(F("Selected pin needs cooldown, skipping. Last activation (in sec): "));
          Serial.println(static_cast<int>((millis() - controller_pins[pinValue].getLastActivation()) / 1000));
          #endif
          return 0; // Pin needs cooldown
      }
  }

  // check if there is enough water time left
  if (this->water_time <= 0) {
    #ifdef DEBUG
    Serial.println("no time left");
    #endif
    return 0; // not ready to water
  }

  // calculate the remaining watering time, return seconds using min function
  int remainingTime = min(water_time, max_active_time_sec);
  remainingTime = max(remainingTime, (int)0); //avoid values beyond 0
  this->water_time -= remainingTime; // update the water time

  // save water time variable
  if (!saveScheduleConfig(CONFIG_FILE_PATH, name)){
    #ifdef DEBUG
    Serial.println(F("Failed to save status!"));
    #endif
    return 0; // saving variable error
  }

  // update the last activation time of the pins when everything is fine
  for (const auto& pinValue : this->driver_pins) {
      controller_pins[pinValue].setLastActivation();
  }

  // return the remaining time which the pins should be active
  #ifdef DEBUG
  Serial.print("remaining time: "); Serial.println(remainingTime);
  #endif
  return remainingTime; //return active time
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads the JSON file at the specified file path and parses it to retrieve values for a number of member variables in the IrrigationController class.
// The member variables include is_set, name, timetable, watering, water_time, solenoid_pin, and pump_pin.
// Returns true if the file was read and parsed successfully, false if the file path is invalid or if there is an error reading or parsing the file.
bool IrrigationController::loadScheduleConfig(const JsonPair& groupPair) {
  JsonObject groupData = groupPair.value();

  // Check if the group data is valid
  if (groupData.isNull()) {
    #ifdef DEBUG
    Serial.print("Invalid group data provided when loading schedule configuration for group: ");
    Serial.println(groupPair.key().c_str());
    #endif
    return false;
  }

  // Extract the group name from the iterator
  String groupName = groupPair.key().c_str();

  // Set the group name in the IrrigationController instance
  strncpy(this->name, groupName.c_str(), MAX_GROUP_LENGTH - 1);
  this->name[MAX_GROUP_LENGTH - 1] = '\0';  // Ensure null-termination

  // Extract the values from the groupData object
  this->is_set = groupData["is_set"].as<bool>();
  this->timetable = groupData["timetable"].as<uint32_t>();
  this->watering = groupData["watering"].as<int16_t>();
  this->water_time = groupData["water-time"].as<int16_t>();

  // Print the key (name) and the four important statistics
  //Serial.print("Group Name: ");
  //Serial.println(groupName);
  //Serial.print("is_set: ");
  //Serial.println(this->is_set);
  //Serial.print("timetable: ");
  //Serial.println(this->timetable, BIN);
  //Serial.print("watering: ");
  //Serial.println(this->watering);
  //Serial.print("water_time: ");
  //Serial.println(this->water_time);

  // Populate driver_pin vector
  JsonArray pinsArray = groupData["vpins"].as<JsonArray>();
  // Clear the vector before populating it
  driver_pins.clear();

  // Access the "vpins" array and populate the vector
  for (JsonVariant value : pinsArray) {
    driver_pins.push_back(value.as<int>());
  }

  #ifdef DEBUG
  // Debug print the values in the vector
  Serial.println("driver_pins values:");
  for (int pin : driver_pins) {
    Serial.println(pin);
  }
  #endif

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates the values of a number of member variables in the IrrigationController class in the JSON file at the specified file path.
// Returns true if the file was updated successfully, false if the file path is invalid or if there is an error reading or writing the file.
bool IrrigationController::saveScheduleConfig(const char path[PATH_LENGTH], const char grp_name[MAX_GROUP_LENGTH]) {
Serial.print("Saving: "); Serial.println(grp_name);
  DynamicJsonDocument jsonDoc =  Helper::readConfigFile(path); // read the config file
  if (jsonDoc.isNull()) {
    return false;
  }

  // Update the values in the configuration file with the values of the member variables
  jsonDoc["group"][grp_name]["is_set"] = this->is_set;
  jsonDoc["group"][grp_name]["timetable"] = this->timetable;
  jsonDoc["group"][grp_name]["watering"] = this->watering;
  jsonDoc["group"][grp_name]["water-time"] = this->water_time;

  // save Vpins Create a new JSON array
  JsonArray newPinsArray = jsonDoc["group"][grp_name].createNestedArray("vpins");

  // Populate the new JSON array with the values from the driver_pins vector
  for (int i = 0; i < driver_pins.size(); i++) {
    newPinsArray.add(driver_pins[i]);
  }

  // Replace the old vpins entry with the new JSON array
  jsonDoc["group"][grp_name]["vpins"] = newPinsArray;

  return Helper::writeConfigFile(jsonDoc, path); // write the updated JSON data to the config file
  }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Resets all member variables to their default values
void IrrigationController::reset() {
  is_set = false;
  timetable = 0;
  watering = 0;
  water_time = 0;
  driver_pins.clear(); // Clear the elements of driver_pins vector
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO CHANGE TO WORK WITH PIN ARRAY
void IrrigationController::activatePWM(int time) {
  // Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  // FUNCTION PARAMETER:
  // time        -- time in seconds, max 60 seconds                                    int
  //------------------------------------------------------------------------------------------------
  // check if time is within range
  int time_s = 0;
  time_s = max(0, time);
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
  Helper::shiftvalue(0, max_groups);
  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(this->name); Serial.print(F("time: ")); Serial.println(time_ms);
  #endif

  // perform actual function
  unsigned long value = 0;  // Initialize the value to 0
  // iterate over driver_pins and set them
Serial.println("driverpins:");
  for (int pin : this->driver_pins) {
Serial.print(pin);
    value |= (1 << pin);  // Set the bit at the pin number to 1 using bitwise OR
  }
Serial.println();
Serial.print("value: "); Serial.println(value);

  #ifdef DEBUG
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif
  Helper::shiftvalue(value, max_groups);
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
  Helper::shiftvalue(0, max_groups);

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
  Helper::shiftvalue(0, max_groups);
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
  Helper::shiftvalue(value, max_groups);

  delay(100); //wait balance load after pump switches on

  // Wait for the specified time
  delay(time_ms);

  // reset
  // seting shiftregister to 0
  Helper::shiftvalue(0, max_groups);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Member function to start watering process
int IrrigationController::waterOn(int hour) {
  // Function description: Starts the irrigation procedure after checking if the hardware is ready
  // FUNCTION PARAMETER:
  // currentHour - the active hour to check the with the timetable
  /*
  #ifdef DEBUG
  Serial.print(F("Watering group: ")); Serial.println(name);
  Serial.print(F("is_set: ")); Serial.println(is_set);
  Serial.print(F("timetable: ")); Serial.println(timetable, BIN);
  Serial.print(F("current hour: ")); Serial.println(hour);
  #endif
  */
  // Check if hardware is ready to water
  int active_time = readyToWater(hour);
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