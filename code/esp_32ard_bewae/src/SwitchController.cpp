////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file contains implementation of status switches.
// Adding functionality to turn on/off parts of the system.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SwitchController.h"

// Constructor
SwitchController::SwitchController(HelperBase* helper) : helper(helper) {
  /*
  String sensorPathString = String(HTTPGET_PREFIX) + String(SENS_CONFIG_PATH);
  const char* sensorPath = sensorPathString.c_str();

  DynamicJsonDocument jsonDoc = helper->readConfigFile(sensorPath);
  int reading_errors = 0;
  if (!jsonDoc.isNull()) {
    if (jsonDoc.containsKey("dn")) {
      name = jsonDoc["dn"].as<String>();
    }
    else{
      #ifdef DEBUG
      Serial.println("Warning: could not find Device name!");
      #endif
      name = String("");
    }
    main_switch = jsonDoc["main"] | false; // main - main system ON/OFF
    placeholder3 = jsonDoc["dmmy"] | false; // placeholder - placeholder ON/OFF
    irrigation_system_switch = jsonDoc["irig"] | false; // irig - irrigation system ON/OFF
    dataloging_switch = jsonDoc["mssr"] | false; // mssr - measurement ON/OFF
    
    // Check for reading errors
    if (jsonDoc["dn"].isNull()) reading_errors++;
    if (jsonDoc["main"].isNull()) reading_errors++;
    if (jsonDoc["dmmy"].isNull()) reading_errors++;
    if (jsonDoc["irig"].isNull()) reading_errors++;
    if (jsonDoc["mssr"].isNull()) reading_errors++;

    if (reading_errors > 0) {
      #ifdef DEBUG
      Serial.print(F("WARNING: Switching Class config file could not be read correctly! Some states are set to 'false', count: "));
      Serial.println(reading_errors);
      #endif
    }
  } else {
    #ifdef DEBUG
    Serial.println(F("WARNING: Switching Class could not be initialized! All States are set to 'false'."));
    #endif
  }*/
  updateSwitches(); // set class variables
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool SwitchController::saveSwitches() { // save class variables
  String sensorPathString = String(DEVICE_CONFIG_PATH) + String(JSON_SUFFIX);
  const char* sensorPath = sensorPathString.c_str();

  // Load the config file
  DynamicJsonDocument jsonDoc = helper->readConfigFile(sensorPath);
  if (jsonDoc.isNull()) {
    return false;
  }
  
  // Update the switch values in the config file
  jsonDoc["dn"]= this->name;
  jsonDoc["main"] = this->main_switch;
  jsonDoc["mssr"] = this->dataloging_switch;
  jsonDoc["irig"] = this->irrigation_system_switch;
  jsonDoc["dmmy"] = this->placeholder3;
  
  // Save the updated config file
  bool success = helper->writeConfigFile(jsonDoc, sensorPath);
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
  /* // OLD CODE (BACKUP)
  String sensorPathString = String(HTTPGET_PREFIX) + String(SENS_CONFIG_PATH);
  const char* sensorPath = sensorPathString.c_str();
  
  // Update config variables from local file
  DynamicJsonDocument jsonDoc = helper->readConfigFile(sensorPath);

  // Check json document
  if (jsonDoc.isNull()) {
    return false;
  }

  // Updating class variables
  if (jsonDoc.containsKey("dn")) {
    this->name = jsonDoc["dn"].as<String>();
  }
  else{
    #ifdef DEBUG
    Serial.println("Warning: could not find Device name!");
    #endif
    this->name = String("");
  }
  this->main_switch = jsonDoc["main"] | false;
  this->dataloging_switch = jsonDoc["mssr"] | false;
  this->irrigation_system_switch = jsonDoc["irig"] | false;
  this->placeholder3 = jsonDoc["dmmy"] | false;

  return true;*/
  String sensorPathString = String(DEVICE_CONFIG_PATH) + String(JSON_SUFFIX);
  const char* sensorPath = sensorPathString.c_str();

  DynamicJsonDocument jsonDoc = helper->readConfigFile(sensorPath);
  int reading_errors = 0;
  if (!jsonDoc.isNull()) {
    if (jsonDoc.containsKey("dn")) {
      name = jsonDoc["dn"].as<String>();
    }
    else{
      #ifdef DEBUG
      Serial.println("Warning: could not find Device name!");
      #endif
      name = String("");
    }

    main_switch = jsonDoc["main"].as<bool>() | false; // main - main system ON/OFF
    placeholder3 = jsonDoc["dmmy"].as<bool>() | false; // placeholder - placeholder ON/OFF
    irrigation_system_switch = jsonDoc["irig"].as<bool>() | false; // irig - irrigation system ON/OFF
    dataloging_switch = jsonDoc["mssr"].as<bool>() | false; // mssr - measurement ON/OFF
    
    // Check for reading errors
    if (jsonDoc["dn"].isNull()) reading_errors++;
    if (jsonDoc["main"].isNull()) reading_errors++;
    if (jsonDoc["dmmy"].isNull()) reading_errors++;
    if (jsonDoc["irig"].isNull()) reading_errors++;
    if (jsonDoc["mssr"].isNull()) reading_errors++;

    if (reading_errors > 0) {
      #ifdef DEBUG
      Serial.print(F("WARNING: Switching Class config file could not be read correctly! Some set to 'false', count: "));
      Serial.println(reading_errors);
      Serial.print("Path: "); Serial.println(sensorPath);
      #endif
      return false;
    }
    return true; // ALL Good
  }
  else {
    #ifdef DEBUG
    Serial.println(F("WARNING: Switching Class could not be initialized! All States are set to 'false'."));
    Serial.print(F("Tried to read: ")); Serial.println(sensorPath);
    #endif
    return false;
  }
  #ifdef DEBUG
  Serial.println(F("Critical Error: Switching Class!"));
  #endif
  return false; // default false return, should never be reached anyway!
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Getters
String SwitchController::getName() {
  return name;
}

bool SwitchController::getMainSwitch() {
  return main_switch;
}

bool SwitchController::getDatalogingSwitch() {
  return dataloging_switch;
}

bool SwitchController::getIrrigationSystemSwitch() {
  return irrigation_system_switch;
}

bool SwitchController::getPlaceholder3() {
  return placeholder3;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Setters
void SwitchController::setMainSwitch(bool value) {
  main_switch = value;
}

void SwitchController::setDatalogingSwitch(bool value) {
  dataloging_switch = value;
}

void SwitchController::setIrrigationSystemSwitch(bool value) {
  irrigation_system_switch = value;
}

void SwitchController::setPlaceholder3(bool value) {
  placeholder3 = value;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////