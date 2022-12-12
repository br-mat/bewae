#ifndef IRRIGATIONCONTROLLER_H
#define IRRIGATIONCONTROLLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <config.h>

// Define a struct to store the information for a solenoid
struct Solenoid {
  int pin;
  long lastActivation;
};

// Define a struct to store the information for a pump
struct Pump {
  int pin;
  long lastActivation;
};

class IrrigationController {
  private:
    bool is_set; //activate deactivate group

  public:
    IrrigationController(
                          bool is_set, //activate deactivate group
                          String name, //name of the group
                          double timetable,
                          int watering_default, //defualt value of watering amount set for group (should not be changed)
                          Solenoid* solenoid, //attatched solenoid
                          Pump* pump, //attatched pump
                          int watering_mqtt=0, //base value of watering time sent from RaspPi
                          int waterTime=0 //holds value of how long it should water
                          );
    String name; //name of the group
    unsigned long timetable;
    int watering_default; //defualt value of watering amount set for group (should not be changed)
    Solenoid* solenoid; //attatched solenoid
    Pump* pump; //attatched pump
    int watering_mqtt; //base value of watering time sent from RaspPi
    int waterTime; //holds value of how long it should water

    //METHODS:
    void loadFromConfig();
    void saveToConfig();
    void updateController();
    int readyToWater(int currentHour);
};

#endif