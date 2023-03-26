////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file contains a collection of closely related classes managing the measuring system. It includes functions to
// generating measurments of all sensors, interacting
// with various hardware components, and reading config files.
//
// Dependencies:
// - Arduino.h
// - Wire.h
// - ArduinoJson.h
// - HTTPClient.h
// - SPIFFS.h
// - config.h
// - connection.h
// - Helper.h
// - WiFi.h
// - Adafruit_Sensor.h
// - Adafruit_BME280.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: implement SPIFFS and http GET to recieve config file!

// SensorController.cpp

#include "SensorController.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BasicSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Basic constructor
BasicSensor::BasicSensor(const String& name) : sensorName(name), status(true) {}

// Basic Destructor
BasicSensor::~BasicSensor() {}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MeasuringController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor with default measurement function nullptr
MeasuringController::MeasuringController(const String& name, std::function<float()> measurementFunction)
: BasicSensor(name), measurementFunction(measurementFunction) {}

// Destructor
MeasuringController::~MeasuringController() {}

// Override virtual measure function from base class
float MeasuringController::measure() {
  if (measurementFunction) {
      return measurementFunction(); // execute measurement function
  } else {
      return 0.0f; // return 0 if no function passed
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  VpinController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Total Constructor
VpinController::VpinController(const String& name, int vpin, int low_limit, int high_limit, float* factor)
    : BasicSensor(name), virtualPin(vpin), low_limit(low_limit), high_limit(high_limit), factor(factor) {}

// Short Constructor
VpinController::VpinController(const String& name, int vpin) : VpinController(name, vpin, low_lim, high_lim, nullptr) {}

// Override virtual measure function from base class
float VpinController::measureRaw() {
  int result = 0;
  Helper::controll_mux(this->virtualPin, sig_mux_1, en_mux_1, "read", &result); // use virtualPin on MUX and read its value

  // return result
  if (factor != nullptr) {
    return result * (*factor);
  }
  return (float)result;
}

// returns relative measurement in %
float VpinController::measure() {
  int value = 0;
  Helper::controll_mux(this->virtualPin, sig_mux_1, en_mux_1, "read", &value); // use virtualPin on MUX and read its value
  float temp = (float)value * measurement_LSB5V * 1000;
  #ifdef DEBUG
  Serial.print(F("raw read:")); Serial.println(value);
  Serial.print(F("raw read int:")); Serial.println((int)temp);
  #endif
  value = temp;
  value = constrain(value, this->low_limit, this->high_limit); //x within borders else x = border value; (example 1221 wet; 3176 dry [in mV])
                                                //avoid using other functions inside the brackets of constrain
  value = map(value, this->low_limit, this->high_limit, 1000, 0) / 10;
  
  // return result
  if (factor != nullptr) {
    return value * (*factor);
  }
  
  return value;
}