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
// The goal is to store all the virtual pins we want to measure in a configuration file.
// Then use the helper function to retrieve all the keys from the virtual pin group
// and initialize the corresponding sensor classes when we want to take measurements.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SensorController.h

#ifndef SENSOR_CONTROLLER_H
#define SENSOR_CONTROLLER_H

#include <Arduino.h>
#include <Helper.h>
#include <functional>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BasicSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class BasicSensor {
public:
    // Basic constructor that takes the name of the sensor as an argument
    BasicSensor(const String& name);

    // Basic Destructor
    virtual ~BasicSensor();

    // Pure virtual measure function that must be overridden by derived classes
    virtual float measure() = 0;

protected:
    String sensorName; // Name of the sensor
    bool status;       // Status of the sensor (true = on, false = off)
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MeasuringController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MeasuringController : public BasicSensor {
public:
    // Constructor that takes the name of the sensor and a std::function as arguments.
    // The measurement function is optional and defaults to nullptr if not provided.
    MeasuringController(const String& name, std::function<float()> measurementFunction = nullptr);

    // Destructor
    ~MeasuringController();

    // Override virtual measure function from base class to return the result of calling the measurementFunction.
    float measure() override;

private:
    std::function<float()> measurementFunction; // std::function that returns a float and takes no arguments.
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  VpinController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VpinController : public BasicSensor {
public:
  // Constructor that takes the name of the sensor and a virtual pin number as arguments.
  VpinController(const String& name, int vpin);

  // Override virtual measure function from base class to read and return the value from the specified virtual pin.
  float measure() override;

private:
  int virtualPin; // Virtual pin number associated with this controller.
};

#endif
