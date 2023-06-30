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

namespace Helper{
    String timestamp();
    void shiftvalue8b(uint8_t val); // set shift register to value (8 bit)
    void shiftvalue(uint32_t val, uint8_t numBits); // set shift register with more bits (32 bit)
    void system_sleep(); // prepare low power mode (currently without light or deepsleep; TODO!)
    void copy(int* src, int* dst, int len); // copy string from dest to src
    /*
    void watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, 
        uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm);
    */
    void controll_mux(uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val);
    /*
    bool save_datalog(String data, uint8_t cs, const char * file);
    */
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
    bool pubInfluxData(String sensor_name, String field_name, float value); // send data to influxdb, return true when everything is ok
    void blinkOnBoard(String howLong, int times); // blink onboard led to signal something
    JsonObject getJsonObjects(const char* key, const char* filepath); // returns requestet JSON object (key) of specified file
    DynamicJsonDocument getJSONData(const char* server, int serverPort, const char* serverPath); 
    bool updateConfig(const char* path); // check for config file updates from raspberrypi
    struct solenoid{
                    bool is_set; //activate deactivate group
                    uint8_t pin; //pin of the solenoid (virtual pin!)
                    uint8_t pump_pin; //pump pin (virtual pin!)
                    String name; //name of the group
                    int watering_default; //defualt value of watering amount set for group (should not be changed)
                    unsigned long timetable;
                    int watering_mqtt; //base value of watering time sent from RaspPi
                    unsigned int watering_time; //initialize on 0 always! variable will be set during watering procedure and reduced untill 0
                    unsigned long last_t; //last activation time
                    };
    struct sensors{ //contain all measurement done by esp on its adc pins
                    bool is_set; //activate measurement
                    byte pin; //virtual pin number, read function will handle that (0-15)
                    String name; //name of the measurement
                    float val; //storing measurement
                    uint8_t group_vpin; //solenoid group virtual pin         
    };
};

#endif