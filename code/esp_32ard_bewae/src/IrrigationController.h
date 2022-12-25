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
#include <config.h>


// Define a setupfunction for Hardware
//void IrrigationHardware();

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

class IrrigationController {
  private:
    Solenoid* solenoid; //attatched solenoid
    Pump* pump; //attatched pump

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
                          const char path[20],
                          Solenoid* solenoid, //attatched solenoid
                          Pump* pump //attatched pump
                          );

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
    // Member function loads config from file
    bool loadFromConfig(const char path[20], int pin);
    // Member function saves config to file
    bool saveToConfig(const char path[20], int pin);
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
};

#endif