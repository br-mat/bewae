////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file contains a collection of closely related classes managing the measuring system. It includes functions to
// generating measurments of all sensors, interacting
// with various hardware components, and reading config files.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SensorController.h"

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
    //helper->disableWiFi(); // make sure to be able to use adc2 pins
    //helper->enablePeripherals();
    //helper->enableSensor();
    //delay(10);

    // measurement performed either default or relative (in %)
    int resultm = 0;
    helper->controll_mux(virtualPin, "read", &resultm); // use virtualPin on MUX and read its value

    // return result (float)
    return resultm;
}

float BasicSensor::onewirehandler(){
    HWHelper.enablePeripherals();
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
    HWHelper.enablePeripherals();
    if (!bmeModule->begin(BME280_I2C_ADDRESS)) {
        #ifdef DEBUG
        Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
        #endif
        return 0;
    }
    float val = 0;
    val = bmeModule->readTemperature();
    return val;
}

float BasicSensor::bmehumhandler(){
    HWHelper.enablePeripherals();
    if (!bmeModule->begin(BME280_I2C_ADDRESS)) {
        #ifdef DEBUG
        Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
        #endif
        return 0;
    }
    float val = 0;
    val = bmeModule->readHumidity();
    return val;
}

float BasicSensor::bmepresshandler(){
    HWHelper.enablePeripherals();
    if (!bmeModule->begin(BME280_I2C_ADDRESS)) {
        #ifdef DEBUG
        Serial.println(F("Warning: Could not find a valid BME280 sensor, check wiring!"));
        #endif
        return 0;
    }
    float val = 0;
    val = bmeModule->readPressure();
    return val;
}

float BasicSensor::measuref(HelperBase* helper, const JsonObject& sensorConfig) {
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
            HWHelper.checkAnalogPin(hardwarePin_);
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
        temp = measurmentraw + 0.5; // cast remporary float to int and round
        result_ = constrain(temp, low_limit_, high_limit_); //x within borders else x = border value; (example 1221 wet; 3176 dry [in mV])
                                                        //avoid using other functions inside the brackets of constrain
        measurmentPub = map(result_, low_limit_, high_limit_, 1000, 0) / 10;
        #ifdef DEBUG
        Serial.print(F("Reading Sensor (relative): ")); Serial.println(sensorConfig["name"].as<String>());
        Serial.print(F("raw: ")); Serial.println(measurmentraw);
        Serial.print(F("return: ")); Serial.println(measurmentPub);
        #endif
        return measurmentPub;
    }
    measurmentPub = factor_ * measurmentraw + additive_;
    #ifdef DEBUG
    Serial.print(F("Reading Sensor: ")); Serial.println(sensorConfig["name"].as<String>());
    Serial.print("raw: "); Serial.println(measurmentraw);
    Serial.print("return: "); Serial.println(measurmentPub);
    #endif

    // set class variable
    result = measurmentPub;

    // return result
    return measurmentPub;
}

SensorData BasicSensor::measurePoint(HelperBase* helper, const String& id, const JsonObject& sensorConfig){
    String name_;
    String field_;

    // check passed name and field
    if (sensorConfig.containsKey("name")) {
        name_ = sensorConfig["name"].as<String>();
    }
    else{
        #ifdef DEBUG
        Serial.print(F("Warning: No name found in Sensor Configuration, Id:"));
        Serial.println(id);
        #endif
    }
    if (sensorConfig.containsKey("field")) {
        field_ = sensorConfig["field"].as<String>();
    }
    else{
        field_ = INFLUXDB_FIELD;
        #ifdef DEBUG
        Serial.print(F("Warning: Field not found! set to default, Id:"));
        Serial.println(id);
        #endif
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