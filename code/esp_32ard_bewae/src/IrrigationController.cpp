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

//#define DEBUG

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HARDWARE STRUCTS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor takes pin as input
LoadDriverPin::LoadDriverPin(int pin) {
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
  {0}, //reserve pump
  {1}, //pump
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

// DEFAULT Constructor seting an empty class
IrrigationController::IrrigationController()
    : is_set(false), timetable(0), watering(0), water_time(0), name("NV") {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEFAULT Destructor free up dynamic allocated memory
IrrigationController::~IrrigationController() {
  // No need to delete[] driver_pins as it is handled automatically by std::vector
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function:
// check if hardware is ready for watering, returns:
// - (time in sec) allowed time to be active
// - -1 if the system is not ready to water
// - 0 if group is not configured at that time or in error case
int IrrigationController::readyToWater() {
// INPUT: currentHour int of full hour
//        currentDay int of day of month

  // check if group is set
  if(!this->is_set){ //return 0 if the group is not set
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(this->name); Serial.println(F("' not set"));
    #endif
    return 0;
  }

  // check if there is something to do (probably repeating call)
  if (this->watering == 0) {
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(this->name); 
    Serial.println(F("' nothing to do (old call?)"));
    #endif
    return 0; // done or nothing for this time
  }

  // get the current time in milliseconds
  unsigned long currentMillis = millis();

  // Accessing the elements of driver_pins using a range-based for loop
  for (const auto& pinValue : this->driver_pins) {
      // Check each pin
      if (pinValue > static_cast<int>(max_groups)) {
          #ifdef DEBUG
          Serial.print(F("Selected pin not configured: "));
          Serial.println(pinValue);
          #endif
          return 0; // Pin not configured
      }

      // Check for cooldown of the pin
      if (millis() - controller_pins[pinValue].getLastActivation() < DRIVER_COOLDOWN) {
          #ifdef DEBUG
          Serial.print(F("Selected pin needs cooldown, skipping. Last activation (in sec): "));
          Serial.println(static_cast<int>((millis() - controller_pins[pinValue].getLastActivation()) / 1000));
          #endif
          return -1; // Pin needs cooldown
      }
  }

  // check if there is enough water time left
  if (this->watering <= 0) {
    return 0; // group done, info handled in watering_task_handler
  }

  // calculate the remaining watering time, return seconds using min function
  int remainingTime = min(this->watering, max_active_time_sec);
  remainingTime = max(remainingTime, (int)0); //avoid values beyond 0

  // return the remaining time which the pins should be active
  #ifdef DEBUG
  Serial.println();
  Serial.print("watering time: "); Serial.println(remainingTime);
  #endif
  return remainingTime; // return active time
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads the JSON & assign member variables in the IrrigationController class.
// Returns true if the file was read and parsed successfully, false if the file path is invalid or if there is an error reading or parsing the file.
bool IrrigationController::loadScheduleConfig(const JsonPair& groupPair) {
  JsonObject groupData = groupPair.value();

  bool verify = verifyScheduleConfig(groupPair);
  if (!verify) {
    IrrigationController::reset();
    return 0;
  }

  this->key = groupPair.key().c_str(); // Get the key from the JsonPair

  String groupName = groupData["pn"].as<String>();
  strncpy(this->name, groupName.c_str(), MAX_GROUP_LENGTH);
  this->name[MAX_GROUP_LENGTH] = '\0';  // Ensure null-termination

  // Extract the values from the groupData object
  this->is_set = groupData["ps"].as<bool>();
  this->timetable = groupData["wt"].as<uint32_t>();
  this->water_time = groupData["pw"].as<int16_t>();
  JsonArray pinsArray = groupData["pp"].as<JsonArray>();
  driver_pins.clear(); // Clear the vector before populating it
  // Access the "vpins" array and populate the vector
  for (JsonVariant value : pinsArray) {
    driver_pins.push_back(value.as<int>());
  }
  this->plant_size = groupData["pls"].as<int16_t>();
  this->pot_size = groupData["pts"].as<int16_t>();

  loadDuty(this->key);

  // verify instance safety
  if (driver_pins.empty()) {
    #ifdef DEBUG
    Serial.print(F("Error: while init class vector (pp - plant pins)"));
    #endif
    IrrigationController::reset();
    return 0;
  }

  // return true if everything was ok
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Verify Configfile
bool IrrigationController::verifyScheduleConfig(const JsonPair& groupPair) {
  JsonObject groupData = groupPair.value();

  // Verify the group name and ensure it's not an empty string
  if (!groupData.containsKey("pn") || !groupData["pn"].is<String>() || groupData["pn"].as<String>().length() == 0) {
    #ifdef DEBUG
    Serial.println(F("Group name 'pn' is missing, not a string, or is empty."));
    #endif
    return false;
  }

  // Verify the schedule set flag
  if (!groupData.containsKey("ps") || !groupData["ps"].is<int>()) {
    #ifdef DEBUG
    Serial.println(F("Schedule set flag 'ps' is missing or not a boolean."));
    #endif
    return false;
  }

  // Verify the timetable
  if (!groupData.containsKey("pw") || !groupData["pw"].is<uint32_t>()) {
    #ifdef DEBUG
    Serial.println(F("Timetable 'pw' is missing or not an unsigned 32-bit integer."));
    #endif
    return false;
  }

  // Verify the water time
  if (!groupData.containsKey("wt") || !groupData["wt"].is<uint32_t>()) {
    #ifdef DEBUG
    Serial.println(F("Water time 'wt' is missing or not a 32-bit integer."));
    #endif
    return false;
  }

  // Verify the driver pins array, if all elements are convertible to an int, and if the array is not empty
  if (!groupData.containsKey("pp") || !groupData["pp"].is<JsonArray>()) {
    #ifdef DEBUG
    Serial.println(F("Driver pins 'pp' are missing or not an array."));
    #endif
    return false;
  } else {
    JsonArray pinsArray = groupData["pp"].as<JsonArray>();
    if (pinsArray.size() == 0) {
      #ifdef DEBUG
      Serial.println(F("Driver pins array 'pp' is empty."));
      #endif
      return false;
    }
    for (JsonVariant pin : pinsArray) {
      if (!pin.is<int>()) {
        #ifdef DEBUG
        Serial.println(F("An element in the driver pins array 'pp' is not convertible to an int."));
        #endif
        return false;
      }
    }
  }

  // Verify the plant size
  if (!groupData.containsKey("pls") || !groupData["pls"].is<float>()) {
    #ifdef DEBUG
    Serial.println(F("Plant size 'pls' is missing or not a 16-bit integer."));
    #endif
    return false;
  }

  // Verify the pot size
  if (!groupData.containsKey("pts") || !groupData["pts"].is<float>()) {
    #ifdef DEBUG
    Serial.println(F("Pot size 'pts' is missing or not a 16-bit integer."));
    #endif
    return false;
  }

  // If all checks pass, return true
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates the values of a number of member variables in the IrrigationController class in the JSON file at the specified file path.
// Returns true if the file was updated successfully, false if the file path is invalid or if there is an error reading or writing the file.
bool IrrigationController::saveScheduleConfig(const char path[PATH_LENGTH], const char grp_name[MAX_GROUP_LENGTH]) {
  DynamicJsonDocument jsonDoc = HWHelper.readConfigFile(path); // read the config file
  // check jsonDoc
  if (jsonDoc.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Warning: Json config not saved!"));
    #endif
    return false;
  }

  // check name
  if (!jsonDoc.containsKey(grp_name)) {
  #ifdef DEBUG
  Serial.println(F("Warning: No group not found!"));
  #endif
  return false;
  }

  // Update the values in the configuration file with the values of the member variables
  jsonDoc[grp_name]["pn"] = this->name;
  jsonDoc[grp_name]["ps"] = this->is_set;
  jsonDoc[grp_name]["wt"] = this->timetable;
  jsonDoc[grp_name]["pw"] = this->water_time;

  // Saving vpins: Clear the existing vpins array
  jsonDoc[grp_name]["pp"].clear();
  // Populate the vpins array with the values from the driver_pins vector
  for (int pin : driver_pins) {
    jsonDoc[grp_name]["pp"].add(pin);
  }
/*
  // save lastupdate array
  jsonDoc[grp_name]["dty"] = this->watering;
  jsonDoc[grp_name]["up"][0] = this->lastHour;
  jsonDoc[grp_name]["up"][1] = this->lastDay;*/

  saveDuty(this->key);

  // return bool to indicate if status failed
  return HWHelper.writeConfigFile(jsonDoc, path); // write the updated JSON data to the config file
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

void IrrigationController::activate(int time_s) {
  // Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  // FUNCTION PARAMETER:
  // time        -- time in seconds, max 40 seconds                                    int
  //------------------------------------------------------------------------------------------------
  // check if time is within range
  time_s = max(0, time_s);
  time_s = min(time_s, max_active_time_sec);

  #ifdef DEBUG
  Serial.print(F("uint time:")); Serial.println(time_s);
  #endif

  unsigned long time_ms = (unsigned long)time_s * 1000UL;
  if (time_ms > (unsigned long)max_active_time_sec * 1000UL){
    time_ms = (unsigned long)max_active_time_sec * 1000UL;
    #ifdef DEBUG
    Serial.println(F("time warning: time exceeds sec"));
    #endif
  }

  // seting shiftregister to 0
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);

  // perform actual function
  unsigned long value = 0;  // Initialize the value to 0
  for (int pin : this->driver_pins) {
      value |= (1 << pin);  // Set the bit at the pin number to 1 using bitwise OR
  }

  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(this->name); Serial.print(F("time: ")); Serial.println(time_ms);
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif

  // activate pins
  HWHelper.shiftvalue(value, max_groups, INVERT_SHIFTOUT);

  delay(100); //balance load after pump switches on

  // set start point of active phase
  unsigned long activephase = millis();
  
  // IMPORTANT: wait until process finished, DO NOT manipulate shiftout pins,
  // use function with long runtime or long delays and be careful with interupts.
  // This could cause unexpected and unwanted behaviour
  while (millis() - activephase < time_ms) {
      // perform other functions here
      delay(1);
  }

  // reset
  // seting shiftregister to 0
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);

// TODO: Test if block below is not needed if, -> remove!
//  digitalWrite(sh_cp_shft, LOW); //make sure clock is low so rising-edge triggers
//  digitalWrite(st_cp_shft, LOW);
//  digitalWrite(data_shft, LOW);
//  digitalWrite(sw_3_3v, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Public function: Handling watering process calling related functionality, seting variables saving config
int IrrigationController::watering_task_handler() {
  // Function description: Starts the irrigation procedure after checking if the hardware is ready
  // FUNCTION PARAMETER:
  // currentHour - the active hour to check the with the timetable
  // currentDay - the active day
  // returns - int 1 if not finished and 0 if finished
  // call this function at least once an hour to update information on what is already done  
  // check for hour change and shift water_time base value into watering value to be processed
  struct tm localTime;
  bool status = HWHelper.readTime(&localTime);
  byte minute = localTime.tm_min, hour = localTime.tm_hour, day = localTime.tm_mday;

  // if not set return early
  if(!this->is_set){
    #ifdef DEBUG
    Serial.print(F("Group '")); Serial.print(this->name); Serial.println(F("' not set!"));
    #endif
    return 0;
  }

  //lastDay = 0; lastHour = 0; // TESTING/DEBUGING
  // check if group is set for this hour
  bool correcthour = (this->timetable & (1 << hour)) != 0;
  if(!correcthour){
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(this->name); Serial.println(F("' nothing to do this time"));
Serial.print("TODO: timetable = "); Serial.println(this->timetable);
    #endif
    return 0; // nothing to do this time
  }

  // look for new hour and update runtimevariables
  bool timechange = (this->lastDay != day) || (this->lastHour != hour);
  if(timechange){
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(this->name); Serial.println(F("' hour change detected"));
    #endif
    // set new update timestamp
    this->lastDay = day;
    this->lastHour = hour;

    // write water_time to watering when starting watering process
    this->watering = this->water_time; // multiply with factor to adjust wheater conditons when raspi not reachable?
  }

  // get info if system is allowed to water
  int active_time = readyToWater();

  // check if fnished
  if (active_time == 0) {
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(this->name); Serial.println(F("' checked!"));
    #endif
    return 0;
  }

  // check if it should wait
  if (active_time < 0) {
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(this->name); Serial.println(F("' need cooldown!"));
    #endif
    return 1;
  }

  // handle intended case when system is ready to water
  if (active_time > 0) {
    // Print info to serial if wanted
    #ifdef DEBUG
    Serial.print(F("Watering group: "));
    Serial.print(key);
    Serial.print(F(" - With name: "));
    Serial.println(name);
    Serial.print(F("driver_pins values: "));
    for (int pin : driver_pins) {
      Serial.print(pin);
      Serial.print(F(", "));
    }
    Serial.println();
    Serial.print(F("Time: ")); Serial.println(active_time);
    #endif

    #ifdef DEBUG
    Serial.print(F("Set activationtimestamp to pin: "));
    #endif
    // update the last activation time of the pins when everything is fine
    for (const auto& pinValue : this->driver_pins) {
        controller_pins[pinValue].setLastActivation();
        #ifdef DEBUG
        Serial.print(pinValue); Serial.print(F(" "));
        #endif
    }
    #ifdef DEBUG
    Serial.println();
    #endif

    // update watering variable
    this->watering = this->watering - active_time; // update the water time

    String path = String(IRRIG_CONFIG_PATH) + String(JSON_SUFFIX);

    // save water time variable
    if (!saveScheduleConfig(path.c_str(), this->key)){ // WARNING: calling this too ofthen could wear out flash memory
      #ifdef DEBUG
      Serial.println(F("Failed to save status!"));
      #endif
      return 0; // saving variable error
    }

    // Activate watering process
    activate(active_time);
  }

  // check if group is NOT done
  if(watering!=0){
    return 1;
  }
  // check if group is done
  if(watering == 0){
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(name); Serial.println(F("' finished! OLD STATEMENT"));
    #endif
    return 0;
  }

  #ifdef DEBUG
  Serial.println(F("Warning: something unhandeled occured in warterOn!"));
  #endif

  return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define the implementation of the combineTimetables() static member function
long IrrigationController::combineTimetables() {
  // Initialize the combined timetable to 0.
  long combinedTimetable = 0;

  // load config
  String path = String(IRRIG_CONFIG_PATH) + String(JSON_SUFFIX);
  DynamicJsonDocument doc(CONF_FILE_SIZE);
  doc = HWHelper.readConfigFile(path.c_str());

  // Access the "groups" object
  JsonObject groups = doc.as<JsonObject>();
  doc.clear();
  for (JsonObject::iterator it = groups.begin(); it != groups.end(); ++it) {
    // validate if key is something that could be used
    JsonObject groupItem = it->value();
    bool condition = groupItem["ps"];
    if(!condition){
      continue; // skip if not set
    }
    long timetable = groupItem["wt"];
    combinedTimetable |= timetable;
  }
  return combinedTimetable;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// function to init running file & update use this funciton to set a irrigation duty
void IrrigationController::loadDuty(const char* objkey) {
  const char* RUNTIME_FILE_PATH = RUNNING_FILE_PATH;
  // Check if running file exists and load it

    DynamicJsonDocument jsonDoc = HWHelper.readConfigFile(RUNTIME_FILE_PATH);
    if (jsonDoc.isNull() || !jsonDoc.containsKey(objkey)) {
      // Set default values if file doesn't exist
      this->watering = 0;
      this->lastHour = 0;
      this->lastDay = 0;
      return;
    }

    JsonObject groupData = jsonDoc.as<JsonObject>()[objkey];

    // Check for "dty" and "up" keys and set default values if not present
    this->watering = groupData.containsKey("dty") ? groupData["dty"].as<int16_t>() : 0;
    JsonArray timestampArr = groupData.containsKey("up") ? groupData["up"].as<JsonArray>() : jsonDoc.createNestedArray("up");
    this->lastHour = timestampArr.size() > 0 ? timestampArr[0] : 0;
    this->lastDay = timestampArr.size() > 1 ? timestampArr[1] : 0;

    // Add water_time to watering
    this->watering += this->water_time;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// function to init running file & update use this funciton to set a irrigation duty
bool IrrigationController::saveDuty(const char* objkey) {
  const char* RUNTIME_FILE_PATH = RUNNING_FILE_PATH;
  DynamicJsonDocument jsonDoc(CONF_FILE_SIZE);

  // load unning file
  jsonDoc = HWHelper.readConfigFile(RUNTIME_FILE_PATH);
  if (!SPIFFS.exists(RUNTIME_FILE_PATH)) {
    #ifdef DEBUG
    Serial.println(F("Info: Created Duty file (Saving)!"));
    #endif
    HWHelper.createFile(RUNTIME_FILE_PATH);
  }
  if (!jsonDoc.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Warning: Duty file was empty (Saving)!"));
    #endif
  }

  // Update or set the "dty" and "up" keys
  jsonDoc[objkey]["dty"] = this->watering;
  jsonDoc[objkey].remove("up"); // ensure up is replaced
  JsonArray timestampArr = jsonDoc[objkey]["up"];
  timestampArr.add(this->lastHour);
  timestampArr.add(this->lastDay);

  // Write the updated JSON data to the config file
  HWHelper.writeConfigFile(jsonDoc, RUNTIME_FILE_PATH);

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////