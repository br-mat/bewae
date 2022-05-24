#ifndef __Helper_H__
#define __Helper_H__

#include <Arduino.h>
#include <config.h>

namespace Helper{
    String timestamp();
    void shiftvalue8b(uint8_t val);
    void system_sleep();
    void copy(int* src, int* dst, int len);
    void watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, 
        uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm);
    void controll_mux(uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val);
    void save_datalog(String data, uint8_t cs, const char * file);
    void set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
    void read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year);
    void disableWiFi();
    bool enableWifi();
    void setModemSleep();
    void wakeModemSleep();
    bool find_element(int *array, int item);
    struct solenoid{
                    bool is_set; //activate deactivate group
                    uint8_t pin; //pin of the solenoid (virtual pin!)
                    uint8_t pump_pin; //pump pin (virtual pin!)
                    String name; //name of the group
                    int watering_base; //base value of watering time (should not be changed)
                    unsigned int watering_time; //variable will be set during watering procedure and reduced untill 0
                    unsigned long last_t; //last activation time
                    };
};

#endif