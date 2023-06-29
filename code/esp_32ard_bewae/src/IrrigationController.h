////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This file contains the declaration for the IrrigationController class, which is responsible for managing and
// controlling an irrigation system. It includes functions for creating and configuring irrigation controllers,
// updating the controller based on current conditions, and determining when it is time to water.
//
// Dependencies:
// - Arduino.h
// - ArduinoJson.h
// - HTTPClient.h
// - SPIFFS.h
// - config.h
// - connection.h
// - Helper.h
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

class IrrigationController {
  private:
    // PRIVATE VARIABLES
    // New
    std::vector<int> driver_pins; // vector of driver pins

    bool is_set; //activate deactivate group, value will get saved to config
    char name[MAX_GROUP_LENGTH]; //name of the group, value will get saved to config
    unsigned long timetable; //timetable special formated, value will get saved to config
    int watering; //defualt value of watering amount set for group, value will get saved to config
    int water_time; //holds value of how long it should water, value will get saved to config

    // PRIVATE METHODS:
    // Loads the config file and sets the values of the member variables
    //DynamicJsonDocument readConfigFile(const char path[PATH_LENGTH]);
    // Saves the values of the member variables to the config file
    //bool writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]);
    // sends GET request to RaspberryPi and store response in json doc
    DynamicJsonDocument getJSONData(const char* server, int serverPort, const char* serverPath); // OUTDATED
    // Member function to activate watering using PWM
    void activatePWM(int time_s);
    // Member function to activate watering
    void activate(int time_s);

  public:
    // NEW Constructor:
    IrrigationController(
                          const char path[PATH_LENGTH],
                          const char keyName[MAX_GROUP_LENGTH]
    );

    // DEFAULT Constructor seting an empty class
    IrrigationController();

    // DEFAULT Destructor free up dynamic allocated memory
    ~IrrigationController();

    // PUBLIC METHODS:
    // Member function loads schedule config from file
    bool loadScheduleConfig(const JsonPair& groupPair);
    // Member function saves schedule config to file
    bool saveScheduleConfig(const char path[PATH_LENGTH], const char name[MAX_GROUP_LENGTH]);
    // Member function checks if a group is ready,
    // returns part of the watering time the group is allowed to be active
    int readyToWater(int currentHour);
    // Method to start watering process
    int waterOn(int hour);
    // Resets all member variables to their default values
    void reset();
    // Member function to update the data
    //bool updateController();
    // Define a static member function to combine the timetables of an array of IrrigationController objects using a loop
    static long combineTimetables();
};

#endif