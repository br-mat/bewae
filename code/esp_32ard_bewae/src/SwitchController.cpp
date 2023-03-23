#include "SwitchController.h"

// Constructor
SwitchController::SwitchController() {
  JsonObject switches = Helper::getJsonObjects("switches", CONFIG_FILE_PATH);
  if (!switches.isNull()) {
    if (switches.containsKey("main_switch")) {
      main_switch = switches["main_switch"];
    } else {
      main_switch = false;
    }
    if (switches.containsKey("dataloging_switch")) {
      dataloging_switch = switches["dataloging_switch"];
    } else {
      dataloging_switch = false;
    }
    if (switches.containsKey("irrigation_system_switch")) {
      irrigation_system_switch = switches["irrigation_system_switch"];
    } else {
      irrigation_system_switch = false;
    }
    if (switches.containsKey("placeholder3")) {
      placeholder3 = switches["placeholder3"];
    } else {
      placeholder3 = false;
    }
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
