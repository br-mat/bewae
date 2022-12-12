#include <Arduino.h>
#include <IrrigationController.h>

IrrigationController::IrrigationController(
                        bool is_set, //activate deactivate group
                        String name, //name of the group
                        double timetable,
                        int watering_default, //defualt value of watering amount set for group (should not be changed)
                        Solenoid* solenoid, //attatched solenoid
                        Pump* pump, //attatched pump
                        int watering_mqtt, //base value of watering time sent from RaspPi
                        int waterTime //holds value of how long it should water
                        ){
  this->is_set = is_set;
  this->name = name;
  this->timetable = timetable;
  this->watering_default = watering_default;
  this->solenoid = solenoid;
  this->pump = pump;
  this->watering_mqtt = watering_mqtt;
  this->waterTime = waterTime;
  }

  int IrrigationController::readyToWater(int currentHour) {
  //INPUT: currentHour is the int digits of the full hour
  //Function:
  //It checks if the irrigation system is ready to water.
  //If the system is ready,
  //it calculates the remaining watering time and updates the waterTime variable and returns the active time.
  //It returns 0 if the system is not ready to water.

  //to prevent unexpected behaviour its important to pass only valid hours

  if(!is_set){ //return 0 if the group is not set
    return 0;
  }

    // check if the corresponding bit is set in the timetable value
  if ((timetable & (1 << currentHour)) == 0) {
    return 0; // not ready to water
  }

  // get the current time in milliseconds
  unsigned long currentMillis = millis();

  // check if the solenoid has been active for too long
  if (currentMillis - solenoid->lastActivation < max_active_time_sec) {
    return 0; // not ready to water
  }

  // check if the pump has been active for too long
  if (currentMillis - pump->lastActivation < pump_cooldown_sec) {
    return 0; // not ready to water
  }

  // check if there is enough water time left
  if (waterTime <= 0) {
    return 0; // not ready to water
  }

// both the solenoid and pump are ready, so calculate the remaining watering time
int remainingTime = min((unsigned long)waterTime, (max_active_time_sec - (currentMillis - solenoid->lastActivation)));
this->waterTime -= remainingTime; // update the water time

// update the last activation time of the solenoid
solenoid->lastActivation = currentMillis;

return remainingTime; //return active time

}


void IrrigationController::loadFromConfig(){
  //TODO: implement function to load config file
};

void IrrigationController::saveToConfig(){
  //TODO: implement function to save config file
};

void IrrigationController::updateController(){
  //TODO: implement function to update controller status data regarding watering system
}