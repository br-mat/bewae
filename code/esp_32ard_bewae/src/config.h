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


#define RasPi 1

#ifndef CONFIG_FILE_PATH
#define CONFIG_FILE_PATH "/config.JSON" //specifies name of config file stored within spiffs
#endif

#ifndef WEB_PREFIX
#define WEB_PREFIX "/bewae"
#endif

#ifndef DEVICE_CONFIG_PATH
#define DEVICE_CONFIG_PATH "/get-device-config"
#endif

#ifndef IRRIG_CONFIG_PATH
#define IRRIG_CONFIG_PATH "/get-irrig-config"
#endif

#ifndef SENS_CONFIG_PATH
#define SENS_CONFIG_PATH "/get-sensor-config"
#endif

#ifndef JSON_SUFFIX
#define JSON_SUFFIX ".Json"
#endif

#ifndef CONF_FILE_SIZE
#define CONF_FILE_SIZE 5000 // estimate at https://arduinojson.org/v6/assistant/#/step1
#endif

#ifndef MAX_GROUP_LENGTH
#define MAX_GROUP_LENGTH 5 //Max Group length
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pin & Adress configuration
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef DS3231_I2C_ADDRESS
#define DS3231_I2C_ADDRESS 0x68 //adress rtc module
#endif

#ifndef BME280_I2C_ADDRESS
#define BME280_I2C_ADDRESS 0x76 //adress bme module
#endif

#ifndef oneWireBus
#define oneWireBus 5 // GPIO where the DS18B20 is connected to
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// default variable definitions & constants
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reverse shiftregister output in case of optocoupler (v-pins)
#ifndef INVERT_SHIFTOUT
#define INVERT_SHIFTOUT true // set true if using negative logic relais
#endif

// publish data flag (DEBUG FLAG)
#ifndef PUBDATA
#define PUBDATA true // set true if you dont want to pub data
#endif

// debug flag
#ifndef DEBUG
#define DEBUG 1
#endif

// debug spam flag (show all logged info)
#ifndef DEBUG_SPAM
//#define DEBUG_SPAM 1
#endif

// sensor field
#ifndef INFLUXDB_FIELD
#define INFLUXDB_FIELD "bewae-sensors"
#endif

// pump & transistor max on time
#ifndef max_active_time_sec
#define max_active_time_sec 40 //max time active of each solenoid ==> SECOND
#endif

#ifndef measure_intervall
#define measure_intervall 600000UL //~10 min, measurement intervall ==> MILLISECOND
#endif

#ifndef SOLENOID_COOLDOWN
#define SOLENOID_COOLDOWN 30000UL //min cooldown time of each solenoid ==> MILLISECOND
#endif

#ifndef DRIVER_COOLDOWN
#define DRIVER_COOLDOWN 50000UL // min cooldown of drivers and hardware (somewhat depending on max_active_time_sec)
#endif

// number of possible solenoids (v-pins)
#ifndef max_groups
#define max_groups 16
#endif

// time factors
#ifndef uS_TO_S_FACTOR
#define uS_TO_S_FACTOR 1000000 //Conversion factor for micro seconds to seconds
#endif

#ifndef TIME_TO_SLEEP
#define TIME_TO_SLEEP  8 //Time ESP32 will go to sleep (in seconds)
#endif

#ifndef measurement_LSB
#define measurement_LSB 0.01008018 //LSB in Volt! - REPLACE with your specific value, derives from voltage devider and
                                            //resolution of adc
#endif

#ifndef measurement_LSB5V
#define measurement_LSB5V 0.00151581243 //LSB in Volt! - REPLACE with your specific value, derives from voltage devider and
                                            //resolution of adc
#endif

// time & timezonerelated stuff
const int SECONDS_PER_HOUR = 3600;
const int gmtOffset_hours = 1;
const int daylightOffset_hours = 1;

#endif