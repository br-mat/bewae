////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// see gitHub for author info
//
// This file contains a collection of closely related classes managing the measuring system. It includes functions to
// generating measurments of all sensors, interacting
// with various hardware components, and reading config files.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SensorController.h"
#include "LogFire.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BasicSensor
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Basic constructor
BasicSensor::BasicSensor(HelperBase* helper, Adafruit_BME280* bmeClass, DallasTemperature DallasTemp) : helper(helper), bmeModule(bmeClass), ds18b20Module(DallasTemp){
}

// Basic Destructor
BasicSensor::~BasicSensor() {}

// function to publish a data point
bool BasicSensor::pubData(InfluxDBClient* influx_client, String name, String field, float value){
    return HWHelper.pubInfluxData(influx_client, name, field, value);
}

// funciton to publish a whole data vector
bool BasicSensor::pubVector(InfluxDBClient* influx_client, const std::vector<SensorData>& sensors) {
    // Publish data from the internal vector
    bool success = false;
    for (const SensorData& sensor : sensors) {
        success |= HWHelper.pubInfluxData(influx_client, sensor.name, sensor.field, sensor.data);
    }
    return success;
}


// handlers
float BasicSensor::analoghandler(uint8_t hardwarePin){
    helper->disableWiFi(); // make sure to be able to use adc2 pins
    helper->enablePeripherals();
    helper->enableSensor();
    delay(3);
    // Read the value from the specified pin
    float resultm = helper->readAnalogRoutine(hardwarePin);

    return resultm;
}

float BasicSensor::analogVhandler(uint8_t virtualPin){
    // measurement performed either default or relative (in %); in case of problems increase delay of controll_mux
    int resultm = 0;
    helper->controll_mux(virtualPin, "read", &resultm); // use virtualPin on MUX and read its value

    // return result (float)
    return resultm;
}

float BasicSensor::onewirehandler(){
    HWHelper.enablePeripherals();
    this->ds18b20Module.begin();
    delayMicroseconds(200); // give sensor time to start up

    // Request temperature
    this->ds18b20Module.requestTemperatures();
    delay(2);

    // Read temperature from DS18B20 sensor
    float temperatureC = 0;
    temperatureC = this->ds18b20Module.getTempCByIndex(0);

    // Return the measured and processed temperature value
    return temperatureC;
}

float BasicSensor::bmetemphandler(){
    HWHelper.enablePeripherals();
    HWHelper.enableSensor();
    delay(1); // give sensor time to settle
    if (!bmeModule->begin(BME280_I2C_ADDRESS)) {
        LogFire.log("sensor: BME280 not found (temp), check wiring", 2);
        return 0;
    }
    float val = 0;
    val = bmeModule->readTemperature();
    return val;
}

float BasicSensor::bmehumhandler(){
    HWHelper.enablePeripherals();
    HWHelper.enableSensor();
    delay(1); // give sensor time to settle
    if (!bmeModule->begin(BME280_I2C_ADDRESS)) {
        LogFire.log("sensor: BME280 not found (hum), check wiring", 2);
        return 0;
    }
    float val = 0;
    val = bmeModule->readHumidity();
    return val;
}

float BasicSensor::bmepresshandler(){
    HWHelper.enablePeripherals();
    HWHelper.enableSensor();
    delay(1); // give sensor time to settle
    if (!bmeModule->begin(BME280_I2C_ADDRESS)) {
        LogFire.log("sensor: BME280 not found (press), check wiring", 2);
        return 0;
    }
    float val = 0;
    val = bmeModule->readPressure();
    return val;
}

float BasicSensor::measuref(HelperBase* helper, const JsonObject& sensorConfig) {
    int additive_ = 0;
    float factor_ = 1.0;
    int high_limit_;
    int low_limit_;
    int virtualPin_;
    int hardwarePin_;

    float measurmentraw;
    float measurmentPub;

    // validate JSON: Check for the presence of the additive and factor properties
    /*
    if (sensorConfig.containsKey("add")) {
        additive_ = sensorConfig["add"].as<int>();
    } else {
        additive_ = 0.0;
    }
    if (sensorConfig.containsKey("fac")) {
        factor_ = sensorConfig["fac"].as<float>();
    } else {
        factor_ = 1.0;
    }*/
    if (sensorConfig.containsKey("hl")) {
        high_limit_ = sensorConfig["hl"].as<int>();
    } else {
        LogFire.log("measuref: 'hl' missing in sensor config", 2);
        return 0;
    }
    if (sensorConfig.containsKey("ll")) {
        low_limit_ = sensorConfig["ll"].as<int>();
    } else {
        LogFire.log("measuref: 'll' missing in sensor config", 2);
        return 0;
    }
    if (sensorConfig.containsKey("sp")) {
        virtualPin_ = sensorConfig["sp"].as<uint16_t>();
        hardwarePin_ = sensorConfig["sp"].as<uint16_t>();
    } else {
        LogFire.log("measuref: 'sp' missing in sensor config", 2);
        return 0;
    }
    if (sensorConfig.containsKey("sm")) {
        String mode = sensorConfig["sm"];
        if (mode == String("analog")) {
            if (!HWHelper.checkAnalogPin(hardwarePin_)) {
                LogFire.log("measuref: invalid analog pin " + String(hardwarePin_), 2);
                return 0;
            }
            measurmentraw = analoghandler(hardwarePin_);
        } else if (mode == String("vanalog")) {
            measurmentraw = analogVhandler(virtualPin_);
        } else if (mode == String("bmetemp")) {
            measurmentraw = bmetemphandler();
        } else if (mode == String("bmehum")) {
            measurmentraw = bmehumhandler();
        } else if (mode == String("bmepress")) {
            measurmentraw = bmepresshandler();
        } else if (mode == String("soiltemp")) {
            measurmentraw = onewirehandler();
        } else {
            LogFire.log("measuref: unknown mode '" + mode + "'", 2);
            return 0;
        }
    }
    // check if rel measurement is needed
    if ((high_limit_ != 0) && (low_limit_ != 0)) {
        float temp = (float)measurmentraw;
        float result_;
        temp = measurmentraw + 0.5;
        result_ = constrain(temp, low_limit_, high_limit_);
        measurmentPub = map(result_, low_limit_, high_limit_, 1000, 0) / 10;
        LogFire.log("sensor \"" + sensorConfig["sn"].as<String>() + "\": raw=" + String(measurmentraw) + " -> " + String(measurmentPub) + "%", 0);
        return measurmentPub;
    }
    measurmentPub = factor_ * measurmentraw + additive_;
    LogFire.log("sensor \"" + sensorConfig["sn"].as<String>() + "\": raw=" + String(measurmentraw) + " -> " + String(measurmentPub), 0);

    // set class variable
    result = measurmentPub;

    // return result
    return measurmentPub;
}

SensorData BasicSensor::measurePoint(HelperBase* helper, const String& id, const JsonObject& sensorConfig){
    String name_;
    String field_;

    // check passed name and field
    if (sensorConfig.containsKey("sn")) {
        name_ = sensorConfig["sn"].as<String>();
    }
    else{
        LogFire.log("measurePoint: 'sn' missing in sensor config id=" + id, 2);
    }
    if (sensorConfig.containsKey("sf")) {
        field_ = sensorConfig["sf"].as<String>();
    }
    else{
        field_ = INFLUXDB_FIELD;
        LogFire.log("measurePoint: 'sf' missing in sensor config id=" + id + ", defaulting to " + String(INFLUXDB_FIELD), 1);
    }

    // check if sensor is enabled (ss flag)
    if (sensorConfig.containsKey("ss") && sensorConfig["ss"].as<int>() != 1) {
        LogFire.log("sensor \"" + id + "\": disabled (ss!=1), skip", 0);
        SensorData dataPoint;
        dataPoint.name = name_;
        dataPoint.field = field_;
        dataPoint.data = 0;
        return dataPoint;
    }

    // create and fill dataPoint
    SensorData dataPoint;
    dataPoint.data = measuref(helper, sensorConfig);
    dataPoint.name = name_;
    dataPoint.field = field_;
    return dataPoint;
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
    return result;
}

void BasicSensor::setValue(float result) {
    this->result = result;
}