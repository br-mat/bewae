#ifndef SWITCHCONTROLLER_H
#define SWITCHCONTROLLER_H

#include <ArduinoJson.h>
#include "Helper.h"

class SwitchController {
  private:
    // SWITCH VARIABLES:
    bool main_switch; // main switch
    bool dataloging_switch; // dataloging switch
    bool irrigation_system_switch; // irrigation system switch
    bool placeholder3; // switch

  public:
    // Constructor
    SwitchController();

    // SAVE SWITCH SETTINGS TO CONFIG FILE:
    bool saveSwitches();

    // Getters
    bool getMainSwitch();
    bool getDatalogingSwitch();
    bool getIrrigationSystemSwitch();
    bool getPlaceholder3();

    // Setters
    void setMainSwitch(bool value);
    void setDatalogingSwitch(bool value);
    void setIrrigationSystemSwitch(bool value);
    void setPlaceholder3(bool value);
};

#endif