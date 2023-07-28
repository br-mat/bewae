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

#ifndef SENSOR_CONTROLLER_H
#define SENSOR_CONTROLLER_H

#include <Arduino.h>
#include <Helper.h>
#include <functional>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Setup
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////    

// Setup a oneWire instance to communicate with any OneWire devices
//OneWire WireBus(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
//DallasTemperature sensors(&WireBus);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BasicSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This is the Basic class holding sensor name, a status variable and a dummy measure function
class BasicSensor {
  public:
    // Basic constructor that takes the name of the sensor as an argument
    BasicSensor(HelperBase* helper, const String& name);

    // Basic Destructor
    virtual ~BasicSensor();

    // Pure virtual measure function that must be overridden by derived classes
    virtual float measure() = 0;

    // Publishing Data
    virtual bool pubData(InfluxDBClient* influx_client, float value);

    // setters & getters
    String getSensorName() const;
    void setSensorName(const String& name);

    bool getStatus() const;
    void setStatus(bool status);

    float getValue() const;
    void setValue(float result);

    // Helper config member
    HelperBase* helper;

  private:
    String sensorName; // Name of the sensor
    bool status;       // Status of the sensor (true = on, false = off)
    float result;      // result value
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MeasuringController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Thias class should take a custom measurement function to read a sensor
class MeasuringController : public BasicSensor {
  public:
    // Constructor that takes the name of the sensor and a std::function as arguments.
    // The measurement function is optional and defaults to nullptr if not provided.
    MeasuringController(HelperBase* helper, const String& name, std::function<float()> measurementFunction = nullptr);

    // Destructor
    ~MeasuringController();

    // Override virtual measure function from base class to return the result of calling the measurementFunction.
    float measure() override;

  private:
    std::function<float()> measurementFunction; // std::function that returns a float and takes no arguments.
    //using BasicSensor::sensorName;
    //using BasicSensor::status;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  VpinController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This class should be read analog values and give back either raw reading or relative reading within boarders
class VpinController : public BasicSensor {
  public:
    // Total Constructor
    VpinController(HelperBase* helper, const String& name, int vpin, int low_limit, int high_limit, int additive, float factor);

    // Short Constructor
    VpinController(HelperBase* helper, const String& name, int vpin);

    // JSON Obj Constructor
    // pass name (key) and config (value)
    VpinController(HelperBase* helper, const String& name, const JsonObject& sensorConfig);

    // Override virtual measure function from base class to read and return the value from the specified virtual pin.
    float measure() override; // measurement performed either default or relative (in %)

  private:
    int virtualPin; // Virtual pin number associated with this controller.
    int low_limit; // Lower limit value for measurement
    int high_limit; // Upper limit value for measurement
    int additive; // additive factor to measurement
    float factor; // Factor to multiply measurement values by

    //using BasicSensor::sensorName; // previously protected:
    //using BasicSensor::status;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  PinController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// PinController class that inherits from BasicSensor
class PinController : public BasicSensor {
  public:
      // Total Constructor
    PinController(HelperBase* helper, const String& name, int pin, int low_limit, int high_limit, int additive, float factor);

    // Short Constructor
    PinController(HelperBase* helper, const String& name, int pin);

    // JSON Obj Constructor
    // pass name (key) and config (value)
    PinController(HelperBase* helper, const String& name, const JsonObject& sensorConfig);
    
    // Override the measure function to read from an Arduino pin
    float measure() override;

  private:
    int pin;
    int low_limit; // Lower limit value for measurement
    int high_limit; // Upper limit value for measurement
    int additive; // additive factor to measurement
    float factor; // Factor to multiply measurement values by
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  SoilTempSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PinController class that inherits from BasicSensor
class SoilTempSensor : public BasicSensor {
  public:
      // Total Constructor
    SoilTempSensor(HelperBase* helper, const String& name, int addr, int additive, float factor);

    // Short Constructor
    SoilTempSensor(HelperBase* helper, const String& name, int addr);

    // JSON Obj Constructor
    // pass name (key) and config (value)
    SoilTempSensor(HelperBase* helper, const String& name, const JsonObject& sensorConfig);
    
    // Override the measure function to read from an Arduino pin
    float measure() override;

  private:
    OneWire oneWire;             // OneWire instance
    DallasTemperature sensors;   // DallasTemperature instance
    int addr;
    int additive; // additive factor to measurement
    float factor; // Factor to multiply measurement values by
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  SoilTempSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This class is used to gahter and publish data from an bme280 using adafruit lib
class bmeSensor : public BasicSensor {
  public:
      // Total Constructor
    bmeSensor(HelperBase* helper, const String& name, int addr, int mode);

    // JSON Obj Constructor
    // pass name (key) and config (value)
    bmeSensor(HelperBase* helper, const String& name, const JsonObject& sensorConfig);
    
    // Override the measure function
    float measure() override;

  private:
    Adafruit_BME280 bme_; // BME280 sensor instance
    int addr; // I2C address of the BME280 sensor
    int mode;
};
#endif
