#ifndef IRRIGATIONCONTROLLER_H
#define IRRIGATIONCONTROLLER_H


#include <Arduino.h>
#include <ArduinoJson.h>
#include <Helper.h>
#include <config.h>


// Define a setupfunction for Hardware
void IrrigationHardware();

// Define a struct to store the information for a solenoid
// this should be constructed as an array the index of the array should be assigned
// to the pin lastActivation should be millis() this will prevent watering on reset in the programm
struct Solenoid {
  int pin;
  long lastActivation;

  // Member function to initialize the solenoid
  void setup(int pin);
};

// Define a struct to store the information for a pump
struct Pump {
  int pin;
  long lastActivation;

  // Member function to initialize the pump
  void setup(int pin);
};

class IrrigationController {
  private:
    bool is_set; //activate deactivate group, value will get saved to config
    Solenoid* solenoid; //attatched solenoid
    Pump* pump; //attatched pump

  public:
    IrrigationController(
                          const char path[],
                          Solenoid* solenoid, //attatched solenoid
                          Pump* pump //attatched pump
                          );
    String name; //name of the group, value will get saved to config
    unsigned long timetable; //timetable special formated, value will get saved to config
    int watering_default; //defualt value of watering amount set for group (should not be changed), value will get saved to config
    int watering_mqtt; //base value of watering time sent from RaspPi, value will get saved to config
    int water_time; //holds value of how long it should water, value will get saved to config
    int solenoid_pin; //storing registered struct solenoid pin, value will get saved to config
    int pump_pin; //storing registered struct pump pin, value will get saved to config

    //METHODS:

    // Member function will create a new member and take all attributes manually
    void createNewController(
                          bool is_set, //activate deactivate group
                          String name, //name of the group
                          double timetable,
                          int watering_default, //defualt value of watering amount set for group (should not be changed)
                          Solenoid* solenoid, //attatched solenoid
                          Pump* pump, //attatched pump
                          int watering_mqtt=0, //base value of watering time sent from RaspPi
                          int water_time=0 //holds value of how long it should water
                          );
    // Member function loads config from file
    bool loadFromConfig(const char path[], int pin);
    // Member function
    bool saveToConfig(const char path[], int pin);
    // Member function
    void updateController();
    // Member function
    int readyToWater(int currentHour);
};

#endif