#ifndef __CONFIG_H__
#define __CONFIG_H__

#define BME280 1
#define SD_log 1
#define RasPi 1

#ifndef DS3231_I2C_ADDRESS
#define DS3231_I2C_ADDRESS 0x68 //adress bme280 sensor
#endif

#ifndef MAX_MSG_LEN
#define MAX_MSG_LEN 128
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wifi Constants
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // WiFi
    #ifndef ssid
    #define ssid "XXXXXXXXX"         // Your personal network SSID
    #endif

    #ifndef wifi_password
    #define wifi_password "XXXXXXXXX" // Your personal network password
    #endif

    // MQTT
    #ifndef mqtt_server
    #define mqtt_server "XXXXXXXXX"  // IP of the MQTT broker
    #endif

    #ifndef mqtt_username
    #define mqtt_username "XXXXXXXXX" // MQTT username
    #endif

    #ifndef mqtt_password
    #define mqtt_password "XXXXXXXXX" // MQTT password
    #endif

    #ifndef cliendID
    #define clientID "client_bewaev3" // MQTT client ID
    #endif

    #ifndef topic_prefix
    #define topic_prefix "home/sens3/z"
    #endif

    #ifndef humidity_topic_sens2
    #define humidity_topic_sens2 "home/sens2/humidity"
    #endif

    #ifndef temperature_topic_sens2
    #define temperature_topic_sens2 "home/sens2/temperature"
    #endif

    #ifndef pressure_topic_sens2
    #define pressure_topic_sens2 "home/sens2/pressure"
    #endif

    #ifndef config_status
    #define config_status "home/bewae/config_status" //send status with mqtt to raspberrypi
    #endif

    //subsciption topics
    #ifndef watering_topic
    #define watering_topic "home/bewae/config"   //bewae config and config status indicate things not passed to influx recive watering instructions
    #endif
    #ifndef bewae_sw
    #define bewae_sw "home/nano/bewae_sw"     //switching watering system no/off
    #endif
    #ifndef watering_sw
    #define watering_sw "home/nano/watering_sw" //switching value override on/off (off - using default values on nano)
    #endif
    #ifndef testing
    #define testing "home/test/tester"
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
    #define max_groups 6 //number of plant groups
    #endif

    #ifndef measure_intervall
    #define measure_intervall 600000UL //~10 min, measurement intervall ==> MILLISECOND
    #endif

    #ifndef cooldown
    #define cooldown 30000UL //min cooldown time of each solenoid ==> MILLISECOND
    #endif

    #ifndef max_active_time_sec
    #define max_active_time_sec 60 //max time active of each solenoid ==> SECOND
    #endif

    #ifndef pump_cooldown_sec
    #define pump_cooldown_sec 60000UL //max time active of each solenoid ==> MILLISECOND
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
    #define pump1 (uint8_t)7
    #endif

    #ifndef pump2
    #define pump2 (uint8_t)6
    #endif

    // moisture sensors
    #ifndef low_lim
    #define low_lim = 300;  //lower limitations, values lower are not realistic
    #endif
    #ifndef high_lim
    #define high_lim = 600; //high limitation, values passed that threshold are not realistic
    #endif

#endif
