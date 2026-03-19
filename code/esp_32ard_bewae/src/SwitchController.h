////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// see gitHub for author info
//
// This file contains definition of switches class.
// Adding functionality to turn on/off parts of the system.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef SWITCHCONTROLLER_H
#define SWITCHCONTROLLER_H

#include <ArduinoJson.h>
#include <Helper.h>

class SwitchController {
  private:
    String name; // device name

    // SWITCH VARIABLES:
    bool main_switch; // main switch
    bool datalogging_switch; // dataloging switch
    bool irrigation_system_switch; // irrigation system switch
    bool placeholder3; // switch

    HelperBase* helper;

  public:
    // Constructor
    SwitchController(HelperBase* helper);

    // SAVE SWITCH SETTINGS TO CONFIG FILE:
    bool saveSwitches();
    // Member function to update related data
    bool updateSwitches();

    // Getters
    String getName();
    bool getMainSwitch();
    bool getDataloggingSwitch();
    bool getIrrigationSystemSwitch();
    bool getPlaceholder3();

    // Setters
    void setMainSwitch(bool value);
    void setDataloggingSwitch(bool value);
    void setIrrigationSystemSwitch(bool value);
    void setPlaceholder3(bool value);
};


#endif