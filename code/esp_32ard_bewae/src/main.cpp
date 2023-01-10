////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This is the main source file for the irrigation system. It is responsible for monitoring all sensor values,
// handling the watering procedure, and communicating with the Raspberry Pi to get and send data.
//
// Dependencies:
// - Arduino.h
// - Wire.h
// - SPI.h
// - SD.h
// - SPIFFS.h
// - Adafruit_Sensor.h
// - Adafruit_BME280.h
// - WiFi.h
// - PubSubClient.h
// - ArduinoJson.h
// - connection.h
// - Helper.h
// - IrrigationController.h
// - config.h
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Standard
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "SPIFFS.h"

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <connection.h>
#include <Helper.h>
#include <IrrigationController.h>
#include <config.h>

using namespace std;

#define DEBUG 1

using namespace Helper;

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- GLOBAL VARIABLES AND DEFINITIONS --------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

//limitations & watering & other
//group limits and watering (some hardcoded stuff)                          
//          type                  amount of plant/brushes    estimated water amount in liter per day (average hot +30*C)
//group 1 - große tom             5 brushes                  ~8l
//group 2 - Chilli, Paprika       5 brushes                  ~1.5l
//group 3 - Kräuter (trocken)     4 brushes                  ~0.2l
//group 4 - Hochbeet2             10 brushes                 ~4l
//group 5 - kleine tom            4 brushes                  ~3.5l
//group 6 - leer 4 brushes +3 small?        ~0.5l

//stay global for access through more than one iteration of loop (keep memory in mind)
//is_set, v-pin, pump_pin, name, watering default, timetable, watering base, watering time, last act,
solenoid group[max_groups] =
{ 
  {true, 0, pump1, "Tom1", 100, 0, 0, 0, 0}, //group0
  {true, 1, pump1, "Tom2", 80, 0, 0, 0, 0}, //group1
  {true, 2, pump1, "Gewa", 30, 0, 0, 0, 0}, //group2
  {true, 3, pump1, "Chil", 40, 0, 0, 0, 0}, //group3
  {true, 6, pump1, "Krtr", 20, 0, 0, 0, 0}, //group4
  {true, 7, pump1, "Erdb", 50, 0, 0, 0, 0}, //group5
};


//NEW IMPLEMENTATION
// The idea is to use the config file to controll the watering groups. This way 
// with sending the JSON data we can adjust and initialize new groups over the air.
// The switch conditions and timetables get reduced too as there is no need for handling outages of
// wifi controll as the controller will then use what is stored locally in the config file.

// Create new solenoid struct and initialize their corresponding virtual pins
Solenoid controller_sollenoids[6] =
{
  {0}, //solenoid 0
  {1}, //solenoid 1
  {2}, //solenoid 2
  {3}, //solenoid 3
  {6}, //solenoid 4
  {7}, //solenoid 5
};

// Create a new Pump struct and initialize their corresponding virtual pins
Pump pump_main[2] = 
{
  pump1,
  pump2,
};

//IrrigationController
// No need for initialization as it will be loaded from config file


//timetable storing watering hours
//                                           2523211917151311 9 7 5 3 1
//                                            | | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000000000000010000000000;
//                                             | | | | | | | | | | | | |
//                                            2422201816141210 8 6 4 2 0
unsigned long int timetable = timetable_default; //initialize on default
unsigned long int timetable_raspi = 0; //initialize on default

//is_set, pin, name, val, group
sensors measure_point[16] =
{
  {true, 0, "Tom-RR", 0.0, 0},
  {true, 1, "Chl-GW", 0.0, 0},
  {true, 2, "Krt-HB", 0.0, 1},
  {false, 3, "Tom-RC", 0.0, 1},
  {false, 4, "Tom-FL", 0.0, 0},
  {false, 5, "Pep-Bl", 0.0, 2},
  {false, 6, "Pep-5f", 0.0, 2},
  {false, 7, "Krt-Ba", 0.0, 3},
  {false, 8, "HoB-1", 0.0, 4},
  {false, 9, "HoB-2", 0.0, 5},
  {false, 10, "Tom-GR", 0.0, max_groups+1},
  {false, 11, "Tom-JC", 0.0, max_groups+1},
  {false, 12, "test12", 0.0, max_groups+1},
  {false, 13, "test13", 0.0, max_groups+1},
  {true, 14, "Bat-12", 0.0, max_groups+1},
  {false, 15, "pht-rs", 0.0, max_groups+1},
};

sensors bme_point[3] =
{
  {true, 0, "temp280", 0.0, max_groups+1},
  {true, 0, "pres280", 0.0, max_groups+1},
  {true, 0, "humi280", 0.0, max_groups+1},
};

//important global variables
byte sec_; byte min_; byte hour_; byte day_w_; byte day_m_; byte mon_; byte year_;
unsigned long up_time = 0;    //general estimated uptime
unsigned long last_activation = 0; //timemark variable
bool thirsty = false; //marks if a watering cycle is finished
bool config = false;  //handle wireless configuration

//wireless config array to switch on/off functions
// watering time of specific group; binary values;
//const int raspi_config_size = max_groups+2; //6 groups + 2 binary
//int raspi_config[raspi_config_size]={0};

bool post_DATA = true; //controlls if data is sent back to pi or not
bool msg_stop = false; //is config finished or not
bool sw6 = 1;  //system switch (ON/OFF) NOT IMPLEMENTED YET
bool sw0 = 1; //bewae switch (ON/OFF)
bool sw1 = 0; //water time override condition
bool sw2 = 0; //timetable override condition

Adafruit_BME280 bme; // use I2C interface

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SOME ADDITIONAL FUNCTIONS ---------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################
// Initialise the WiFi and MQTT Client objects
WiFiClient wificlient;
PubSubClient client(wificlient);

//define skeletton
void callback(char *topic, byte *payload, unsigned int msg_length);
bool connect_MQTT();
bool msg_mqtt();
void sub_mqtt();

//callback function to receive mqtt data
//watering topic payload rules:
//',' is the sepperator -- no leading ',' -- msg can end with or without ',' -- only int numbers
// -- max_groups should be the same as in code for nano!!!
void callback(char *topic, byte *payload, unsigned int msg_length){
  #ifdef DEBUG
  Serial.print(F("Enter callback topic: "));
  Serial.println(topic);
  #endif
  // copy payload to a string
  String msg="";
  if(msg_length > MAX_MSG_LEN){ //limit msg length
    msg_length = MAX_MSG_LEN;
    #ifdef DEBUG
    Serial.print(F("Message got cut off, allowed len: "));
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
      int start_index = c_index; //use old index position
      c_index=msg.indexOf(",",start_index)+1; //find point of digit after the sepperator ','
      if((int)c_index == (int)-1){            //index = -1 means char nor found
        c_index=msg_length+1;                 //add 1 to reach end of string
      }
      //extracting csv values
      for(start_index; start_index<c_index-1; start_index++){
        val+=msg[start_index];
      }
      group[i].watering_mqtt = val.toInt(); //changing watering value of the valve

      #ifdef DEBUG
      Serial.print(F("MQTT val received:"));
      Serial.println(val.toInt());
      Serial.print(F("index: "));
      Serial.println(i);
      #endif
    }
  }

  if(String(bewae_sw) == topic){
    sw0 = (bool)msg.toInt(); //toInt returns long! naming is confusing
    #ifdef DEBUG
    Serial.print(F("Watering switched: ")); Serial.println(sw0);
    #endif
  }

  if(String(watering_sw) == topic){
    sw1 = (bool)msg.toInt(); //toInt returns long! naming is confusing
    #ifdef DEBUG
    Serial.println(F("watering-values changed, transmitted values will be considered"));
    #endif
  }

  if(String(testing) == topic){
    #ifdef DEBUG
    Serial.println(F("test msg: ")); Serial.println(msg);
    #endif
  }

  if(String(timetable_sw) == topic){
    sw2 = (bool)msg.toInt(); //toInt returns long! naming is confusing
    #ifdef DEBUG
    Serial.println(F("custom timetable changed: ")); Serial.println(sw2);
    #endif
  }

  if(String(timetable_content) == topic){ //sent timetable
    timetable_raspi = (unsigned long) msg.toInt(); //toInt returns long! naming is confusing
    #ifdef DEBUG
    Serial.println(F("timetable changed"));
    #endif
  }

  // TODO implement command function consisting of two letters and a msg part
  //   this function should be able to set watering times (probably values too?)
  //   and turn on/off measureing watering etc.
  //
  // Command: (1 letter) - (32 digits)
  // EXAMPLE: STOP = "X8"
  if(String(comms) == topic){
    if(msg.length() > 33){
      msg = "Z00000000000000000000000000000000";
      #ifdef DEBUG
      Serial.println(F("command message too long."));
      #endif
      msg_stop = true;
      return;
    }
    String pt0 = msg.substring(0, 1);
    char buff[1];
    pt0.toCharArray(buff, 1);
    String msg_num = msg.substring(1, 33);
    long full = msg_num.toInt();
    int high = full >> 16; //high int
    int low = (int16_t)full; //lower int
    #ifdef DEBUG
    Serial.println(F("Command DEBUG:")); Serial.print(F("MSG: ")); Serial.println(msg);
    Serial.print(F("pt0: ")); Serial.print(F("pt0")); Serial.print(F(" - buff[0]: ")); Serial.println(buff[0]);
    Serial.print(F("high bits: ")); Serial.println(high);
    Serial.print(F("low bits: ")); Serial.println(low);
    #endif
    switch(buff[0])
    {
      case 'S': //switch features ON/OFF - only take high (MSB) int part
        switch(high){
          case 6:
            #ifdef DEBUG
            Serial.print(F("System: ")); Serial.println(low);
            #endif
            sw6 = low; //everything ON
            break;

          case 2:
            #ifdef DEBUG
            Serial.print(F("bewae: ")); Serial.println(low);
            #endif
            sw0 = low; //bewae ON
            break;

          case 3:
            #ifdef DEBUG
            Serial.print(F("water time: ")); Serial.println(low);
            #endif
            sw1 = low; //water time override ON
            break;

          case 4:
            #ifdef DEBUG
            Serial.print(F("custom timetable: ")); Serial.println(low, BIN);
            #endif
            sw2 = low; //timetable override ON
            break;

          case 5:
            #ifdef DEBUG
            Serial.print(F("publish DATA: ")); Serial.println(low);
            #endif
            post_DATA = low; //sending Data to PI ON
            break;

          default:
            #ifdef DEBUG
            Serial.println(F("Warning: dont know this command"));
            #endif
            break;
        }
        break;

      case 'T':
        int group_index;
        group_index = (high & 0xFF00)>>8; //get high 8 bits as group index
        high = high & 0x00FF; //get lower 8 bits as rest of timetable transmission
        group[group_index].timetable = (long)high<<16 + low;
        #ifdef DEBUG
        Serial.print(F("Timetable set to: ")); Serial.println(timetable_raspi);
        #endif
        break;

      case 'W': //watering times - (int) group (v pin number) - (int) value
      { //care: initialize variables can cause problems within case block, therefor {}
        int len = sizeof(group)/sizeof(group[0]);
        struct solenoid* ptr = group;
        for (int i=0; i<len; i++, ptr++){
          if((int)ptr->pin == high){
            ptr->watering_time = low;
          }
        }
        break;
      }

      case 'X': //indicates finished configuration
        if(high == 8){
          msg_stop = true;
          #ifdef DEBUG
          Serial.println(F("Stop message."));
          #endif
        }
        else{
          msg_stop = true;
          #ifdef DEBUG
          Serial.println(F("Warning stop message might be incorrect or noisy."));
          #endif
        }
        break;

      case 'Z': //transmission failure
          #ifdef DEBUG
          Serial.println(F("Warning: transmission failure"));
          #endif
          break;

      default:
        #ifdef DEBUG
        Serial.println(F("Warning: Default command triggered"));
        #endif
        break;
    }

    #ifdef DEBUG
    Serial.println(F("command recieved"));
    #endif
  }
}


void sub_mqtt(){
  if(WiFi.status() != WL_CONNECTED){
    client.subscribe(watering_topic, 1);  //watering values (val1, val2, val3, ...)
    client.subscribe(bewae_sw, 1); //switch on/off default values for watering
    client.subscribe(watering_sw, 1); //switch on/off watering procedure
    client.subscribe(timetable_sw, 1); //switch on/off custom timetable
    client.subscribe(timetable_content, 1); //change timetable
    client.subscribe(testing, 1);
    client.subscribe(comms, 1); //commands from Pi
    client.setCallback(callback);
    client.loop();
  }
}


// Custom function to connet to the MQTT broker via WiFi
bool connect_MQTT(){
  // initial connect attempt
  // return true when everything went well, false if not
  enableWifi();

  //failure management
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    delay(100);
    #ifdef DEBUG
    Serial.print("Connecting to ");
    Serial.println(ssid);
    #endif
    // Connect to the WiFi
    WiFi.begin(ssid, wifi_password);
  }
  
  int tries = 0;
  //retry until the connection has been confirmed before continuing
  while ((WiFi.status() != WL_CONNECTED)) {
    delay(1000);
    #ifdef DEBUG
    Serial.print(" . ");
    #endif
    if(tries == 15) {
      //try reactivate wifi
      disableWiFi();
      delay(100);
      enableWifi();
      delay(2500);
      WiFi.begin(ssid, wifi_password);
      #ifdef DEBUG
      Serial.print(F(" Trying again "));
      Serial.println(tries);
      #endif
      
    }
    tries++;
    if(tries > 30){
      #ifdef DEBUG
      Serial.println();
      Serial.print(F("Error: Wifi connection could not be established! "));
      Serial.println(tries);
      #endif
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
  bool connection = client.connect(clientID, mqtt_username, mqtt_password);
  int mqtt_i = 0;
  #ifdef DEBUG
  Serial.println(F("Connect to MQTT"));
  #endif
  while(!connection){
    connection = client.connect(clientID, mqtt_username, mqtt_password);
    #ifdef DEBUG
    Serial.print(F(" . "));
    #endif
    //timeout condition
    if(mqtt_i>30){
      #ifdef DEBUG
      Serial.print("Connection to MQTT Broker failed: ");
      Serial.println(connection);
      #endif
      return false;
    }
    delay(1000);
  }
  #ifdef DEBUG
  Serial.println("Connected to MQTT Broker!");
  #endif
  //client.setCallback(callback);
  //client.subscribe(topic, qos) qos 0 fire and forget, qos 1 confirm at least once, qos 2 double confirmation reciever
  sub_mqtt();
  return true;
}


bool msg_mqtt(String topic, String data){
  //This function take topic and data as String and publishes it via MQTT
  //it returns a bool value, where 0 means success and 1 is a failed attemt

  //input: String topic, String data
  //return: false if everything ok otherwise true

  #ifdef DEBUG
  Serial.println(F("msg_mqtt func called"));
  #endif

  // Try to reconnect to the MQTT broker if not already connected
  int reconnectAttempts = 0;
  const int MAX_RECONNECT_ATTEMPTS = 5;
  while(!client.connected() && reconnectAttempts < MAX_RECONNECT_ATTEMPTS){
    #ifdef DEBUG
    Serial.print(F("Client not connected trying to reconnect!"));
    #endif
    wakeModemSleep();
    if(!connect_MQTT()){
      reconnectAttempts++;
    }
    else{
      client.loop();
      break;
    }
  }

  // If the MQTT client failed to reconnect to the broker after multiple attempts, return a failure
  if(!client.connected()){
    #ifdef DEBUG
    Serial.println(F("ERROR: failed to reconnect to MQTT broker"));
    #endif
    return false;
  }

  // Attempt to publish the data
  if(client.publish(topic.c_str(), data.c_str())){
    #ifdef DEBUG
    Serial.print(F("Data sent: ")); Serial.println(data.c_str()); Serial.println(topic.c_str());
    #endif
    return false;
  }
  else{
    // Attempt to reconnect to the broker and publish the data again
    if(connect_MQTT()){
      sub_mqtt();
      if(client.publish(topic.c_str(), data.c_str())){
        #ifdef DEBUG
        Serial.println(F("Data sent!"));
        #endif
        return false;
      }
    }
    #ifdef DEBUG
    Serial.println(F("ERROR no data sent!"));
    #endif
    return true;
  }
}


bool pub_data(struct sensors* output_data, unsigned int length){
  //This function should take an array and publish the content via mqtt
  //input: a pointer array of sensors struct, its unsigned int length
  //return: false on success, true if any problems occured
  if(!client.connected()){
    #ifdef DEBUG
    Serial.print(F("Client not connected trying to reconnect!"));
    #endif
    wakeModemSleep();
    connect_MQTT();
    client.loop();
  }

  #ifdef DEBUG
  Serial.println(F("pub_data func called"));
  #endif
  unsigned int problem = 0;
  for (int i=0; i<length; i++, output_data++ ) {
    if(output_data->is_set){
      String name = output_data->name;
      String val = String(output_data->val);
      String topic = topic_prefix + name;
      problem += msg_mqtt(topic, val);
    }
  }
  if((bool)problem){
    #ifdef DEBUG
    Serial.println(F("Problem occured while publishing data! count: ")); Serial.println(problem);
    #endif
  }

  return (bool)problem;
}

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- SETUP -----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

void setup() {
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// set clock speed
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //setCpuFrequencyMhz(80);
  client.setServer(mqtt_server, 1883);//connecting to mqtt server
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// configure pin mode                                                             ESP32 port
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  pinMode(sw_sens, OUTPUT);       //switch mux, SD etc.                           GPIO04()
  pinMode(sw_sens2, OUTPUT);      //switch sensor rail                            GPIO25()
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
  //pinMode(A0, INPUT);             //analog in?                                  GPIO()
  pinMode(sig_mux_1, INPUT);      //mux sig in                                    GPIO39()
  pinMode(en_mux_1, OUTPUT);      //mux enable                                    GPIO05() bei boot nicht high!
  //hardware defined IIC pin      //A4  SDA                                       GPIO21()
  //hardware defined IIC pin      //A5  SCL                                       GPIO22()
  //input only                                                                    (N)GPIO22(34)
  //input only 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize serial
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  delay(1);
  #ifdef DEBUG
  Serial.println("Hello!");
  #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize SPIFFS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  File file = SPIFFS.open(CONFIG_FILE_PATH);
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  #ifdef DEBUG
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
  #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize bme280 sensor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  digitalWrite(sw_3_3v, HIGH); delay(5);
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set shift reigster to value (0)
  digitalWrite(st_cp_shft, HIGH); //write our to shift register output
  delay(1);
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(sw_sens, HIGH);
  digitalWrite(sw_sens2, HIGH);

  Serial.println(F("BME280 Sensor event test"));
  if (!bme.begin(BME280_I2C_ADDRESS)) {
    #ifdef DEBUG
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    #endif
    int it = 0;
    while (1){
      delay(25);
      it++;
      #ifdef DEBUG
      Serial.println(F("."));
      #endif
      if(it > 100){
        break;
      }
    }
  }
  #ifdef DEBUG
  else{
    float temp_test = bme.readTemperature();
    Serial.print(F("Temperature reading: ")); Serial.println(temp_test);
  }
  #endif
  delay(500);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize PWM:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  //  ledcAttachPin(GPIO_pin, PWM_channel);
  ledcAttachPin(vent_pwm, pwm_ch0);
  //Configure this PWM Channel with the selected frequency & resolution using this function:
  // ledcSetup(PWM_Ch, PWM_Freq, PWM_Res);
  ledcSetup(pwm_ch0, 30000, pwm_ch0_res);
  // PWM changing the duty cycle:
  // ledcWrite(pwm_ch0, pow(2, pwm_ch0_res) * fac);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init time and date
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //uncomment if want to set the time
  //set_time(01,59,16,02,30,06,22);

  //seting time (second,minute,hour,weekday,date_day,date_month,year)
  //set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  //  sec,min,h,day_w,day_m,month,ear
  #ifdef DEBUG
  Serial.println(F("debug statement"));
  #endif
  //initialize global time
  Helper::read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  #ifdef DEBUG
  Serial.print("Time: "); Serial.print(hour_);
  Serial.println(F("debug 0"));
  #endif

  delay(500);  //make TX pin Blink 2 times
  Serial.print(measure_intervall);
  delay(500);
  Serial.println(measure_intervall);
  
  system_sleep(); //turn off all external transistors
  delay(500);
  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  wakeModemSleep();
  connect_MQTT();
  #ifdef DEBUG
  Serial.print(F("Publish mqtt msg: "));
  String test_msg = String("Hello! ") + String(hour_) + String(":") + String(min_);
  Serial.println(msg_mqtt(testing, test_msg));
  Serial.print(hour_);
  delay(1000);
  #endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//init Irrigation Controller instance and update config
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef RasPi
// setup empty class instance
IrrigationController controller;
// update the config file stored in spiffs
// in order to work a RasPi with node-red and configured flow is needed
controller.updateController();
#endif
  //hour_ = 0;
  //disableWiFi();

    system_sleep(); //turn off all external transistors
  }

//######################################################################################################################
//----------------------------------------------------------------------------------------------------------------------
//---- MAIN PROGRAM LOOP -----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//######################################################################################################################

unsigned long startLoop = millis();
void loop(){
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// start loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
digitalWrite(sw_3_3v, HIGH); delay(10);
shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set shift reigster to value (0)
digitalWrite(st_cp_shft, HIGH); //write our to shift register output
delay(1);
digitalWrite(st_cp_shft, LOW);
delay(20);
#ifdef DEBUG
Serial.println(F("start loop setup"));
#endif

Wire.beginTransmission(DS3231_I2C_ADDRESS);
byte rtc_status = Wire.endTransmission();
//delay(200);
byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;

//check real time clock module,
// check for hour change and request config update from pi or use default settings
if(rtc_status != 0){
  #ifdef DEBUG
  Serial.println(F("Error: rtc device not available!"));
  #endif
  int i = 0;
  while(rtc_status != 0){
    //loop as long as the rtc module is unavailable!
    #ifdef DEBUG
    Serial.print(F(" . "));
    #endif
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    rtc_status = Wire.endTransmission();

    //reactivate 3.3v supply
    digitalWrite(sw_3_3v, HIGH); delay(10);
    shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set shift reigster to value (0)
    digitalWrite(st_cp_shft, HIGH); //write our to shift register output
    delay(1);
    digitalWrite(st_cp_shft, LOW);
    delay(1000);
    if(i > (int)10){
      break;
    }
    i++;
  }

  //store into variable available for all following functions
}
else{
  Serial.println(F("rtc found"));
}
if (!(bool)rtc_status)
{
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
}
  #ifdef DEBUG
  delay(1);
  Serial.print(F("Time hour: ")); Serial.print(hour1); Serial.print(F(" last ")); Serial.println(hour_);
  Serial.print("rtc status: "); Serial.println(rtc_status); Serial.println(!(bool) rtc_status);
  #endif

if((hour_ != hour1) & (!(bool)rtc_status)){
//if(true){
  // check for hour change
  up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);

  #ifdef RasPi
  // ask for config updates
  wakeModemSleep();
  delay(1);
  connect_MQTT();
  delay(1000);
  #ifdef DEBUG
  Serial.println(F("Sending status message to Raspi!")); Serial.print(F("Is connected: ")); Serial.println(client.connected());
  #endif
  msg_mqtt(config_status, String("2")); //signal message to rasPi
  Serial.print(F("Is connected: ")); Serial.println(client.connected());
  client.loop();
  unsigned long start = millis();
  while(1){
    delay(100);
    client.loop();
    if(millis() > start + 6500){
      msg_stop = true;
      #ifdef DEBUG
      Serial.println(F("Warning: No commands recieved from Raspi!"));
      #endif
      break;
    }
  }
  #endif

  //select chosen timetable
  if(sw2){
    timetable = timetable_raspi;
  }
  else{
    timetable = timetable_default;
  }

  //if(true){
  if(bitRead(timetable, hour1)){
    thirsty = true; //initialize watering phase

    #ifdef DEBUG
    Serial.print("bool status: sw1 "); Serial.print(sw1); Serial.print(" sw0 "); Serial.print(sw0);
    #endif
    // take default values
    //if(sw1){
    if(sw1 && sw0){
      //take recieved values
      #ifdef DEBUG
      Serial.println(F("consider sent watering configuration."));
      #endif
      int len = sizeof(group)/sizeof(group[0]);
      struct solenoid* ptr = group;
      for (int i=0; i<len; i++, ptr++){
        if(ptr->is_set){
          ptr->watering_time = ptr->watering_mqtt;
        }
      }
    }
    if(!sw1 && sw0){
      // take default values
      #ifdef DEBUG
      Serial.println(F("consider default watering configuration."));
      #endif
      int len = sizeof(group)/sizeof(group[0]);
      struct solenoid* ptr = group;
      for (int i=0; i<len; i++, ptr++){
        if(ptr->is_set){
          ptr->watering_time = ptr->watering_default;
        }
      }
    }

    if(!sw0){
      #ifdef DEBUG
      Serial.print(F("Set watering of all active groups to 0."));
      #endif
      int len = sizeof(group)/sizeof(group[0]);
      struct solenoid* ptr = group;
      for(int i=0; i<len; i++, ptr++){
        if(ptr->is_set){
          ptr->watering_time = 0;
        }
      }
    }

    #ifdef DEBUG
    if(sw0)
    {
      Serial.println(F("Watering ON"));
    }
    else{
      Serial.println(F("Watering OFF"));
    }
    if(1){
      Serial.println(F("Water configuration:"));
      int len = sizeof(group)/sizeof(group[0]);
      struct solenoid* ptr = group;
      for(int i=0; i<len; i++, ptr++){
        Serial.print(F("Group - ")); Serial.print(ptr->name); Serial.print(F(" - time - ")); Serial.println(ptr->watering_time);
      }
    }
    #endif
    
  }
}
#ifdef DEBUG
Serial.println(F("Config: ")); Serial.print(F("Bewae switch: ")); Serial.println(sw0);
Serial.print(F("Value override: ")); Serial.println(sw1);
Serial.print(F("timetable override: ")); Serial.println(sw2);
Serial.print(F("Timetable: ")); Serial.print(timetable_raspi); Serial.print(F(" value ")); Serial.println(timetable, BIN); 
#endif

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
if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall))
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
  digitalWrite(sw_sens2, HIGH);  //activate sensor rail
  delay(1000);
  int value=0; //mux reading value

  //measure all moisture sensors
  int len = sizeof(measure_point)/sizeof(measure_point[0]);
  struct sensors* m_ptr = measure_point;
  for (int i=0; i<len; i++, m_ptr++ ) {
    if(m_ptr->is_set){ //convert moisture reading to relative moisture and clip bad data with constrain
      controll_mux(m_ptr->pin, sig_mux_1, en_mux_1, "read", &value);
      if(m_ptr->group_vpin < max_groups){ //will be true if it is moisture measurement (max_group as dummy for all values not assigned to a group)
        float temp = (float)value * measurement_LSB5V * 1000;
        #ifdef DEBUG
        Serial.print(F("raw read:")); Serial.println(value);
        Serial.print(F("raw read int:")); Serial.println((int)temp);
        #endif
        value = temp;
        value = constrain(value, low_lim, high_lim); //x within borders else x = border value; (example 1221 wet; 3176 dry [in mV])
                                                      //avoid using other functions inside the brackets of constrain
        value = map(value, low_lim, high_lim, 100, 0);
        m_ptr->val = value;
      }
      else{
        m_ptr->val = (float)value * (float)measurement_LSB;
      }
    }
  }

  int data2[28] = {0}; //create data array
  controll_mux(11, sig_mux_1, en_mux_1, "read", &value); //take battery measurement before devices are switched on

  data2[4] = value;                                       //--> battery voltage (low load)
  delay(100); //give sensor time to stabilize voltage
  for(int i=0; i<16; i++){
  controll_mux(i, sig_mux_1, en_mux_1, "read", &value);
  delay(1); //was 3
  data2[6+i] = value;
  }
  digitalWrite(sw_3_3v, HIGH);
  delay(1);

  // BME280 sensor (default)
  #ifdef BME280
  if (!bme.begin(BME280_I2C_ADDRESS)) {
    #ifdef DEBUG
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    #endif
    data2[0] = 0; data2[1] = 0; data2[2] = 0;

    bme_point[1].val = 0; 
    bme_point[2].val = 0;
    bme_point[0].val = 0;
  }
  else{
    data2[0] = (float)bme.readPressure() / (float)10+0;     //--> press reading
    data2[1] = (float)bme.readHumidity() * (float)100+0;    //--> hum reading
    data2[2] = (float)bme.readTemperature() * (float)100+0; //--> temp reading

    bme_point[1].val = bme.readPressure(); 
    bme_point[2].val = bme.readHumidity();
    bme_point[0].val = bme.readTemperature();
  }
  #endif

  // DHT11 sensor (alternative)
  #ifdef DHT
  //possible dht solution --- CURRENTLY NOT IMPLEMENTED!
  #endif

  // log data to SD card (backup) ---  CURRENTLY NOT IMPLEMENTED!
  #ifdef SD_log
  /*
  // --- data shape---
  //data2[0] = (float)bme280.getPressure() / (float)10+0;     //--> press reading
  //data2[1] = (float)bme280.getHumidity() * (float)100+0;    //--> hum reading
  //data2[2] = (float)bme280.getTemperature() * (float)100+0; //--> temp reading
  //data2[3] = readVcc();                     //--> vcc placeholder (not active with esp right now)
  //data2[4] = battery_indicator;             //--> battery voltage (low load)
  //data2[5] = 0;                             //--> placeholder
  //data2 6-21                                //--> 15 Mux channels
  //data2 22-27                               //--> unused
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
  digitalWrite(sw_sens2, LOW);   //deactivate sensor rail
  digitalWrite(sw_3_3v, LOW);
  */
  #endif //sd log

  // log to INFLUXDB (default)
  #ifdef RasPi
  if(post_DATA){
    wakeModemSleep();
    delay(1);
    connect_MQTT();
    int len = sizeof(measure_point)/sizeof(measure_point[0]);
    struct sensors* data_ptr = measure_point;
    for (int i=0; i<len; i++, data_ptr++ ) {
      if(data_ptr->is_set){
        String topic = topic_prefix + data_ptr->name;
        String data = String(data_ptr->val);
        msg_mqtt(topic, data);
      }
    }
    //switch if using DHT
    #ifdef BME280
    len = sizeof(bme_point)/sizeof(bme_point[0]);
    struct sensors* data_ptr2 = bme_point;
    for (int i=0; i<len; i++, data_ptr2++) {
      if(data_ptr2->is_set){
        String topic = topic_prefix + data_ptr2->name;
        String data = String(data_ptr2->val);
        msg_mqtt(topic, data);
      }
    }
    #endif
  }
  #endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(thirsty){
  digitalWrite(sw_3_3v, HIGH); delay(10); //switch on shift register! and logic?
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set shift reigster to value (0)
  digitalWrite(st_cp_shft, HIGH); //write our to shift register output
  delay(1);
  digitalWrite(st_cp_shft, LOW);
  #ifdef DEBUG
  Serial.println(F("start watering phase"));
  #endif
  delay(30);

  // --- Watering ---
  //description: trigger at specific time
  //             alternate the solenoids to avoid heat damage, let cooldown time if only one remains
  //Hints:  main mosfet probably get warm
  //        pause procedure when measure events needs to happen
  //        NEVER INTERUPT WHILE WATERING!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while((loop_t + measure_intervall > millis()) & (thirsty)){
  // process will trigger multiple loop iterations until thirsty is set false
  // this should allow a regular measure intervall and give additional time to the water to slowly drip into the soil

    // load the stored file and get all keys
    DynamicJsonDocument doc(CONF_FILE_SIZE);
    if (!(Helper::load_conf(CONFIG_FILE_PATH, doc))){
      #ifdef DEBUG
      Serial.println(F("Error during watering procedure: Happened when loading config file!"));
      #endif
      break;
    }
    // Access the "groups" object
    JsonObject groups = doc["groups"];
    doc.clear();

    int numgroups = groups.size();
    IrrigationController Group[numgroups];
    int j = 0;
    // Iterate through all keys in the "groups" object and initialize the class instance
    // kv = key value pair
    for(JsonPair kv : groups){
      // check if group is presend and get its index
      int solenoid_index = 0;
      if(groups[kv.key()].is<int>()){
        solenoid_index = groups[kv.key()].as<int>();
        // access watering group via virtual pin stored as jsonkey (solenoid ?|pump pin)
        Group[j].loadScheduleConfig(CONFIG_FILE_PATH, solenoid_index);
      }
      else{
        // reset the group to take it out of process
        Group[j].reset();
      }
      // Initialize Group
      // Access the value of the current key-value pair
      j++;
    }

    // Iterate over all irrigation controller objects in the Group array
    // This process will trigger multiple loop iterations until thirsty is set false
    int status = 0;
    for(int i = 0; i < numgroups; i++){
      // ACTIVATE SOLENOID AND/OR PUMP
      // The method checks if the instance is ready for watering
      // if not it will return early, else it will use delay to wait untill the process has finished
      status += Group[i].waterOn(hour1);
    }
    // check if all groups are finished and reset status
    if(!status){
      thirsty = false;
    }
  }
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set shift reigster to value (0)
  delay(10);
  digitalWrite(sw_3_3v, LOW); //switch OFF logic gates (5V) and shift register
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sleep
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if (loop_t + measure_intervall > millis()){
  //sleep till next measure point
  int sleep_ = (long)(loop_t + measure_intervall - millis())/(TIME_TO_SLEEP * 1000UL);
  #ifdef DEBUG
  if(sleep_ < (int)measure_intervall / (TIME_TO_SLEEP * 1000)){
  Serial.println(F("Warning: sleep time calculation went wrong value too high!"));
  }
  Serial.print(F("sleep in s: ")); Serial.println((float)sleep_ * TIME_TO_SLEEP);
  #endif
  for(int i = 0; i < sleep_; i++){
    system_sleep(); //turn off all external transistors
    esp_light_sleep_start();
    if(loop_t + measure_intervall < millis()){
      break; //stop loop to start measuring
    }
  }
}
#ifdef DEBUG
Serial.println(F("End loop!"));
#endif
}