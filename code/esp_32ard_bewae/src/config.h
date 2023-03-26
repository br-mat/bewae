////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// Config file, setting constants and macros.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CONFIG_H__
#define __CONFIG_H__


#define BME280 1
//#define SD_log 1
#define RasPi 1

#ifndef CONFIG_FILE_PATH
#define CONFIG_FILE_PATH "/config.JSON" //specifies name of config file stored within spiffs
#endif

#ifndef SENSOR_FILE_PATH
#define SENSOR_FILE_PATH "/sensors.JSON" //specifies name of config file stored within spiffs
#endif

#ifndef CONF_FILE_SIZE
#define CONF_FILE_SIZE 2048 // estimate at https://arduinojson.org/v6/assistant/#/step1
#endif

#ifndef SENSOR_FILE_SIZE
#define SENSOR_FILE_SIZE 3072 // estimate at https://arduinojson.org/v6/assistant/#/step1
#endif

#ifndef DS3231_I2C_ADDRESS
#define DS3231_I2C_ADDRESS 0x68 //adress rtc module
#endif

#ifndef BME280_I2C_ADDRESS
#define BME280_I2C_ADDRESS 0x76 //adress bme module
#endif

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 128
#endif

#ifndef uS_TO_S_FACTOR
#define uS_TO_S_FACTOR 1000000  //Conversion factor for micro seconds to seconds
#endif
#ifndef TIME_TO_SLEEP
#define TIME_TO_SLEEP  8        //Time ESP32 will go to sleep (in seconds)
#endif

#ifndef measurement_LSB
#define measurement_LSB 0.01008018 //LSB in Volt! - REPLACE with your specific value, derives from voltage devider and
                                            //resolution of adc
#endif

#ifndef measurement_LSB5V
#define measurement_LSB5V 0.00151581243 //LSB in Volt! - REPLACE with your specific value, derives from voltage devider and
                                            //resolution of adc
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MQTT topics
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    #ifndef cliendID
    #define clientID "client_test2"
    #endif

    //mqtt topics
    #ifndef topic_prefix
    #define topic_prefix "home/bewae_data/"
    #endif

    #ifndef humidity_topic_sens2 //old
    #define humidity_topic_sens2 "home/sens2/humidity"
    #endif

    #ifndef temperature_topic_sens2 //old
    #define temperature_topic_sens2 "home/sens2/temperature"
    #endif

    #ifndef pressure_topic_sens2 //old
    #define pressure_topic_sens2 "home/sens2/pressure"
    #endif

    #ifndef config_status
    #define config_status "home/bewae/config_status" //signal Pi that system is up and listening for instructions
    #endif

    //subsciption topics
    #ifndef watering_topic
    #define watering_topic "home/bewae/config" //bewae config and config status indicate things not passed to influx recive watering instructions
    #endif
    #ifndef bewae_sw
    #define bewae_sw "home/nano/bewae_sw" //switching watering system no/off
    #endif
    #ifndef watering_sw
    #define watering_sw "home/nano/watering_sw" //switching value override on/off (default values on esp (false) or sent from pi (true))
    #endif
    #ifndef timetable_sw
    #define timetable_sw "home/nano/timetable_sw" //switch custom timetable on/off (take default (false) take sent (true))
    #endif
    #ifndef timetable_content
    #define timetable_content "home/nano/timetable" //changing timetable
    #endif
    #ifndef testing
    #define testing "home/test/tester"
    #endif
    #ifndef comms
    #define comms "home/bewae/comms" //get command from raspberrypi
    #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pin configuration
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    #ifndef sw_sens
    #define sw_sens 4 //switch mux, SD etc.
    #endif

    #ifndef en_mux_1
    #define en_mux_1 5 //mux enable
    #endif

    #ifndef chip_select
    #define chip_select 15 //SPI cs pin
    #endif

    #ifndef s3_mux_1
    #define s3_mux_1 16 //mux controll pin & s3_mux_1
    #endif

    #ifndef s2_mux_1
    #define s2_mux_1 17 //mux controll pin & s2_mux_1
    #endif

    #ifndef s1_mux_1
    #define s1_mux_1 18 //mux controll pin & s1_mux_1
    #endif

    #ifndef s0_mux_1
    #define s0_mux_1 19 //mux controll pin & s0_mux_1
    #endif

    //hardware defined IIC pin     SDA    GPIO21()
    //hardware defined IIC pin     SCL    GPIO22()
    #ifndef sw_3_3v
    #define sw_3_3v 23 //switch 3.3v output (with shift bme, rtc)
    #endif

    #ifndef sw_sens2
    #define sw_sens2 25 //switch sensor rail IMPORTANT: PIN moved to 25 layout board 1 v3 !!!!!
    #endif

    #ifndef st_cp_shft
    #define st_cp_shft 26 //74hc595 ST_CP LATCH pin
    #endif

    #ifndef sh_cp_shft
    #define sh_cp_shft 27 //74hc595 SH_CP CLK pin
    #endif

    #ifndef vent_pwm
    #define vent_pwm 32 //vent pwm output
    #endif

    #ifndef data_shft
    #define data_shft 33 //74hc595 Data
    #endif

    #ifndef sig_mux_1
    #define sig_mux_1 39 //mux sig in
    #endif

    #ifndef max_groups
    #define max_groups 6 //number of possible solenoids (v-pins)
    #endif

    #ifndef MAX_PUMPS
    #define MAX_PUMPS 2 //number of possible pumps (v-pins)
    #endif

    #ifndef measure_intervall
    #define measure_intervall 600000UL //~10 min, measurement intervall ==> MILLISECOND
    #endif

    #ifndef SOLENOID_COOLDOWN
    #define SOLENOID_COOLDOWN 30000UL //min cooldown time of each solenoid ==> MILLISECOND
    #endif
    
    //pump & transistor max on time
    #ifndef max_active_time_sec
    #define max_active_time_sec 40 //max time active of each solenoid ==> SECOND
    #endif

    #ifndef pump_cooldown_sec
    #define pump_cooldown_sec 60000UL //max time active of each pump ==> MILLISECOND
    #endif

    #ifndef pwm_ch0
    #define pwm_ch0 0 //pwm channel
    #endif

    #ifndef pwm_ch0_res
    #define pwm_ch0_res 8 //pwm resolution in bit (1-15)
    #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// default static variable definitions & virtual pins (shift register)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // virtual pins (shift register)
    #ifndef pump1
    #define pump1 (uint8_t)5
    #endif

    #ifndef pump2
    #define pump2 (uint8_t)5
    #endif

    // moisture sensors
    #ifndef low_lim
    #define low_lim 1000  //(m Volt) lower limitations, values lower are not realistic
    #endif
    
    #ifndef high_lim
    #define high_lim 1500 //(m Volt)high limitation, values passed that threshold are not realistic
    #endif

    //commands
    #ifndef stat_request
    #define stat_request 2
    #endif

#endif
