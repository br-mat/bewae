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

// Initialize the static variable
int Solenoid::activeInstances = 0;

// Default constructor
Solenoid::Solenoid() : pin(-1), lastActivation(0) {
  activeInstances++;
}

// Constructor that takes a pin number as input
Solenoid::Solenoid(int pin) : pin(pin), lastActivation(0) {
  activeInstances++;
}

// Destructor
Solenoid::~Solenoid() {
  activeInstances--;
}

// Member function to initialize the solenoid
void Solenoid::setup(int pin) {
  this->pin = pin;
  pinMode(pin, OUTPUT);
}

// Member function to initialize Pump object by assigning a pin
void Pump::setup(int pin){
  // Initialize pumps
  this->pin = pin;
  this->lastActivation = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS IrrigationController
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IrrigationController::IrrigationController(const char path[PATH_LENGTH], 
                      Solenoid* solenoid, //attatched solenoid
                      Pump* pump //attatched pump
                      ){
  this->solenoid = solenoid;
  this->pump = pump;
  bool cond = this->loadScheduleConfig(path, solenoid->pin);
  if(cond){
    return;
  }
  //else set values to 0
  this->is_set = false;
  this->name = "NaN";
  this->timetable = 0;
  this->watering = 0;
  this->water_time = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IrrigationController::createNewController(
                      bool is_set, //activate deactivate group
                      String name, //name of the group
                      unsigned long timetable,
                      int watering, //defualt value of watering amount set for group
                      Solenoid* solenoid, //attatched solenoid
                      Pump* pump, //attatched pump
                      int water_time //holds value of how long it should water
                      ){
  this->is_set = is_set;
  this->name = name;
  this->timetable = timetable;
  this->watering = watering;
  this->solenoid = solenoid;
  this->pump = pump;
  this->solenoid_pin = solenoid->pin;
  this->pump_pin = pump->pin;
  this->water_time = water_time;
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
  // unsigned long currentMillis = millis();
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

  // save water time variable
  if (!saveScheduleConfig(CONFIG_FILE_PATH, this->solenoid_pin)){
    return 0; // saving variable error
  }

  // update the last activation time of the solenoid and pump
  solenoid->lastActivation = currentMillis;
  pump->lastActivation = currentMillis;

  #ifdef DEBUG
  Serial.print("remaining time: "); Serial.println(remainingTime);
  #endif
  return remainingTime; //return active time
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads the JSON file at the specified file path and parses it to retrieve values for a number of member variables in the IrrigationController class.
// The member variables include is_set, name, timetable, watering, water_time, solenoid_pin, and pump_pin.
// Returns true if the file was read and parsed successfully, false if the file path is invalid or if there is an error reading or parsing the file.
bool IrrigationController::loadScheduleConfig(const char path[PATH_LENGTH], int pin) {
  DynamicJsonDocument jsonDoc = Helper::readConfigFile(path); // read the config file
  if (jsonDoc.isNull()) {
    return false;
  }

  // check if pin is valid
  if (pin >= max_groups) {
    return false;
  }

  // Set the values of the member variables using the values from the JSON document
  this->is_set = jsonDoc["group"][String(pin)]["is_set"].as<bool>();
  this->name = jsonDoc["group"][String(pin)]["name"].as<String>();
  this->timetable = jsonDoc["group"][String(pin)]["timetable"].as<uint32_t>();
  this->watering = jsonDoc["group"][String(pin)]["watering"].as<int16_t>();
  this->water_time = jsonDoc["group"][String(pin)]["water_time"].as<int16_t>();

  // Store pin information as its identical with the index of the solenoids
  this->solenoid_pin = jsonDoc["group"][String(pin)]["solenoid_pin"].as<int16_t>();
  this->pump_pin = jsonDoc["group"][String(pin)]["pump_pin"].as<int16_t>();

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Updates the values of a number of member variables in the IrrigationController class in the JSON file at the specified file path.
// Returns true if the file was updated successfully, false if the file path is invalid or if there is an error reading or writing the file.
bool IrrigationController::saveScheduleConfig(const char path[PATH_LENGTH], int pin) {
  DynamicJsonDocument jsonDoc =  Helper::readConfigFile(path); // read the config file
  if (jsonDoc.isNull()) {
    return false;
  }

  // Update the values in the configuration file with the values of the member variables
  jsonDoc["group"][String(pin)]["is_set"] = this->is_set;
  jsonDoc["group"][String(pin)]["name"] = this->name;
  jsonDoc["group"][String(pin)]["timetable"] = this->timetable;
  jsonDoc["group"][String(pin)]["watering"] = this->watering;
  jsonDoc["group"][String(pin)]["water_time"] = this->water_time;
  jsonDoc["group"][String(pin)]["solenoid_pin"] = this->solenoid_pin;
  jsonDoc["group"][String(pin)]["pump_pin"] = this->pump_pin;

  return Helper::writeConfigFile(jsonDoc, path); // write the updated JSON data to the config file
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Resets all member variables to their default values
void IrrigationController::reset() {
  is_set = false;
  name = "";
  timetable = 0;
  watering = 0;
  solenoid = nullptr;
  pump = nullptr;
  water_time = 0;
  solenoid_pin = 0;
  pump_pin = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
  uint8_t vent_pin = this->solenoid_pin;
  uint8_t pump_pin = this->pump_pin;
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
  Serial.println(vent_pin); Serial.print(F("time: ")); Serial.println(time_ms);
  #endif
  // perform actual function
  // byte value = (1 << vent_pin) + (1 << pump_pin);

  byte value = solenoid == nullptr ? 0 : ((1 << solenoid->pin) | (pump == nullptr ? 0 : (1 << pump->pin)));

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
  uint8_t vent_pin = this->solenoid_pin;
  uint8_t pump_pin = this->pump_pin;
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
  Serial.println(vent_pin); Serial.print(F("time: ")); Serial.println(time_ms);
  #endif
  // perform actual function
  //byte value = (1 << vent_pin) + (1 << pump_pin);

  byte value = solenoid == nullptr ? 0 : ((1 << solenoid->pin) | (pump == nullptr ? 0 : (1 << pump->pin)));

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
  // Function description: Starts the irrigation procedure after checking if the hardware is ready
  // FUNCTION PARAMETER:
  // currentHour - the active hour to check the with the timetable

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

bool IrrigationController::updateController(){
  // call this function once per hour it uses timetable to determine if it should set water_time variable with content either
  // watering_default or watering_mqtt depending on a modus variable past with the function (this can be an integer about 0-5)
  // water time variable holds information about the water cycle for the active hour if its 0 theres nothing to do
  // if its higher then it should be recognized by the readyToWater function, this value

  if (WiFi.status() == WL_CONNECTED) {
    DynamicJsonDocument newdoc = getJSONData(SERVER, SERVER_PORT, SERVER_PATH);
    if(newdoc.isNull()){
      #ifdef DEBUG
      Serial.println(F("Error reading server file"));
      #endif
      return false;
    }
    DynamicJsonDocument jsonDoc = Helper::readConfigFile(CONFIG_FILE_PATH); // read the config file
    if(jsonDoc.isNull()){
      #ifdef DEBUG
      Serial.println(F("Error reading local file"));
      #endif
      return false;
    }
    jsonDoc = newdoc;

    return Helper::writeConfigFile(jsonDoc, CONFIG_FILE_PATH); // write the updated JSON data to the config file
  }
  else{
    #ifdef DEBUG
    Serial.println(F("Error no Wifi connection established"));
    #endif
    return false;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define the implementation of the combineTimetables() static member function
long IrrigationController::combineTimetables()
{
  // Initialize the combined timetable to 0
  long combinedTimetable = 0;

  // load config
  DynamicJsonDocument doc(CONF_FILE_SIZE);
  doc = Helper::readConfigFile(CONFIG_FILE_PATH);

  // Access the "groups" object
  JsonObject groups = doc["groups"];
  doc.clear();

  for (JsonObject::iterator it = groups.begin(); it != groups.end(); ++it) {
    String key = it->key().c_str();
    // validate if key is something that could be used
    // TODO: ADD CHECK FOR MAX VPIN GROUPS!
    if (key.toInt() != 0 || key == "0") {
      JsonObject groupItem = it->value();
      long timetable = groupItem["timetable"];
      combinedTimetable |= timetable;
    }
  }
  
  return combinedTimetable;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DynamicJsonDocument IrrigationController::getJSONData(const char* server, int serverPort, const char* serverPath) {
  // Send the HTTP GET request to the Raspberry Pi server
  DynamicJsonDocument JSONdata(CONF_FILE_SIZE);
  HTTPClient http;
  http.begin(String("http://") + server + ":" + serverPort + serverPath);
  int httpCode = http.GET();

  // Check the status code
  if (httpCode == HTTP_CODE_OK) {
    // Parse the JSON data
    DeserializationError error = deserializeJson(JSONdata, http.getString());

    if (error) {
      #ifdef DEBUG
      Serial.println(F("Error parsing JSON data"));
      #endif
      return JSONdata;
    } else {
      return JSONdata;
    }
  } else {
    #ifdef DEBUG
    Serial.println(F("Error sending request to server"));
    #endif
    return JSONdata;
  }

  http.end();
  return JSONdata;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
// Getter & Setters
bool IrrigationController::getMainSwitch() const { return main_switch; }
void IrrigationController::setMainSwitch(bool value) { main_switch = value; }
bool IrrigationController::getSwitch3() const { return placeholder3; }
void IrrigationController::setSwitch3(bool value) { placeholder3 = value; }
bool IrrigationController::getDataloggingSwitch() const { return dataloging_switch; }
void IrrigationController::setDataloggingSwitch(bool value) { dataloging_switch = value; }
bool IrrigationController::getIrrigationSystemSwitch() const { return irrigation_system_switch; }
void IrrigationController::setIrrigationSystemSwitch(bool value) { irrigation_system_switch = value; }
*/