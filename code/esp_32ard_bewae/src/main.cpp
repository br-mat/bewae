///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// br-mat (c) 2022
// email: matthiasbraun@gmx.at                                                       
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Standard
//#include <ArduinoSTL.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <PubSubClient.h> //problems with arduino framework?

#include <Helper.h>
#include <config.h>


using namespace std;

#define DEBUG
//#define ESP32

using namespace Helper;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Global variables                                                     
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//limitations & watering & other
//group limits and watering (some hardcoded stuff)                          
//          type                  amount of plant/brushes    estimated water amount in liter per day (average hot +30*C)
//group 1 - große tom             5 brushes                  ~8l
//group 2 - Chilli, Paprika       5 brushes                  ~1.5l
//group 3 - Kräuter (trocken)     4 brushes                  ~0.2l
//group 4 - Hochbeet2             10 brushes                 ~4l
//group 5 - kleine tom            4 brushes                  ~3.5l
//group 6 - leer 4 brushes +3 small?        ~0.5l

//stay global for access through more than one iteration of loop
//bool, pin, pump_pin, name, watering base, watering time, last act,
solenoid group[] =
{ 
  {true, 0, pump1, "Tom1", 120, 0, 0}, //group1
  {true, 1, pump1, "Not set", 10, 0, 0}, //group2
  {true, 2, pump1, "Not set", 5, 0, 0}, //group3
  {true, 3, pump1, "Not set", 30, 0, 0}, //group4
  {true, 4, pump1, "Not set", 60, 0, 0}, //group5
  {false, 5, pump1, "Not set", 0, 0, 0}, //group6
};

//important global variables
byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_;
unsigned long up_time = 0;    //general estimated uptime
unsigned long last_activation = 0; //timemark variable
bool thirsty = false; //marks if a watering cycle is finished
bool config = false;  //handle wireless configuration
bool config_bewae_sw = true; //switch watering on off
bool config_watering_sw = true; //switch default and custom values, default in group_st_time and custom sent via mqtt

//wireless config array to switch on/off functions
// watering time of specific group; binary values;
const int raspi_config_size = max_groups+2; //6 groups + 2 binary
int raspi_config[raspi_config_size]={0};

//int groups[max_groups] = {0};
byte sw0; //bewae switch
byte sw1; //water value override switch

//timetable storing watering hours
//                                    2523211917151311 9 7 5 3 1
//                                     | | | | | | | | | | | | |
unsigned long int timetable = 0b00000000000001000000010000000000;
//                                      | | | | | | | | | | | |
//                                     2422201816141210 8 6 4 2 

Adafruit_BME280 bme; // use I2C interface

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wifi & Pubsubclient                                           
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise the WiFi and MQTT Client objects
WiFiClient wificlient;

PubSubClient client(wificlient);

void callback(char *topic, byte *payload, unsigned int msg_length);
bool connect_MQTT();

// Custom function to connet to the MQTT broker via WiFi
bool connect_MQTT(){
  WiFi.mode(WIFI_STA);
  delay(1);
  #ifdef DEBUG
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
  // Connect to the WiFi
  WiFi.begin(ssid, wifi_password);
  int tries = 0;
  // Wait until the connection has been confirmed before continuing
  while ((WiFi.status() != WL_CONNECTED)) {
    delay(500);
    #ifdef DEBUG
    Serial.print(" . ");
    #endif
    if(tries == 10) {
      WiFi.begin(ssid, wifi_password);
      #ifdef DEBUG
      Serial.print(F(" Trying again "));
      Serial.print(tries);
      #endif
      delay(5000);
    }
    tries++;
    if(tries > 20){
      return false;
    }
  }
  #ifdef DEBUG
  // Debugging - Output the IP Address of the ESP32
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
  // Connect to MQTT Broker
  // client.connect returns a boolean value to let us know if the connection was successful.
  // If the connection is failing, make sure you are using the correct MQTT Username and Password (Setup Earlier in the Instructable)
  if (client.connect(clientID, mqtt_username, mqtt_password)) {
    #ifdef DEBUG
    Serial.println("Connected to MQTT Broker!");
    #endif
    client.setCallback(callback);
    client.subscribe(watering_topic, 0); //(topic, qos) qos 0 fire and forget, qos 1 confirm at least once, qos 2 double confirmation reciever
    client.subscribe(bewae_sw, 0);
    client.subscribe(watering_sw, 0);
    client.subscribe(testing, 0);
    client.setCallback(callback);
    //client.loop();
    return true;
  }
  else {
    #ifdef DEBUG
    Serial.println("Connection to MQTT Broker failed...");
    #endif
    return false;
  }
}

//callback function to receive mqtt data
//watering topic payload rules:
//',' is the sepperator -- no leading ',' -- msg can end with or without ',' -- only int numbers
// -- max_groups should be the same as in code for nano!!!
void callback(char *topic, byte *payload, unsigned int msg_length){
  #ifdef DEBUG
  Serial.print(F("Enter callbacktopic: "));
  Serial.println(topic);
  #endif
  // copy payload to a string
  String msg="";
  if(msg_length > MAX_MSG_LEN){ //limit msg length
    msg_length = MAX_MSG_LEN;
    #ifdef DEBUG
    Serial.print(F("Message got cut off, allowed: "));
    Serial.println(MAX_MSG_LEN);
    #endif
  }
  for(int i=0; i< msg_length; i++){
    msg+=String((char)payload[i]);
  }
  //trigger correct topic to decode msg
  if(String(watering_topic) == topic){
    int c_index=0;
    for(int i=0; i<max_groups; i++){
      String val="";
      int start_index = c_index;
      c_index=msg.indexOf(",",start_index)+1; //find point of digit after the sepperator ','
      if((int)c_index == (int)-1){            //index = -1 means char nor found
        c_index=msg_length+1;      //add 1 to get full message range in case there is no ',' on payload end
      }
      //extracting csv values
      for(start_index; start_index<c_index-1; start_index++){
        val+=msg[start_index];
      }
      //groups[i]=val.toInt();
      //TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO
      #ifdef DEBUG
      Serial.print(F("MQTT val received:"));
      Serial.println(val.toInt());
      Serial.print(F("index: "));
      Serial.println(i);
      #endif
    }
  }
  if(String(bewae_sw) == topic){
    sw0 = msg.toInt();
    #ifdef DEBUG
    Serial.println(F("watering switched"));
    #endif
  }
  if(String(watering_sw) == topic){
    sw1 = msg.toInt();
    #ifdef DEBUG
    Serial.println(F("watering-values switched"));
    #endif
    //data_collected=true; //!!!!!!!!!!!!!!!!!!!! replace later last sent by raspi
  }
  /*
  if(msg_count+1>3){
    data_collected=true;
    statusbyte=111;
  }
  */
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP                                                       
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// configure pin mode                                                             ESP32 port
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(sw_sens, OUTPUT);       //switch mux, SD etc.                           GPIO04()
  pinMode(sw_sens2, OUTPUT);      //switch sensor rail                            GPIO36()
  pinMode(sw_3_3v, OUTPUT);       //switch 3.3v output (with shift bme, rtc)      GPIO23()
  pinMode(s0_mux_1, OUTPUT);      //mux controll pin & s0_mux_1                   GPIO19()
  pinMode(s1_mux_1, OUTPUT);      //mux controll pin & s1_mux_1                   GPIO18()
  pinMode(s2_mux_1, OUTPUT);      //mux controll pin & s2_mux_1                   GPIO17()
  pinMode(s3_mux_1, OUTPUT);      //mux controll pin & s3_mux_1                   GPIO16()
  pinMode(sh_cp_shft, OUTPUT);    //74hc595 SH_CP                                 GPIO27()
  pinMode(st_cp_shft, OUTPUT);    //74hc595 ST_CP                                 GPIO26()
  pinMode(data_shft, OUTPUT);     //74hc595 Data                                  GPIO33()
  pinMode(vent_pwm, OUTPUT);      //vent pwm output                               GPIO32()
  pinMode(chip_select, OUTPUT);   //SPI CS                                        GPIO15()
  //hardware defined SPI pin      //SPI MOSI                                      GPIO13()
  //hardware defined SPI pin      //SPI MISO                                      GPIO12()
  //hardware defined SPI pin      //SPI CLK                                       GPIO14()
  //pinMode(A0, INPUT);             //analog in?                                    GPIO()
  pinMode(sig_mux_1, INPUT);      //mux sig in                                    GPIO39()
  pinMode(en_mux_1, OUTPUT);      //mux enable                                    GPIO05() bei boot nicht high!
  //hardware defined IIC pin      //A4  SDA                                       GPIO21()
  //hardware defined IIC pin      a//A5  SCL                                       GPIO22()
  //input only                                                                    (N)GPIO22(34)
  //input only 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize serial
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize bme280 sensor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(sw_3_3v, HIGH); delay(5);
  #ifdef BME280
  Serial.println(F("BME280 Sensor event test"));
  if (!bme.begin()) {
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    int it = 0;
    while (1){
      delay(10);
      it++;
      if(it > 100){
        break;
      }
    }
  }
  #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize PWM:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //  ledcAttachPin(GPIO_pin, PWM_channel);
  ledcAttachPin(vent_pwm, pwm_ch0);
  //Configure this PWM Channel with the selected frequency & resolution using this function:
  // ledcSetup(PWM_Ch, PWM_Freq, PWM_Res);
  ledcSetup(pwm_ch0, 30000, pwm_ch0_res);
  //control this PWM pin by changing the duty cycle:
  // ledcWrite(PWM_Ch, DutyCycle); //max 2^resolution
  //ledcWrite(pwm_ch0, pow(2, pwm_ch0_res) * fac);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init time and date
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time
  //set_time(01,42,17,02,30,07+2,20);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear
  #ifdef DEBUG
  Serial.println(F("debug statement"));
  #endif
  //initialize global time
  Helper::read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  #ifdef DEBUG
  Serial.println(F("debug 0"));
  #endif

  delay(500);  //make TX pin Blink 2 times, visualize end of setup
  Serial.print(measure_intervall);
  delay(500);
  Serial.println(measure_intervall);
  
  delay(500);
  
  system_sleep(); //turn off all external transistors
  setModemSleep(); //shut down wifi
}

unsigned long startLoop = millis();
void loop(){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// start loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
Serial.println(F("start loop setup"));
#endif

Wire.beginTransmission(DS3231_I2C_ADDRESS);
byte rtc_status = Wire.endTransmission();
if(rtc_status != 0){
  #ifdef DEBUG
  Serial.println(F("Error: rtc device not available!"));
  #endif
  while(rtc_status != 0){
    //loop as long as the rtc module is unavailable!
    #ifdef DEBUG
    Serial.print(F(" . "));
    #endif
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    rtc_status = Wire.endTransmission();
    system_sleep(); //turn off all external transistors
    setModemSleep(); //shut down wifi
    esp_sleep_enable_timer_wakeup(8000);
    esp_light_sleep_start();
  }
}
pinMode(sw_3_3v, HIGH); delay(10);
//delay(200);
byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;
read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
if(hour_ != hour1){
//if(true){
  up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
  //if(1){
  if(bitRead(timetable, hour1)){
  //if(find_element() & (hour1 == (byte)11) | (hour1 == (byte)19)){
    #ifdef RasPi
    config = true; //request config update from pi
    #endif
    thirsty = true; //initialize watering phase
    //TODO? request or derive the watering amount!

//TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO TODO 
// use solenoid struct and set watering_time !
  }
}
//update global time related variables
unsigned long loop_t = millis();
unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL; //time in ms, comming from rtc module
                                                                                                //give acurate values despite temperature changes

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// collect & save or send data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
Serial.print(F("Datalogphase: ")); Serial.println(actual_time-last_activation > measure_intervall);
Serial.println(actual_time); Serial.println(last_activation); Serial.println(measure_intervall-500000UL);
Serial.println((float)((float)actual_time-(float)last_activation)); Serial.println(up_time);
#endif
//if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall-500000UL)) //replace for debuging
if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall)) //use ds3231 based time read for more stability
//if(true)
//if(false)
{
  last_activation = actual_time; //first action refresh the time
  #ifdef DEBUG
  Serial.println(F("enter datalog phase"));
  #endif

  //turn on sensors
  delay(1);
  digitalWrite(sw_sens, HIGH);   //activate mux & SD
  int value=0; //mux reading value
  int data2[28] = {0}; //create data array
  controll_mux(11, sig_mux_1, en_mux_1, "read", &value); //take battery measurement bevore devices are switched on
  digitalWrite(sw_sens2, HIGH);   //activate sensor rail
  data2[4] = value;                                       //--> battery voltage (low load)
  delay(100); //give sensor time to stabilize voltage
  for(int i=0; i<16; i++){
  controll_mux(i, sig_mux_1, en_mux_1, "read", &value);
  delay(1); //was 3
  data2[6+i] = value;
  }
  #ifdef BME280
  data2[0] = (float)bme.readPressure() / (float)10+0;     //--> press reading
  data2[1] = (float)bme.readHumidity() * (float)100+0;    //--> hum reading
  data2[2] = (float)bme.readTemperature() * (float)100+0; //--> temp reading
  #elif
  //possible dht solution?
  #endif

  // --- data shape---
  //data2[0] = (float)bme280.getPressure() / (float)10+0;     //--> press reading
  //data2[1] = (float)bme280.getHumidity() * (float)100+0;    //--> hum reading
  //data2[2] = (float)bme280.getTemperature() * (float)100+0; //--> temp reading
  //data2[3] = readVcc();                     //--> vcc placeholder (not active with esp right now)
  //data2[4] = battery_indicator;             //--> battery voltage (low load)
  //data2[5] = 0;                             //--> placeholder
  //data 6-21                                 //--> 15 Mux channels
  //data2[22]=0;
  //data2[23]=0;
  //data2[24]=0;
  //data2[25]=0;
  //data2[26]=0;
  //data2[27]=0;

  //delay(500);
  digitalWrite(sw_sens2, LOW);   //deactivate sensor rail

  // --- log data to SD card (backup) ---
  #ifdef SD_log
  String data="";
  data += timestamp();
  data += ",";
  for(uint8_t i=0; i<28; i++){
    data += data2[i];
    data += ",";
  }
  delay(100); //give time
  save_datalog(data, chip_select, "datalog2.txt");
  #ifdef DEBUG
  Serial.print(F("data:"));
  Serial.println(data);
  #endif
  data=""; //clear string
  delay(150);
  digitalWrite(sw_sens, LOW);   //deactivate mux & SD
  #endif //sd log

  // --- log to INFLUXDB ---
  #ifdef RasPi

  #endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// recieve commands
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// IMPLEMENT: check for new data and sw conditions on esp sent rom raspberrypi
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if((esp_status == (byte)1) & (config)){
      config = false;
      config_bewae_sw=(bool)raspi_config[max_groups];
      config_watering_sw=(bool)raspi_config[max_groups+1];
      #ifdef DEBUG
      Serial.print(F("config triggered, bool values: ")); Serial.print(config_bewae_sw);
      Serial.print(F(" ")); Serial.println(config_watering_sw);
      #endif
      if(config_watering_sw){
        copy(raspi_config, watering_base, max_groups);
        for(int i=0; i<max_groups; i++){
          watering_base[i]=raspi_config[i];
        }
      }
      if(!config_bewae_sw){
        thirsty = false;
        int zeros[max_groups]={0};
        copy(raspi_config, zeros, max_groups);
      }
      //delay(1000);
    }
  }
  //test   test   test   test   test   test   test   test   test   test   test   test   test   test   test   test   
  Serial.println("test values raspi");
  for(int i=0; i < 8; i++){
    Serial.println(raspi_config[i]);
  }
  */
#ifdef RasPi
/*
switch(cmd[1]){
  case :
}
*/
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(thirsty){
  #ifdef DEBUG
  Serial.println(F("start watering phase"));
  #endif
  // --- Watering ---
  //trigger at specific time
  //alternate the solenoids to avoid heat damage, let cooldown time if only one remains
  //Hints:  main mosfet probably get warm (check that!)
  //        pause procedure when measure events needs to happen
  //        NEVER INTERUPT WHILE WATERING!!!!!!!!!!!!!!!!!!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while((loop_t + measure_intervall > millis()) & (thirsty)){
    int len = sizeof(group)/sizeof(group[0]);
    unsigned int pump_t = 0;
    struct solenoid* ptr = group;
    for (int i=0; i<len; i++, ptr++ ) {
      if((ptr->last_t + cooldown > millis()) & (ptr->watering_time > 0)) //minimum cooldown of 30 sec
      {
        unsigned int water_timer = 0;
        if (ptr->watering_time < max_active_time_sec)
        {
          water_timer = ptr->watering_time;
          ptr->watering_time = 0;
        }
        else{
          water_timer = 60;
          ptr->watering_time -= 60;
        }
        if(water_timer > 60){ //sanity check
          #ifdef DEBUG
          Serial.println(F("Warning watering timer over 60!"));
          #endif
          water_timer = 60;
        }
        #ifdef DEBUG
        Serial.println(F("Watering group: ")); Serial.print(ptr->name); Serial.println(F(";  "));
        Serial.println(water_timer); Serial.println(F(" seconds")); Serial.println(F(";  "));
        #endif
        pump_t += water_timer;
        watering(s0_mux_1, s2_mux_1, s1_mux_1, water_timer, ptr->pin, ptr->pump_pin, sw_3_3v, vent_pwm);
        ptr->last_t = millis();
      }
    }
    if((pump_t > pump_cooldown_sec) & (loop_t + measure_intervall > millis())){
      pump_t = 0;
      esp_sleep_enable_timer_wakeup(pump_cooldown_sec);
      esp_light_sleep_start();
    }
    else{
    //send esp to sleep
    esp_sleep_enable_timer_wakeup(2500);
    esp_light_sleep_start();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sleep
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

if (loop_t + measure_intervall > millis()){
  unsigned long sleep = loop_t + measure_intervall - millis();
  if(sleep < 0 ){
    sleep = 1000;
  }
  system_sleep(); //turn off all external transistors
  setModemSleep(); //shut down wifi
  esp_sleep_enable_timer_wakeup(sleep);
  esp_light_sleep_start(); //deep sleep also possible
}
}