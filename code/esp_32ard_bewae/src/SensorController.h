////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file contains a collection of closely related classes managing the measuring system. It includes functions to
// generating measurments of all sensors, interacting
// with various hardware components, and reading config files.
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
// Data structure
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// basic data structure with name and value
struct SensorData {
    String name;
    String field;
    float data;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BasicSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This is the Basic class holding sensor name, a status variable and a dummy measure function
class BasicSensor {
  public:
    // Basic constructor that takes the name of the sensor as an argument
    BasicSensor(HelperBase* helper, Adafruit_BME280* bmeClass, DallasTemperature DallasTemp);

    // Basic Destructor
    virtual ~BasicSensor();

    // Publishing Data

    // function to publish a data point
    bool pubData(InfluxDBClient* influx_client, float value);
    // funciton to publish a whole data vector
    bool pubVector(InfluxDBClient* influx_client, const std::vector<SensorData>& sensors);

    // handlers
    float analoghandler(uint8_t hardwarePin);
    float analogVhandler(uint8_t virtualPin);
    float onewirehandler();
    float bmetemphandler();
    float bmehumhandler();
    float bmepresshandler();

    // measure function 
    float measuref(HelperBase* helper, const JsonObject& sensorConfig);
    SensorData measurePoint(HelperBase* helper, const String& id, const JsonObject& sensorConfig);

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
    uint8_t bmeaddr;
    uint8_t ds18b20pin;

    Adafruit_BME280* bmeModule; // BME280 sensor instance
    DallasTemperature ds18b20Module;
};
#endif