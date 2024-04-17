////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file contains the declaration for the IrrigationController class, which is responsible for managing and
// controlling an irrigation system. It includes functions for creating and configuring irrigation controllers,
// updating the controller based on current conditions, and determining when it is time to water.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef IRRIGATION_CONTROLLER_H
#define IRRIGATION_CONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <config.h>
#include <connection.h>
#include <Helper.h>

#include <vector>

#ifndef PATH_LENGTH
#define PATH_LENGTH 25
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HARDWARE STRUCTS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define a class to store the information for a solenoid and pumps
class LoadDriverPin {
private:
  int vpin;
  long lastActivation;

public:
  // Constructor
  LoadDriverPin(int pin);

  // Getter for pin
  int getPin() const;

  // Getter for lastActivation
  long getLastActivation() const;

  // Setter for lastActivation
  void setLastActivation();
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS IrrigationController
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Initialize empty class for basic functions
// for irrigation fill instance with loadScheduleConfig()

class IrrigationController {
  private:
    // PRIVATE VARIABLES
    // New
    std::vector<unsigned int> driver_pins; // vector of driver pins
    
    const char* key;
    bool is_set; //activate deactivate group, value will get saved to config
    char name[MAX_GROUP_LENGTH]; //[pn] name of the group, value will get saved to config
    unsigned long timetable; //[pt] timetable special formated, value will get saved to config
    int water_time; //[wt] defualt value of watering amount set for group, value will get saved to config
    int plant_size; //[pls]
    int pot_size; //[pts]
    byte lastDay; //[pu][0] runntimevariable
    byte lastHour; //[pu][0] runntimevariable
    int watering; //[dty] runntimevariable

    // Helper config member
    //HelperBase* helper;

    // PRIVATE METHODS:
    DynamicJsonDocument getJSONData(const char* server, int serverPort, const char* serverPath); // OUTDATED
    // Member function to activate watering using PWM
    void activatePWM(int time_s);
    // Member function to activate watering
    void activate(int time_s);

  public:
    // DEFAULT Constructor seting an empty class, populate with loadScheduleConfig for specific irrigation functionality
    IrrigationController();

    // DEFAULT Destructor free up dynamic allocated memory
    ~IrrigationController();

    // PUBLIC METHODS:

    // Member function loads schedule config from file
    // used to fill empty class
    bool loadScheduleConfig(const JsonPair& groupPair);

    // Verify Configfile
    bool verifyScheduleConfig(const JsonPair& groupPair);

    // Member function saves state to config file
    bool saveScheduleConfig(const char path[PATH_LENGTH], const char name[MAX_GROUP_LENGTH]);

    // Member function checks if a group is ready:
    // returns watering time in (seconds) as int when ready, -1 when waiting, and 0 (finished, not configured, error)
    int readyToWater();

    // Public function: Handling watering process calling related functionality
    // call this function every now and then to keep track of time variables
    int watering_task_handler();

    // Resets all member variables to their default values
    void reset();

    // Static member function to combine the timetables of an array of IrrigationController objects using a loop
    static long combineTimetables();
    // function to init running file & update
    void loadDuty(const char* objkey);
    bool saveDuty(const char* objkey);
};

#endif