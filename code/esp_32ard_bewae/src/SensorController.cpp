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

#include "SensorController.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BasicSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Basic constructor
BasicSensor::BasicSensor(HelperBase* helper, const String& name) : helper(helper), sensorName(name), status(true) {}

// Basic Destructor
BasicSensor::~BasicSensor() {}

bool BasicSensor::pubData(InfluxDBClient* influx_client, float value){
// TODO
    return helper->pubInfluxData(influx_client, this->sensorName, INFLUXDB_FIELD, value);
}

// getters & setters
String BasicSensor::getSensorName() const {
    return sensorName;
}

void BasicSensor::setSensorName(const String& name) {
    sensorName = name;
}

bool BasicSensor::getStatus() const {
    return status;
}

void BasicSensor::setStatus(bool status) {
    this->status = status;
}

float BasicSensor::getValue() const {
    return status;
}

void BasicSensor::setValue(float result) {
    this->result = result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  MeasuringController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Constructor with default measurement function nullptr
MeasuringController::MeasuringController(HelperBase* helper, const String& name, std::function<float()> measurementFunction)
: BasicSensor(helper, name), measurementFunction(measurementFunction) {}

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
VpinController::VpinController(HelperBase* helper, const String& name, int vpin, int low_limit, int high_limit, int additive, float factor)
    : BasicSensor(helper, name), virtualPin(vpin), low_limit(low_limit), high_limit(high_limit), additive(additive), factor(factor) {}

// Short Constructor
VpinController::VpinController(HelperBase* helper, const String& name, int vpin) : VpinController(helper, name, vpin, 0, 0, 0, 1.0) {}

// JSON Obj Constructor
VpinController::VpinController(HelperBase* helper, const String& name, const JsonObject& sensorConfig) : BasicSensor(helper, name) { 
    // validate JSON: Check for the presence of the additive and factor properties
    if (sensorConfig.containsKey("add")) {
        additive = sensorConfig["add"].as<int>();
    } else {
        additive = 0.0;
    }
    if (sensorConfig.containsKey("fac")) {
        factor = sensorConfig["fac"].as<float>();
    } else {
        factor = 1.0;
    }
    if (sensorConfig.containsKey("hlim")) {
        high_limit = sensorConfig["hlim"];
    } else {
        high_limit = 0;
    }
    if (sensorConfig.containsKey("llim")) {
        low_limit = sensorConfig["llim"];
    } else {
        low_limit = 0;
    }
    if (sensorConfig.containsKey("vpin")) {
        virtualPin = sensorConfig["vpin"].as<uint16_t>();
    } else {
        virtualPin = 0;
    }
}

// Override virtual measure function from base class
float VpinController::measure() {
    // measurement performed either default or relative (in %)
    int result = 0;
    helper->controll_mux(this->virtualPin, "read", &result); // use virtualPin on MUX and read its value

    // check if rel measurement is needed
    if ((high_limit != 0) && (low_limit != 0)) {
        float temp = (float)result * measurement_LSB5V * 1000;
        #ifdef DEBUG
        Serial.print(F("raw read:")); Serial.println(result);
        Serial.print(F("raw read int:")); Serial.println((int)temp);
        #endif
        result = temp + 0.5; // cast remporary float to int and round
        result = constrain(result, this->low_limit, this->high_limit); //x within borders else x = border value; (example 1221 wet; 3176 dry [in mV])
                                                        //avoid using other functions inside the brackets of constrain
        result = map(result, this->low_limit, this->high_limit, 1000, 0) / 10;
    }

    // return result
    return this->factor * (float)result + this->additive;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  PinController
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Total Constructor
PinController::PinController(HelperBase* helper, const String& name, int pin, int low_limit, int high_limit, int additive, float factor)
    : BasicSensor(helper, name), pin(pin), low_limit(low_limit), high_limit(high_limit), additive(additive), factor(factor) {}

// Short Constructor
PinController::PinController(HelperBase* helper, const String& name, int pin) : PinController(helper, name, pin, 0, 0, 0, 1.0) {}

// JSON Obj Constructor
PinController::PinController(HelperBase* helper, const String& name, const JsonObject& sensorConfig) : BasicSensor(helper, name) { 
    // validate JSON: Check for the presence of the additive and factor properties
    if (sensorConfig.containsKey("add")) {
        additive = sensorConfig["add"].as<int>();
    } else {
        additive = 0.0;
    }
    if (sensorConfig.containsKey("fac")) {
        factor = sensorConfig["fac"].as<float>();
    } else {
        factor = 1.0;
    }
    if (sensorConfig.containsKey("hlim")) {
        high_limit = sensorConfig["hlim"];
    } else {
        high_limit = 0;
    }
    if (sensorConfig.containsKey("llim")) {
        low_limit = sensorConfig["llim"];
    } else {
        low_limit = 0;
    }
    if (sensorConfig.containsKey("pin")) {
        pin = sensorConfig["pin"].as<uint16_t>();
    } else {
        pin = 0.0;
    }
}

// Override the measure function to read from an Arduino pin
float PinController::measure() {
    int num = 15;
    int result = 0;
    // throw away
    for(int j = 0;j<5;j++){
        result = analogRead(pin); // Read the value from the specified pin
        delayMicroseconds(100);
    }
    // take mean
    result = 0;
    float mean = 0;
    for(int j = 0;j<num;j++){
        mean += analogRead(pin); // Read the value from the specified pin
        delayMicroseconds(100);
    }
    result = (mean/num)+0.5;
    return this->factor * result + this->additive;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  SoilTempSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Total Constructor
SoilTempSensor::SoilTempSensor(HelperBase* helper, const String& name, int addr, int additive, float factor)
    : BasicSensor(helper, name), addr(addr), additive(additive), factor(factor), sensors(&oneWire) {}

// Short Constructor
SoilTempSensor::SoilTempSensor(HelperBase* helper, const String& name, int addr) : SoilTempSensor(helper, name, addr, 0, 1.0) {}

// JSON Obj Constructor
SoilTempSensor::SoilTempSensor(HelperBase* helper, const String& name, const JsonObject& sensorConfig) : BasicSensor(helper, name) { 
    // validate JSON: Check for the presence of the additive and factor properties
    if (sensorConfig.containsKey("add")) {
        additive = sensorConfig["add"].as<int>();
    } else {
        additive = 0.0;
    }
    if (sensorConfig.containsKey("fac")) {
        factor = sensorConfig["fac"].as<float>();
    } else {
        factor = 1.0;
    }
    if (sensorConfig.containsKey("pin")) {
        addr = sensorConfig["pin"].as<uint16_t>();
    } else {
        addr = 0.0;
    }

    oneWire = OneWire((uint8_t)addr);
    sensors = DallasTemperature(&oneWire);
}

// Override the measure function to read from an Arduino pin
float SoilTempSensor::measure() {
    sensors.begin();  // Initialize the DS18B20 sensor
    delay(100);

    // Request temperature
    this->sensors.requestTemperatures();

    // Read temperature from DS18B20 sensor
    float temperatureC = this->sensors.getTempCByIndex(0);

    // Apply additive and factor
    temperatureC = temperatureC + additive;
    temperatureC = temperatureC * factor;

    // Return the measured and processed temperature value
    return temperatureC;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  bmeSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Total Constructor
bmeSensor::bmeSensor(HelperBase* helper, const String& name, int addr, int mode)
    : BasicSensor(helper, name), addr(addr) {}

// JSON Obj Constructor
bmeSensor::bmeSensor(HelperBase* helper, const String& name, const JsonObject& sensorConfig) : BasicSensor(helper, name) { 
    // validate JSON: Check for the presence of the additive and factor properties
    if (sensorConfig.containsKey("addr")) {
        addr = sensorConfig["addr"].as<int>();
    } else {
        addr = BME280_I2C_ADDRESS; // use default if not specified
    }
    // mode: 0 = temp, 1 = humidity, 2 = pressure
    if (sensorConfig.containsKey("mode")) {
        mode = sensorConfig["mode"].as<int>();
    } else {
        mode = 0; // use default if not specified
    }
}

// Override the measure function to read from an Arduino pin
float bmeSensor::measure() {
    // Initialize the BME280 sensor
    if (!bme_.begin(this->addr)) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
        return 0;
    }

    float result;
    switch (this->mode)
    {
    case 0:
        result = bme_.readTemperature();
        break;
    case 1:
        result = bme_.readHumidity();
        break;
    case 2:
        result = bme_.readPressure();
        break;
    default:
        break;
    }

    return result;
}

// TODO:
// - implement bme280
// - add unified publishing method to base class, use default on most of them and on bme custom one
// - build test enironment for only this class + a config file
// - add seperate config file
// - remove dht

// TODO: implement new json constructor to all subclasses
//       implement new classes for normal pin not vpin etc.// TODO: implement new json constructor to all subclasses
//       implement new classes for normal pin not vpin etc.

// Hae
// - ds12b2
// - analog moist 5v