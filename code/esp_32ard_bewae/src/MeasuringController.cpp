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

// MeasuringController.cpp
#include "MeasuringController.h"

// Constructor implementation
MeasuringController::MeasuringController(const String& name, int vpin,float (*measurementFunction)() = nullptr) : sensorName(name), virtualPin(vpin), isSet(true), measurementFunction(measurementFunction) {
}

// Destructor implementation
MeasuringController::~MeasuringController() {
}

// Public member function implementation
float MeasuringController::measure() {
    float measurement = measurementFunction();
    return measurement;
}