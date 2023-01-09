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
// - config.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef IRRIGATIONCONTROLLER_H
#define IRRIGATIONCONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <config.h>

#ifndef PATH_LENGTH
#define PATH_LENGTH 25
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// HARDWARE STRUCTS
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Define a struct to store the information for a solenoid
// this should be constructed as an array the index of the array should be assigned
// to the pin lastActivation should be millis() this will prevent watering on reset in the programm
// Define a struct to store the information for a solenoid
struct Solenoid {
  int pin;
  long lastActivation;
  static int activeInstances;

  // Default constructor
  Solenoid();

  // Constructor that takes a pin number as input
  Solenoid(int pin);

  // Destructor
  ~Solenoid();

  // Member function to initialize the solenoid
  void setup(int pin);
};

// Define a struct to store the information for a pump
// It's basically the same
struct Pump {
  int pin;
  long lastActivation;

  // Member function to initialize the pump
  void setup(int pin);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS IrrigationController
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class IrrigationController {
  private:
    Solenoid* solenoid; //attatched solenoid
    Pump* pump; //attatched pump

    // SWITCH VARIABLES:
    bool main_switch; // main switch
    bool dataloging_switch; // dataloging switch
    bool irrigation_system_switch; // irrigation system switch
    bool placeholder3; // switch

    // METHODS:
    // Member function to activate watering using PWM
    void activatePWM(int time_s);
    // Member function to activate watering
    void activate(int time_s);

  public:
    // Default constructor
    IrrigationController() : solenoid(nullptr), pump(nullptr) {}

    // Constructor that takes a file path, a solenoid object, and a pump object as input
    IrrigationController(
                          const char path[PATH_LENGTH],
                          Solenoid* solenoid, //attatched solenoid
                          Pump* pump //attatched pump
                          );

    // TODO: SHIFT this variables into private as soon as testing is done
    bool is_set; //activate deactivate group, value will get saved to config
    String name; //name of the group, value will get saved to config
    unsigned long timetable; //timetable special formated, value will get saved to config
    int watering; //defualt value of watering amount set for group, value will get saved to config
    int water_time; //holds value of how long it should water, value will get saved to config
    int solenoid_pin; //storing registered struct solenoid pin, value will get saved to config
    int pump_pin; //storing registered struct pump pin, value will get saved to config

    //METHODS:
    // Member function will create a new member and take all attributes manually
    void createNewController(
                          bool is_set, //activate deactivate group
                          String name, //name of the group
                          unsigned long timetable,
                          int watering, //defualt value of watering amount set for group (should not be changed)
                          Solenoid* solenoid, //attatched solenoid
                          Pump* pump, //attatched pump
                          int water_time=0 //holds value of how long it should water
                          );
    // Member function loads schedule config from file
    bool loadScheduleConfig(const char path[PATH_LENGTH], int pin);
    // Member function saves schedule config to file
    bool saveScheduleConfig(const char path[PATH_LENGTH], int pin);
    // Member function checks if a group is ready,
    // returns part of the watering time the group is allowed to be active
    int readyToWater(int currentHour);
    // Method to start watering process
    void waterOn(int hour);
    // Resets all member variables to their default values
    void reset();
    // Member function currently unused probably use it later to update static variables storing state of switches?
    void updateController();
    // Define a static member function to combine the timetables of an array of IrrigationController objects using a loop
    static long combineTimetables(IrrigationController* controllers, size_t size);

    // Loads the config file and sets the values of the member variables
    DynamicJsonDocument readConfigFile(const char path[PATH_LENGTH]);
    // Saves the values of the member variables to the config file
    bool writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]);
    // sends GET request to RaspberryPi and store response in json doc
    bool getJSONData(DynamicJsonDocument& doc, const char* server, int serverPort, const char* serverPath);

    // Getter & Setters
    bool getMainSwitch() const;
    void setMainSwitch(bool value);
    bool getSwitch3() const;
    void setSwitch3(bool value);
    bool getDataloggingSwitch() const;
    void setDataloggingSwitch(bool value);
    bool getIrrigationSystemSwitch() const;
    void setIrrigationSystemSwitch(bool value);
};

#endif