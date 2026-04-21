////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// see gitHub for author info
//
// This file contains the functionality for the IrrigationController class, which is responsible for managing and
// controlling an irrigation system. It includes functions for creating and configuring irrigation controllers,
// updating the controller based on current conditions, and determining when it is time to water.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <IrrigationController.h>
#include <LogFire.h>

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
  {15},
  {16},
  {17},
  {18},
  {19},
  {20},
  {21},
  {22},
  {23},
  {24},
  {25},
  {26},
  {27},
  {28},
  {29},
  {30},
  {31}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS IrrigationController
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// DEFAULT Constructor seting an empty class
IrrigationController::IrrigationController()
    : is_set(false), timetable(0), watering(0), water_time(0), weather_multiplier(1.0f), override_mode(false), name("NV") {
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

  if (!this->is_set) return 0;
  if (this->watering == 0) return 0;

  // Check each driver pin is valid and past cooldown
  for (const auto& pinValue : this->driver_pins) {
    if (pinValue > static_cast<int>(max_groups)) {
      LogFire.log("group \"" + String(this->name) + "\": pin " + String(pinValue) + " exceeds max_groups, aborting", 3);
      return 0;
    }
    if (millis() - controller_pins[pinValue].getLastActivation() < DRIVER_COOLDOWN) {
      int waited = (int)((millis() - controller_pins[pinValue].getLastActivation()) / 1000);
      LogFire.log("group \"" + String(this->name) + "\": cooldown pin=" + String(pinValue) + " waited=" + String(waited) + "s", 0);
      return -1;
    }
  }

  if (this->watering <= 0) return 0;

  int remainingTime = min(this->watering, max_active_time_sec);
  remainingTime = max(remainingTime, (int)0);
  return remainingTime;
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
  this->is_set = bool(groupData["ps"].as<int16_t>());
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

  // Weather multiplier (optional, default 1.0 for backward compatibility)
  if (groupData.containsKey("wm")) {
    this->weather_multiplier = constrain(groupData["wm"].as<float>(), 0.0f, 2.0f);
  } else {
    this->weather_multiplier = 1.0f;
  }

  loadDuty(this->key);

  // verify instance safety
  if (driver_pins.empty()) {
    LogFire.log("init: group \"" + String(this->name) + "\" has no driver pins (pp), resetting", 3);
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
  const char* gkey = groupPair.key().c_str();

  if (!groupData.containsKey("pn") || !groupData["pn"].is<String>() || groupData["pn"].as<String>().length() == 0) {
    LogFire.log("verify: group[" + String(gkey) + "] 'pn' missing/empty", 2);
    return false;
  }

  if (!groupData.containsKey("ps") || !groupData["ps"].is<int>()) {
    LogFire.log("verify: group[" + String(gkey) + "] 'ps' missing/invalid", 2);
    return false;
  }

  if (!groupData.containsKey("pw") || !groupData["pw"].is<uint32_t>()) {
    LogFire.log("verify: group[" + String(gkey) + "] 'pw' missing/invalid", 2);
    return false;
  }

  if (!groupData.containsKey("wt") || !groupData["wt"].is<uint32_t>()) {
    LogFire.log("verify: group[" + String(gkey) + "] 'wt' missing/invalid", 2);
    return false;
  }

  if (!groupData.containsKey("pp") || !groupData["pp"].is<JsonArray>()) {
    LogFire.log("verify: group[" + String(gkey) + "] 'pp' missing/not array", 2);
    return false;
  } else {
    JsonArray pinsArray = groupData["pp"].as<JsonArray>();
    if (pinsArray.size() == 0) {
      LogFire.log("verify: group[" + String(gkey) + "] 'pp' array empty", 2);
      return false;
    }
    for (JsonVariant pin : pinsArray) {
      if (!pin.is<int>()) {
        LogFire.log("verify: group[" + String(gkey) + "] 'pp' non-int element", 2);
        return false;
      }
    }
  }

  if (!groupData.containsKey("pls") || !groupData["pls"].is<float>()) {
    LogFire.log("verify: group[" + String(gkey) + "] 'pls' missing/invalid", 2);
    return false;
  }

  if (!groupData.containsKey("pts") || !groupData["pts"].is<float>()) {
    LogFire.log("verify: group[" + String(gkey) + "] 'pts' missing/invalid", 2);
    return false;
  }

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates the values of a number of member variables in the IrrigationController class in the JSON file at the specified file path.
// Returns true if the file was updated successfully, false if the file path is invalid or if there is an error reading or writing the file.
bool IrrigationController::saveScheduleConfig(const char path[PATH_LENGTH], const char grp_name[MAX_GROUP_LENGTH]) {
  DynamicJsonDocument jsonDoc = HWHelper.readConfigFile(path); // read the config file
  if (jsonDoc.isNull()) {
    LogFire.log("saveScheduleConfig: read failed for " + String(path), 3);
    return false;
  }

  if (!jsonDoc.containsKey(grp_name)) {
    LogFire.log("saveScheduleConfig: group \"" + String(grp_name) + "\" not found in " + String(path), 2);
    return false;
  }

  // Update the values in the configuration file with the values of the member variables
  jsonDoc[grp_name]["pn"] = this->name;
  jsonDoc[grp_name]["ps"] = int(this->is_set);
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
  weather_multiplier = 1.0f;
  override_mode = false;
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

  unsigned long time_ms = (unsigned long)time_s * 1000UL;
  if (time_ms > (unsigned long)max_active_time_sec * 1000UL){
    time_ms = (unsigned long)max_active_time_sec * 1000UL;
    LogFire.log("activate: time clamped to max " + String(max_active_time_sec) + "s", 1);
  }

  // seting shiftregister to 0
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);

  // perform actual function
  unsigned long value = 0;  // Initialize the value to 0
  for (int pin : this->driver_pins) {
      value |= (1 << pin);  // Set the bit at the pin number to 1 using bitwise OR
  }

  LogFire.log("activate: group \"" + String(this->name) + "\" shiftout=0b" + String(value, BIN) + " time=" + String(time_ms) + "ms", 0);

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
int IrrigationController::watering_task_handler(const struct tm& localTime) {
  // Function description: Starts the irrigation procedure after checking if the hardware is ready
  // FUNCTION PARAMETER:
  // localTime - current time struct, read once by caller and shared across all groups (avoids N I2C reads)
  // returns - int 1 if not finished and 0 if finished
  // call this function every iteration of the watering loop to advance the group's watering state
  byte hour = localTime.tm_hour, day = localTime.tm_mday;

  if(!this->is_set){
    LogFire.log("group \"" + String(this->name) + "\": not configured, skip", 0);
    return 0;
  }

  // Manual override: combination with scheduled is handled at detection time (checkOverrides),
  // not here. watering already holds the correct combined or override-only duration.
  if (this->override_mode) {
    LogFire.log("group \"" + String(this->name) + "\": override=" + String(this->watering) + "s", 1);
  } else {
    //lastDay = 0; lastHour = 0; // TESTING/DEBUGING
    // check if group is set for this hour
    bool correcthour = (this->timetable & (1 << hour)) != 0;
    if(!correcthour){
      LogFire.log("group \"" + String(this->name) + "\": skip (not scheduled h=" + String(hour) + ")", 0);
      return 0;
    }

    // look for new hour and update runtimevariables
    bool timechange = (this->lastDay != day) || (this->lastHour != hour);
    if(timechange){
      this->lastDay = day;
      this->lastHour = hour;

      this->watering = (int)(this->water_time * this->weather_multiplier);
      if (this->watering < 0) this->watering = 0;
      LogFire.log("group \"" + String(this->name) + "\": wm=" + String(this->weather_multiplier, 2) + " base=" + String(this->water_time) + "s -> " + String(this->watering) + "s", 1);
    }
  }

  // get info if system is allowed to water
  int active_time = readyToWater();

  if (active_time == 0) {
    return 0;
  }

  if (active_time < 0) {
    return 1;
  }

  if (active_time > 0) {
    // update the last activation time of the pins when everything is fine
    for (const auto& pinValue : this->driver_pins) {
        controller_pins[pinValue].setLastActivation();
    }

    // update watering variable
    this->watering = this->watering - active_time; // update the water time
    if (this->watering < 0) this->watering = 0;

    // Log just before solenoids open — WiFi managed by Phase 4 caller
    {
      String pins = "";
      for (int pin : this->driver_pins) {
        if (pins.length()) pins += ",";
        pins += String(pin);
      }
      LogFire.log("group \"" + String(this->name) + "\": activate pins=[" + pins + "] for " + String(active_time) + "s", 1);
    }

    // Activate watering process
    activate(active_time);

    // Save runtime state when group finishes. plantConfig fields haven't changed so
    // saveScheduleConfig() would just read+checksum+skip — call saveDuty() directly.
    if (this->watering == 0) {
      if (!saveDuty(this->key)){
        LogFire.log("group \"" + String(this->name) + "\": saveDuty failed", 3);
      }
    }
  }

  if(watering == 0){
    LogFire.log("group \"" + String(this->name) + "\": done", 1);
    return 0;
  }

  return 1; // not finished
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

    // Manual override: use dty directly, but still load up[] so the additive
    // scheduled-hour logic in watering_task_handler knows which hours already ran.
    if (groupData.containsKey("ovr") && groupData["ovr"].as<int>() == 1) {
      this->override_mode = true;
      this->watering = groupData.containsKey("dty") ? groupData["dty"].as<int>() : 0;
      JsonArray upArr = groupData.containsKey("up") ? groupData["up"].as<JsonArray>() : JsonArray();
      this->lastHour = upArr.size() > 0 ? upArr[0].as<byte>() : 0;
      this->lastDay = upArr.size() > 1 ? upArr[1].as<byte>() : 0;
      return;
    }

    // Check for "dty" and "up" keys and set default values if not present
    this->watering = groupData.containsKey("dty") ? groupData["dty"].as<int16_t>() : 0;
    JsonArray timestampArr = groupData.containsKey("up") ? groupData["up"].as<JsonArray>() : jsonDoc.createNestedArray("up");
    this->lastHour = timestampArr.size() > 0 ? timestampArr[0] : 0;
    this->lastDay = timestampArr.size() > 1 ? timestampArr[1] : 0;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// function to init running file & update use this funciton to set a irrigation duty
bool IrrigationController::saveDuty(const char* objkey) {
  const char* RUNTIME_FILE_PATH = RUNNING_FILE_PATH;
  DynamicJsonDocument jsonDoc(CONF_FILE_SIZE);

  // load unning file
  jsonDoc = HWHelper.readConfigFile(RUNTIME_FILE_PATH);
  if (!SPIFFS.exists(RUNTIME_FILE_PATH)) {
    LogFire.log("saveDuty: creating new duty file " + String(RUNTIME_FILE_PATH), 0);
    HWHelper.createFile(RUNTIME_FILE_PATH);
  }
  if (jsonDoc.isNull()) {
    LogFire.log("saveDuty: duty file empty/unreadable, starting fresh doc", 2);
    jsonDoc.to<JsonObject>();
  }

  // Update or set the "dty" and "up" keys
  jsonDoc[objkey]["dty"] = this->watering;
  jsonDoc[objkey].remove("up");
  JsonArray timestampArr = jsonDoc[objkey].createNestedArray("up");
  timestampArr.add(this->lastHour);
  timestampArr.add(this->lastDay);

  // Clear override flag when watering is finished
  if (this->watering == 0 && jsonDoc[objkey].containsKey("ovr")) {
    jsonDoc[objkey].remove("ovr");
  }

  // Write the updated JSON data to the config file
  HWHelper.writeConfigFile(jsonDoc, RUNTIME_FILE_PATH);

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////