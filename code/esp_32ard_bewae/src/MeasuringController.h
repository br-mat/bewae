////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file contains a collection of helper functions for the measuring system. It includes functions to
// generating measurments of all sensors, interacting
// with various hardware components, and reading config files.
//
// Dependencies:
// - Arduino.h
// - Wire.h
// - SPI.h
// - SD.h
// - Adafruit_Sensor.h
// - Adafruit_BME280.h
// - WiFi.h
// - Helper.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef MEASURINGCONTROLLER_H
#define MEASURINGCONTROLLER_H

#include <Arduino.h>

class MeasuringController {
public:
    // Constructor with default measurement function nullptr
    MeasuringController(const String& name, int vpin, float (*measurementFunction)() = nullptr) : sensorName(name), virtualPin(vpin), isSet(true), measurementFunction(measurementFunction){}

    // Destructor
    ~MeasuringController();

    // Public member functions
    float measure();

private:
    String sensorName;
    int virtualPin;
    bool isSet;
    float (*measurementFunction)();
};

#endif

