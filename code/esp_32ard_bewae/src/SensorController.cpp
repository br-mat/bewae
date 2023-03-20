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

// Constructor with virtual pin number as argument
VpinController::VpinController(const String& name, int vpin) : BasicSensor(name), virtualPin(vpin) {}

// Override virtual measure function from base class
float VpinController::measure() {
  int result = 0;
  Helper::controll_mux(this->virtualPin, sig_mux_1, en_mux_1, "read", &result); // use virtualPin on MUX and read its value
  return (float)result;
}