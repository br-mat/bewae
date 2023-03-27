#include "SwitchController.h"

// Constructor
SwitchController::SwitchController() {
  JsonObject switches = Helper::getJsonObjects("switches", SENSOR_FILE_PATH);
  int reading_errors = 0;
  if (!switches.isNull()) {
    if (switches.containsKey("main_switch")) {
      main_switch = switches["main_switch"];
    } else {
      main_switch = false; // set default in case of false reading
      reading_errors++;
    }
    if (switches.containsKey("dataloging_switch")) {
      dataloging_switch = switches["dataloging_switch"];
    } else {
      dataloging_switch = false;
      reading_errors++;
    }
    if (switches.containsKey("irrigation_system_switch")) {
      irrigation_system_switch = switches["irrigation_system_switch"];
    } else {
      irrigation_system_switch = false;
      reading_errors++;
    }
    if (switches.containsKey("placeholder3")) {
      placeholder3 = switches["placeholder3"];
    } else {
      placeholder3 = false;
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

bool SwitchController::saveSwitches() {

  // Load the config file
  DynamicJsonDocument jsonDoc = Helper::readConfigFile(CONFIG_FILE_PATH);
  if (jsonDoc.isNull()) {
    return false;
  }
  
  // Update the switch values in the config file
  JsonObject switches = jsonDoc["switches"].as<JsonObject>();
  switches["main_switch"] = this->main_switch;
  switches["dataloging_switch"] = this->dataloging_switch;
  switches["irrigation_system_switch"] = this->irrigation_system_switch;
  switches["placeholder3"] = this->placeholder3;
  
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

DynamicJsonDocument SwitchController::getJSONData(const char* server, int serverPort, const char* serverPath) {
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

bool SwitchController::updateSwitches() {
  // SwitchController update
  SwitchController status_switches;

  DynamicJsonDocument jsonDoc = SwitchController::getJSONData(SERVER, SERVER_PORT, SERVER_PATH);

  // check if file could be found
  if (jsonDoc.isNull()) {
    return false;
  }

  // Update the switch values in the config file
  JsonObject switches = jsonDoc["switches"].as<JsonObject>();
  this->main_switch = switches["main_switch"];
  this->dataloging_switch = switches["dataloging_switch"];
  this->irrigation_system_switch = switches["irrigation_system_switch"];
  this->placeholder3 = switches["placeholder3"];

  return SwitchController::saveSwitches();
}

// Getters
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
