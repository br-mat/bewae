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
      if (millis() - controller_pins[pinValue].getLastActivation()) {
          #ifdef DEBUG
          Serial.println(millis() - controller_pins[pinValue].getLastActivation() > DRIVER_COOLDOWN);
          Serial.print(F("Selected pin needs cooldown, skipping. Last activation (in sec): "));
          Serial.println(static_cast<int>((millis() - controller_pins[pinValue].getLastActivation()) / 1000));
          #endif
          return 0; // Pin needs cooldown
      }
Serial.print(controller_pins[pinValue].getPin()); Serial.println("pin ok");
  }

  // check if there is enough water time left
  if (this->water_time <= 0) {
    #ifdef DEBUG
    Serial.println("group done!");
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

  // Populate driver_pin vector
  JsonArray pinsArray = groupData["vpins"].as<JsonArray>();
  // Clear the vector before populating it
  driver_pins.clear();

  // Access the "vpins" array and populate the vector
  for (JsonVariant value : pinsArray) {
    driver_pins.push_back(value.as<int>());
  }

  // return true if everything was ok
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates the values of a number of member variables in the IrrigationController class in the JSON file at the specified file path.
// Returns true if the file was updated successfully, false if the file path is invalid or if there is an error reading or writing the file.
bool IrrigationController::saveScheduleConfig(const char path[PATH_LENGTH], const char grp_name[MAX_GROUP_LENGTH]) {
  DynamicJsonDocument jsonDoc =  Helper::readConfigFile(path); // read the config file
  // check jsonDoc
  if (jsonDoc.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Warning: json config could not saved correctly!"));
    #endif
    return false;
  }

  // check name
  if (!jsonDoc["group"].containsKey(grp_name)) {
  #ifdef DEBUG
  Serial.println(F("Warning: grp_name not found in jsonDoc!"));
  #endif
  return false;
  }

  // Update the values in the configuration file with the values of the member variables
  jsonDoc["group"][grp_name]["is_set"] = this->is_set;
  jsonDoc["group"][grp_name]["timetable"] = this->timetable;
  jsonDoc["group"][grp_name]["watering"] = this->watering;
  jsonDoc["group"][grp_name]["water-time"] = this->water_time;

  // Saving vpins: Clear the existing vpins array
  jsonDoc["group"][grp_name]["vpins"].clear();

  // Populate the vpins array with the values from the driver_pins vector
  for (int pin : driver_pins) {
    jsonDoc["group"][grp_name]["vpins"].add(pin);
  }

  // return bool to indicate if status failed
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
  digitalWrite(sw_3_3v, LOW);
  digitalWrite(sh_cp_shft, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(data_shft, LOW);
  #ifdef DEBUG
  Serial.print(F("uint time controll statement:")); Serial.println(time_s);
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

  // perform actual function
  unsigned long value = 0;  // Initialize the value to 0
  // iterate over driver_pins and set them
  for (int pin : this->driver_pins) {
    value |= (1 << pin);  // Set the bit at the pin number to 1 using bitwise OR
  }

  // set start point of active phase
  unsigned long activephase = millis();

  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(this->name); Serial.print(F("time in ms: ")); Serial.println(time_ms);
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif

  // activate pins
  Helper::shiftvalue(value, max_groups);
  delay(100); //balance load after pump switches on

  // control PWM pin by changing the duty cycle:
  // perform linear decay of duty cycle to save power
  unsigned long start_decay = millis();
  int decay_time = 1000; // decay time in milliseconds
  float dutyCycle = 1.0;
  float targetDutyCycle = 0.85;
  float decrement = 0.05;
  int numSteps = (float)(dutyCycle - targetDutyCycle) / decrement;
  int delayTime = decay_time / numSteps;
  Serial.println(numSteps);
  Serial.println(delayTime);

  for (int i = 0; i <= numSteps; i++) {
      ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * dutyCycle);
      dutyCycle -= decrement;
      // break if unexpected happened
      if ((start_decay + decay_time + 500) < millis()){
        #ifdef DEBUG
        Serial.println(F("Warning: Somthing unexpected happend during linear decay of PWM rate."));
        #endif
        break;
      }
      delay(delayTime);
  }

  // IMPORTANT: wait until process finished, DO NOT manipulate shiftout pins,
  // use function with long runtime or long delays and be careful with interupts.
  // This could cause unexpected and unwanted behaviour
  while (activephase + time_ms > millis())
  {
  // wait for calculated time and do something
  delay(1);
  }

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
  Helper::shiftvalue(value, max_groups);

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
  Helper::shiftvalue(0, max_groups);

  digitalWrite(sh_cp_shft, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(data_shft, LOW);
  digitalWrite(sw_3_3v, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Member function to start watering process
int IrrigationController::waterOn(int hour) {
  // Function description: Starts the irrigation procedure after checking if the hardware is ready
  // FUNCTION PARAMETER:
  // currentHour - the active hour to check the with the timetable
  // returns - int 1 if not finished and 0 if finished

  // Check if hardware is ready to water
  int active_time = readyToWater(hour);
  if (active_time > 0) {
    // Print info to serial if wanted
    #ifdef DEBUG
    Serial.print(F("Watering group: "));
    Serial.println(name);
    Serial.print("driver_pins values: ");
    for (int pin : driver_pins) {
      Serial.print(pin);
      Serial.print(", ");
    }
    Serial.println();
    Serial.print(F("Time: ")); Serial.println(active_time);
    #endif

    // Activate watering process
    activate(active_time);
  }

  // check if group is NOT done
  if((water_time!=0) && (is_set)){
    return 1;
  }
  // check if group is done
  if(water_time == 0){
    #ifdef DEBUG
    Serial.print(F("Group '"));
    Serial.print(name);
    Serial.println(F("' finished!"));
    #endif
    return 0;
  }

  #ifdef DEBUG
  Serial.println("Warning something unhandeled occured in warterOn!");
  #endif

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