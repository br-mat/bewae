////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// The Helper Classes should form representation of the Hardware.
// This file contains a collection of helper functions for the irrigation system, enerating timestamps, 
// controlling solenoids and pumps, reading and writing to config files and 
// interacting with various hardware components.
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
#include <Crypto.h>

class HelperBase{
public:
    // return timestamp as string
    String timestamp();
    // copy string from dest to src
    void copy(int* src, int* dst, int len);
    // Convert normal decimal numbers to binary coded decimal 
    byte dec_bcd(byte val);
    // Convert binary coded decimal to normal decimal numbers 
    byte bcd_dec(byte val);
    // set Time on RTC module
    void set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
    // read Time procedure default (old)
    void read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year);
    // read Time procedure testing (new)
    bool readTime(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year);
    // Analog measurment routine returning read value as int
    int readAnalogRoutine(uint8_t gpiopin);
    // disables Wifi module saving power
    bool disableWiFi();
    // enables Wifi & connect to configured Wifi
    bool connectWifi();
    // disable wifi and reduce clock speed
    void setModemSleep();
    // enable wifi and set clock to normal speed
    void wakeModemSleep();
    // disabling Bluetooth module (not used currently)
    void disableBluetooth();
    // funciton searches an array for provided element returns true on hit
    bool find_element(int *array, int item);
    // Loads the config file and sets the values of the member variables
    DynamicJsonDocument readConfigFile(const char path[PATH_LENGTH]);
    // Saves the values of the member variables to the config file
    bool writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]);
    // send data to influxdb, return true when everything is ok
    bool pubInfluxData(InfluxDBClient* influx_client, String sensor_name, String field_name, float value);
    // blink onboard led for visual signals
    void blinkOnBoard(String howLong, int times);
    // returns requestet JSON object (key) of specified file
    JsonObject getJsonObjects(const char* key, const char* filepath);
    // HTTP GET request to the a configured server retrieving JSON config data
    DynamicJsonDocument getJSONConfig(const char* server, int serverPort, const char* serverPath);
    // OLD HTTP GET
    DynamicJsonDocument getJSONData(const char* server, int serverPort, const char* serverPath);
    // This function calculates the SHA-256 hash of the input content and returns the hash as a hexadecimal string.
    String sha256(String content);
    // This function verifies the integrity of received JSON data by comparing its checksum with a calculated checksum.
    bool verifyChecksum(DynamicJsonDocument& JSONdata);
    // calculate hash of a json doc without optional contained hash value ("checksum")
    String calculateJSONHash(DynamicJsonDocument& JSONdata);
    // routine to check and update config file (from raspberrypi server)
    bool updateConfig(const char* path);

    // virtual functions

    // set shift register to value (8 bit)
    virtual void shiftvalue8b(uint8_t val, bool invert = false);
    // set shift register with more bits (32 bit)
    virtual void shiftvalue(uint32_t val, uint8_t numBits, bool invert = false);
    // prepare low power mode (currently without light or deepsleep)
    virtual void system_sleep();
    // dummy function
    virtual void controll_mux(uint8_t channel, String mode, int *val);
    // enable 3v3 and others to other devices
    virtual void enablePeripherals();
    // disable 3v3 and others to other devices
    virtual void disablePeripherals();
    // enable 3v3 and others to other devices
    virtual void enableSensor();
    // disable 3v3 and others to other devices
    virtual void disableSensor();
    // check if passed pin is valid
    virtual bool checkAnalogPin(int pin_check);
};

// Hardware configuration 1 (Board 1) main
class Helper_config1_main : public HelperBase{
  // Default
};

// Hardware configuration 1 (Board 1) alternative
class Helper_config1_Board1v3838 : public HelperBase{
public:
    // set shift register to value (8 bit)
    void shiftvalue8b(uint8_t val, bool invert = false) override;
    // set shift register with more bits (32 bit)
    void shiftvalue(uint32_t val, uint8_t numBits, bool invert = false) override;
    // prepare low power mode (currently without light or deepsleep)
    void system_sleep() override;
    // controlls multiplexer to extend GPIO pins
    void controll_mux(uint8_t channel, String mode, int *val) override;
    // enable 3v3 and others to other devices
    void enablePeripherals() override;
    // disable 3v3 and others to other devices
    void disablePeripherals() override;
    // enable 3v3 and others to other devices
    void enableSensor() override;
    // disable 3v3 and others to other devices
    void disableSensor() override;
    // set pinmode of all registered pins elements to OUT/INPUT   
    void setPinModes();
    // config specific function call
    bool checkAnalogPin(int pin_check) override;

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
        SIG_MUX_1 = 39, // input pins
        ST_CP_SHFT = 26,
        DATA_SHFT = 33,
        SH_CP_SHFT = 27
    };
    // iteratble configuratoin pins
    const uint8_t output_pins[13] = {PWM, SW_SENS, SW_SENS2, SW_3_3V, EN_MUX_1, S3_MUX_1, S2_MUX_1, S1_MUX_1, S0_MUX_1, SIG_MUX_1, ST_CP_SHFT, DATA_SHFT, SH_CP_SHFT};
    // configured input pins
    const uint8_t input_pins[1] = {39};
};

// Hardware configuration 2 (Board 5)
class Helper_config1_Board5v5 : public HelperBase{
public:
    // set shift register to value (8 bit)
    void shiftvalue8b(uint8_t val, bool invert = false) override;
    // set shift register with more bits (32 bit)
    void shiftvalue(uint32_t val, uint8_t numBits, bool invert = false) override;
    // prepare low power mode (currently without light or deepsleep)
    void system_sleep() override;
    // enable 3v3 and others to other devices
    void enablePeripherals() override;
    // disable 3v3 and others to other devices
    void disablePeripherals() override;
    // enable 3v3 and others to other devices
    void enableSensor() override;
    // disable 3v3 and others to other devices
    void disableSensor() override;
    // set pinmode of all registered pins elements to OUT/INPUT
    void setPinModes();
    // config specific function call
    bool checkAnalogPin(int pin_check) override;

private:
    enum Pins {
        PWM = 32,
        SW_SENS = 4,
        SW_SENS2 = 25,
        SW_3_3V = 23,
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
extern Helper_config1_Board1v3838 HWHelper;

#endif