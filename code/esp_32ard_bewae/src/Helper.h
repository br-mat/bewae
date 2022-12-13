#ifndef __Helper_H__
#define __Helper_H__


#include <Arduino.h>
#include "driver/adc.h"
#include <config.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <connection.h>

namespace Helper{
    String timestamp();
    void shiftvalue8b(uint8_t val);
    void system_sleep();
    void copy(int* src, int* dst, int len);
    void watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, 
        uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm);
    void controll_mux(uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val);
    bool save_datalog(String data, uint8_t cs, const char * file);
    void set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
    void read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year);
    void disableWiFi();
    bool enableWifi();
    void setModemSleep();
    void wakeModemSleep();
    void disableBluetooth();
    bool find_element(int *array, int item);
    bool load_conf(const char path[20], DynamicJsonDocument &doc);
    bool save_conf(const char path[20], DynamicJsonDocument &doc);
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