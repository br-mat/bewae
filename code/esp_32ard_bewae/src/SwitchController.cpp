#include "SwitchController.h"

// Constructor
SwitchController::SwitchController() {
  JsonObject switches = Helper::getJsonObjects("switch", CONFIG_FILE_PATH);
  int reading_errors = 0;
  if (!switches.isNull()) {
    if (switches.containsKey("main")) {
      main_switch = switches["main"]; // main - main system ON/OFF
    } else {
      main_switch = false; // set default in case of false reading
      reading_errors++;
    }
    if (switches.containsKey("dmmy")) {
      placeholder3 = switches["dmmy"]; // placeholder - placeholder ON/OFF
    } else {
      placeholder3 = false;
      reading_errors++;
    }
    if (switches.containsKey("irig")) {
      irrigation_system_switch = switches["irig"]; // irig - irrigation_system ON/OFF
    } else {
      irrigation_system_switch = false;
      reading_errors++;
    }
    if (switches.containsKey("mssr")) {
      dataloging_switch = switches["mssr"]; // mssr - measurement ON/OFF
    } else {
      dataloging_switch = false;
      reading_errors++;
    }
    if (reading_errors > 0) // if some errors occure output a warning to serial for debuging
    {
    #ifdef DEBUG
    Serial.print(F("WARNING: Switching Class config file could not be read correctly! Some states are set to 'false', count: "));
    Serial.println(reading_errors);
    #endif
    }
    
  }
  else{
    #ifdef DEBUG
    Serial.println(F("WARNING: Switching Class could not be initialized! All States are set to 'false'."));
    #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SwitchController::saveSwitches() {

  // Load the config file
  DynamicJsonDocument jsonDoc = Helper::readConfigFile(CONFIG_FILE_PATH);
  if (jsonDoc.isNull()) {
    return false;
  }
  
  // Update the switch values in the config file
  JsonObject switches = jsonDoc["switch"].as<JsonObject>();
  switches["main"] = this->main_switch;
  switches["mssr"] = this->dataloging_switch;
  switches["irig"] = this->irrigation_system_switch;
  switches["dmmy"] = this->placeholder3;
  
  // Save the updated config file
  bool success = Helper::writeConfigFile(jsonDoc, CONFIG_FILE_PATH);
  if (!success) {
    #ifdef DEBUG
    Serial.println("Error: could not save switches to config file.");
    #endif
    return false;
  }

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SwitchController::updateSwitches(){
  // Update config variables from local file
  // get newest locally stored config
  JsonObject switches = Helper::getJsonObjects("switch", CONFIG_FILE_PATH);

  // check json object
  if (switches.size() == 0) {
    return false;
  }

  // updating class variables
  this->main_switch = switches["main"];
  this->dataloging_switch = switches["irig"];
  this->irrigation_system_switch = switches["mssr"];
  this->placeholder3 = switches["dmmy"];

  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Getters
bool SwitchController::getMainSwitch() {
  return main_switch;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SwitchController::getDatalogingSwitch() {
  return dataloging_switch;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SwitchController::getIrrigationSystemSwitch() {
  return irrigation_system_switch;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SwitchController::getPlaceholder3() {
  return placeholder3;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Setters
void SwitchController::setMainSwitch(bool value) {
  main_switch = value;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwitchController::setDatalogingSwitch(bool value) {
  dataloging_switch = value;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwitchController::setIrrigationSystemSwitch(bool value) {
  irrigation_system_switch = value;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SwitchController::setPlaceholder3(bool value) {
  placeholder3 = value;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////