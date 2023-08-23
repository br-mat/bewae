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
    BasicSensor(HelperBase* helper, Adafruit_BME280* bmeClass, DallasTemperature DallasTemp);

    // Basic Destructor
    virtual ~BasicSensor();

    // Publishing Data
    virtual bool pubData(InfluxDBClient* influx_client, float value);

    // handlers
    float analoghandler(uint8_t hardwarePin);
    float analogVhandler(uint8_t virtualPin);
    float onewirehandler();
    float bmetemphandler();
    float bmehumhandler();
    float bmepresshandler();

    // measure function 
    float measure(HelperBase* helper, const String& name, const JsonObject& sensorConfig);

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
