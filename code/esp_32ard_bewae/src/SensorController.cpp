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
BasicSensor::BasicSensor(HelperBase* helper, Adafruit_BME280 bmeClass, DallasTemperature DallasTemp) : helper(helper), bmeModule(bmeClass), ds18b20Module(DallasTemp){
}

// Basic Destructor
BasicSensor::~BasicSensor() {}

bool BasicSensor::pubData(InfluxDBClient* influx_client, float value){
// TODO
    return helper->pubInfluxData(influx_client, this->sensorName, INFLUXDB_FIELD, value);
}

// handlers
float BasicSensor::analoghandler(uint8_t hardwarePin){
    helper->disableWiFi(); // make sure to be able to use adc2 pins
    delay(3);

    float resultm = helper->readAnalogRoutine(hardwarePin); // Read the value from the specified pin

Serial.print("raw: "); Serial.println(resultm);

    return resultm;
}

float BasicSensor::analogVhandler(uint8_t virtualPin){
    helper->disableWiFi(); // make sure to be able to use adc2 pins
    delay(3);

    // measurement performed either default or relative (in %)
    int resultm = 0;
    helper->controll_mux(virtualPin, "read", &resultm); // use virtualPin on MUX and read its value

Serial.print("raw: "); Serial.println(resultm);

    // return result (float)
    return resultm;
}

float BasicSensor::onewirehandler(){
    this->ds18b20Module.begin();
    delayMicroseconds(10); // give sensor time to start up

    // Request temperature
    this->ds18b20Module.requestTemperatures();
    delay(1);

    // Read temperature from DS18B20 sensor
    float temperatureC = 0;
    temperatureC = this->ds18b20Module.getTempCByIndex(0);

    // Return the measured and processed temperature value
    return temperatureC;
}

float BasicSensor::bmetemphandler(){
    if (!this->bmeModule.begin(this->bmeaddr)) {
        Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
        return 0;
    }
    float val = 0;
    val = this->bmeModule.readTemperature();
    return val;
}

float BasicSensor::bmehumhandler(){
    if (!this->bmeModule.begin(this->bmeaddr)) {
        Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
        return 0;
    }
    float val = 0;
    val = this->bmeModule.readHumidity();
    return val;
}

float BasicSensor::bmepresshandler(){
    if (!this->bmeModule.begin(this->bmeaddr)) {
        Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
        return 0;
    }
    float val = 0;
    val = this->bmeModule.readPressure();
    return val;
}

float BasicSensor::measure(HelperBase* helper, const String& name, const JsonObject& sensorConfig) {
    int additive_;
    float factor_;
    int high_limit_;
    int low_limit_;
    int virtualPin_;
    int hardwarePin_;

    float measurmentraw;
    float measurmentPub;

    // validate JSON: Check for the presence of the additive and factor properties
    if (sensorConfig.containsKey("add")) {
        additive_ = sensorConfig["add"].as<int>();
    } else {
        additive_ = 0.0;
    }
    if (sensorConfig.containsKey("fac")) {
        factor_ = sensorConfig["fac"].as<float>();
    } else {
        factor_ = 1.0;
    }
    if (sensorConfig.containsKey("hlim")) {
        high_limit_ = sensorConfig["hlim"].as<int>();
    } else {
        high_limit_ = 0;
    }
    if (sensorConfig.containsKey("llim")) {
        low_limit_ = sensorConfig["llim"].as<int>();
    } else {
        low_limit_ = 0;
    }
    if (sensorConfig.containsKey("vpin")) {
        virtualPin_ = sensorConfig["vpin"].as<uint16_t>();
    } else {
        virtualPin_ = 0;
    }
    if (sensorConfig.containsKey("pin")) {
        hardwarePin_ = sensorConfig["pin"].as<uint16_t>();
    } else {
        hardwarePin_ = 0;
    }
    if (sensorConfig.containsKey("mode")) {
        String mode = sensorConfig["mode"];
        if (mode == "analog") {
            measurmentraw = analoghandler(hardwarePin_);
        } else if (mode == "vanalog") {
            measurmentraw = analogVhandler(virtualPin_);
        } else if (mode == "bmetemp") {
            measurmentraw = bmetemphandler();
        } else if (mode == "bmehum") {
            measurmentraw = bmehumhandler();
        } else if (mode == "bmepress") {
            measurmentraw = bmepresshandler();
        } else if (mode == "soiltemp") {
            measurmentraw = onewirehandler();
        } else {
        // code for unknown mode
            #ifdef DEBUG
            Serial.println(F("Warning: Unknown measuring mode!"));
            return -273;
            #endif
        }
    }

    // check if rel measurement is needed
    if ((high_limit_ != 0) && (low_limit_ != 0)) {
        float temp = (float)measurmentraw;
        float result_;
        #ifdef DEBUG
        Serial.print(F("raw read:")); Serial.println(result);
        Serial.print(F("raw read int:")); Serial.println((int)temp);
        #endif
        temp = measurmentraw + 0.5; // cast remporary float to int and round
        result_ = constrain(temp, low_limit_, high_limit_); //x within borders else x = border value; (example 1221 wet; 3176 dry [in mV])
                                                        //avoid using other functions inside the brackets of constrain
        measurmentPub = map(result_, low_limit_, high_limit_, 1000, 0) / 10;
        return measurmentPub;
    }
    Serial.print("raw: "); Serial.println(measurmentraw);

    measurmentPub = factor_ * measurmentraw + additive_;
Serial.print("float: "); Serial.println(measurmentPub);
Serial.print("factor: "); Serial.println(factor_);
    // return result
    return measurmentPub;
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