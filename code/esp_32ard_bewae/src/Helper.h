////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This file contains a collection of helper functions for the irrigation system. It includes functions for
// generating timestamps, controlling solenoids and pumps, reading and writing to config files, and interacting
// with various hardware components.
//
// Dependencies:
// - Arduino.h
// - driver/adc.h
// - config.h
// - ArduinoJson.h
// - SPIFFS.h
// - connection.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __Helper_H__
#define __Helper_H__

#ifndef DEBUG
#define DEBUG
#endif

#ifndef PATH_LENGTH
#define PATH_LENGTH 25
#endif

#include <WiFi.h>
#include <InfluxDbClient.h>
#include <Arduino.h>
#include "driver/adc.h"
#include <config.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <connection.h>

class HelperBase{
public:
    String timestamp();
    void copy(int* src, int* dst, int len); // copy string from dest to src
    byte dec_bcd(byte val); // Convert normal decimal numbers to binary coded decimal 
    byte bcd_dec(byte val); // Convert binary coded decimal to normal decimal numbers
    void set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
    void read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year);
    bool disableWiFi(); // disables Wifi
    bool connectWifi(); // enables Wifi and connect
    void setModemSleep(); // disable wifi and reduce clock speed
    void wakeModemSleep(); // enable wifi and set clock to normal speed
    void disableBluetooth();
    bool find_element(int *array, int item);
    // Loads the config file and sets the values of the member variables
    DynamicJsonDocument readConfigFile(const char path[PATH_LENGTH]);
    // Saves the values of the member variables to the config file
    bool writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]);
    bool pubInfluxData(InfluxDBClient* influx_client, String sensor_name, String field_name, float value); // send data to influxdb, return true when everything is ok
    void blinkOnBoard(String howLong, int times); // blink onboard led to signal something
    JsonObject getJsonObjects(const char* key, const char* filepath); // returns requestet JSON object (key) of specified file
    DynamicJsonDocument getJSONData(const char* server, int serverPort, const char* serverPath); 
    bool updateConfig(const char* path); // check for config file updates from raspberrypi

    // virtual functions
    virtual void shiftvalue8b(uint8_t val, bool invert = false); // set shift register to value (8 bit)
    virtual void shiftvalue(uint32_t val, uint8_t numBits, bool invert = false); // set shift register with more bits (32 bit)
    virtual void system_sleep(); // prepare low power mode (currently without light or deepsleep; TODO!)
    virtual void controll_mux(uint8_t channel, String mode, int *val);
    virtual void enablePeripherals(); // enable 3v3 and others to other devices
    virtual void disablePeripherals(); // disable 3v3 and others to other devices
    virtual void enableSensor(); // enable 3v3 and others to other devices
    virtual void disableSensor(); // disable 3v3 and others to other devices
};

// Hardware configuration 1 (Board 1) main
class Helper_config1_main : public HelperBase{
  // Default
};

// Hardware configuration 1 (Board 1) alternative
class Helper_config1_Board1v3838 : public HelperBase{
public:
    void shiftvalue8b(uint8_t val, bool invert = false) override; // set shift register to value (8 bit)
    void shiftvalue(uint32_t val, uint8_t numBits, bool invert = false) override; // set shift register with more bits (32 bit)
    void system_sleep() override; // prepare low power mode (currently without light or deepsleep; TODO!)
    void controll_mux(uint8_t channel, String mode, int *val) override;
    void enablePeripherals() override; // enable 3v3 and others to other devices
    void disablePeripherals() override; // disable 3v3 and others to other devices
    void enableSensor() override; // enable 3v3 and others to other devices
    void disableSensor() override; // disable 3v3 and others to other devices
    void setPinModes(); // set pinmode of all enum elements to OUT/INPUT

private:
    enum Pins {
        PWM = 32,
        SW_SENS = 4,
        SW_SENS2 = 25,
        SW_3_3V = 23,
        EN_MUX_1 = 5,
        S3_MUX_1 = 16,
        S2_MUX_1 = 17,
        S1_MUX_1 = 18,
        S0_MUX_1 = 19,
        //SIG_MUX_1 = 39, // input pins
        ST_CP_SHFT = 26,
        DATA_SHFT = 33,
        SH_CP_SHFT = 27
    };
    // iteratble configuratoin pins
    const uint8_t output_pins[12] = {PWM, SW_SENS, SW_SENS2, SW_3_3V, EN_MUX_1, S3_MUX_1, S2_MUX_1, S1_MUX_1, S0_MUX_1, ST_CP_SHFT, DATA_SHFT, SH_CP_SHFT};
    // configured input pins
    const uint8_t input_pins[1] = {39};
};

// Hardware configuration 2 (Board 5)
class Helper_config1_Board5v5 : public HelperBase{
public:
    void shiftvalue8b(uint8_t val, bool invert = false) override; // set shift register to value (8 bit)
    void shiftvalue(uint32_t val, uint8_t numBits, bool invert = false) override; // set shift register with more bits (32 bit)
    void system_sleep() override; // prepare low power mode (currently without light or deepsleep; TODO!)
    //void controll_mux(uint8_t channel, String mode, int *val) override; // keep empty
    void enablePeripherals() override; // enable 3v3 and others to other devices
    void disablePeripherals() override; // disable 3v3 and others to other devices
    void enableSensor() override; // enable 3v3 and others to other devices
    void disableSensor() override; // disable 3v3 and others to other devices
    void setPinModes();

private:
    enum Pins {
        PWM = 32,
        SW_SENS = 4,
        SW_SENS2 = 25,
        SW_3_3V = 23,
        //SIG_MUX_1 = 39, // input pins
        ST_CP_SHFT = 17,
        DATA_SHFT = 13,
        SH_CP_SHFT = 16
    };
    // iteratble configuratoin pins
    const uint8_t output_pins[7] = {PWM, SW_SENS, SW_SENS2, SW_3_3V, ST_CP_SHFT, DATA_SHFT, SH_CP_SHFT};
    // configured input pins
    const uint8_t input_pins[7] = {26, 27, 39, 18, 15, 34, 36};
};

// allowing to use HelperClass without having to create an instance of the class
extern Helper_config1_Board5v5 HWHelper;

#endif