////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// Monitor all sensor values, handle watering procedure and get/send data to RasPi                                      
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

//Standard
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <Helper.h>
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
//is_set, pin, pump_pin, name, watering default, watering base, watering time, last act,
solenoid group[max_groups] =
{ 
  {true, 0, pump1, "Tom1", 100, 0, 0 , 0}, //group1
  {true, 1, pump1, "Tom2", 80, 0, 0, 0}, //group2
  {true, 2, pump1, "Gewa", 30, 0, 0, 0}, //group3
  {true, 3, pump1, "Chil", 40, 0, 0, 0}, //group4
  {true, 6, pump1, "Krtr", 20, 0, 0, 0}, //group5
  {true, 7, pump1, "Erdb", 50, 0, 0, 0}, //group6
};

//is_set, pin, name, val, group
sensors measure_point[16] =
{
  {false, 0, "Tom-RR", 0.0, 0},
  {false, 1, "Tom-GR", 0.0, 0},
  {false, 2, "Tom-JC", 0.0, 1},
  {false, 3, "Tom-RC", 0.0, 1},
  {false, 4, "Tom-FL", 0.0, 0},
  {false, 5, "Pep-Bl", 0.0, 2},
  {false, 6, "Pep-5f", 0.0, 2},
  {false, 7, "Krt-Ba", 0.0, 3},
  {false, 8, "HoB-1", 0.0, 4},
  {false, 9, "HoB-2", 0.0, 5},
  {false, 10, "test10", 0.0, max_groups+1},
  {false, 11, "test11", 0.0, max_groups+1},
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
bool config_bewae_sw = true; //switch watering on off
bool config_watering_sw = true; //switch default and custom values, default in group_st_time and custom sent via mqtt

//wireless config array to switch on/off functions
// watering time of specific group; binary values;
const int raspi_config_size = max_groups+2; //6 groups + 2 binary
int raspi_config[raspi_config_size]={0};

bool post_DATA = true; //controlls if data is sent back to pi or not
bool msg_stop = false; //is config finished or not
bool sw0 = 1; //bewae switch (ON/OFF)
bool sw1 = 0; //water time override condition
bool sw2 = 0; //timetable override condition

//timetable storing watering hours
//                                           2523211917151311 9 7 5 3 1
//                                            | | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                             | | | | | | | | | | | | |
//                                            2422201816141210 8 6 4 2 0
unsigned long int timetable = timetable_default; //initialize on default
unsigned long int timetable_raspi = timetable_default; //initialize on default

Adafruit_BME280 bme; // use I2C interface

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wifi & Pubsubclient                                           
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Initialise the WiFi and MQTT Client objects
WiFiClient wificlient;
PubSubClient client(wificlient);

void callback(char *topic, byte *payload, unsigned int msg_length);
bool connect_MQTT();
bool msg_mqtt();

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
    Serial.println(F("custom timetable active"));
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
  if(String(comms) == topic){
    int command = (int) msg.toInt(); //toInt returns long! naming is confusing
    switch (command)
    {
    case 0: //stop case indicates end off transmission
      msg_stop = true;
      break;
    
    default:
      break;
    }

    #ifdef DEBUG
    Serial.println(F("command recieved"));
    #endif
  }
}


// Custom function to connet to the MQTT broker via WiFi
bool connect_MQTT(){
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.mode(WIFI_STA);
    delay(1);
    #ifdef DEBUG
    Serial.print("Connecting to ");
    Serial.println(ssid);
    #endif
    // Connect to the WiFi
    WiFi.begin(ssid, wifi_password);
  }
  
  int tries = 0;
  // Wait until the connection has been confirmed before continuing
  while ((WiFi.status() != WL_CONNECTED)) {
    delay(500);
    #ifdef DEBUG
    Serial.print(" . ");
    #endif
    if(tries == 30) {
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
  bool connection = client.connect(clientID, mqtt_username, mqtt_password);
  int mqtt_i = 0;
  #ifdef DEBUG
  Serial.println(F("Connect to MQTT"));
  #endif
  while(!connection){
    delay(500);
    connection = client.connect(clientID, mqtt_username, mqtt_password);
    #ifdef DEBUG
    Serial.print(F(" . "));
    #endif
    if(mqtt_i>20){
      break;
    }
  }
  delay(500);
  if (connection) {
    #ifdef DEBUG
    Serial.println("Connected to MQTT Broker!");
    #endif
    //client.setCallback(callback);
    client.subscribe(watering_topic, 0); //(topic, qos) qos 0 fire and forget, qos 1 confirm at least once, qos 2 double confirmation reciever
    client.subscribe(bewae_sw, 0); //switch on/off default values for watering
    client.subscribe(watering_sw, 0); //switch on/off watering procedure
    client.subscribe(testing, 0);
    client.subscribe(comms, 0); //commands from Pi
    client.setCallback(callback);
    client.loop();
    return true;
  }
  else {
    #ifdef DEBUG
    Serial.print("Connection to MQTT Broker failed: ");
    Serial.println(connection);
    #endif
    return false;
  }
}


bool msg_mqtt(String topic, String data){
  //This function take topic and data as String and publishes it via MQTT
  //it returns a bool value, where 0 means success and 1 is a failed attemt

  //input: String topic, String data
  //return: false if everything ok otherwise true

  //unsigned int length = data.length(); //NEW IMPLEMENTATION SHOULD CORRECT BUG
  client.loop();
  #ifdef DEBUG
  Serial.println(F("msg_mqtt func called"));
  #endif  
  if(client.publish(topic.c_str(), data.c_str())){
    #ifdef DEBUG
    Serial.print(F("Data sent: ")); Serial.println(data.c_str()); Serial.println(topic.c_str());
    #endif
    return false;
  }
  else{
    //handle retry
    #ifdef DEBUG
    Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
    #endif
    client.connect(clientID, mqtt_username, mqtt_password);
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! REDOO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // FALLS ES RECONNECT GIBT DIESE NUTZEN ANSONSTEN anders lösen
    //reconnect macht probleme, weiß aber nicht warum überhaupt notwendig, da eigentlich verbindung stehen sollte.
    client.subscribe(watering_topic, 0); //(topic, qos) qos 0 fire and forget, qos 1 confirm at least once, qos 2 double confirmation reciever
    client.subscribe(bewae_sw, 0); //switch on/off default values for watering
    client.subscribe(watering_sw, 0); //switch on/off watering procedure
    client.subscribe(testing, 0);
    client.subscribe(comms, 0); //commands from Pi
    client.setCallback(callback);
    client.loop();
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! REDOO REDOO REDOO REDOO REDOO REDOO REDOO REDOO REDOO !!!!!!!!!!!!!!!!!!!!!!!!!
    delay(1000); // This delay ensures that client.publish doesn't clash with the client.connect call
    if(!client.publish(topic.c_str(), data.c_str())){
      #ifdef DEBUG
      Serial.println(F("ERROR no data sent!"));
      #endif
      return true;
    }
    else{
      #ifdef DEBUG
      Serial.println(F("Data sent!"));
      #endif
      return false;
    }
  }
}

bool pub_data(struct sensors* output_data, unsigned int length){
  //This function should take an array and publish the content via mqtt
  //input: a pointer array of sensors struct, its unsigned int length
  //return: false on success, true if any problems occured

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
  delay(1000);
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

  delay(500);  //make TX pin Blink 2 times, visualize end of setup
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
  delay(100);
  #ifdef DEBUG
  Serial.println(F("Sending status message to Raspi!"));
  #endif
  msg_mqtt(config_status, String("2")); //signal message to rasPi
  int iter = 0;
  client.loop();
  delay(1);
  client.loop();
  while(true){
    delay(1);
    client.loop();
    Serial.print("1");
    if(iter > 5000){
      msg_stop = true;
      #ifdef DEBUG
      Serial.println(F("Warning: No commands recieved from Raspi!"));
      #endif
      break;
    }
    iter++;
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
      if(ptr->is_set){
        for (int i=0; i<len; i++, ptr++ ) {
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
      for (int i=0; i<len; i++, ptr++ ) {
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
Serial.print(F("Timetable: ")); Serial.print(timetable_raspi); Serial.print(F(" value ")); Serial.println(timetable); 
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
        value = constrain(value, low_lim, high_lim); //x within borders else x = border value; (1221 wet; 3176 dry)
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
  controll_mux(11, sig_mux_1, en_mux_1, "read", &value); //take battery measurement bevore devices are switched on

  data2[4] = value;                                       //--> battery voltage (low load)
  delay(100); //give sensor time to stabilize voltage
  for(int i=0; i<16; i++){
  controll_mux(i, sig_mux_1, en_mux_1, "read", &value);
  delay(1); //was 3
  data2[6+i] = value;
  }
  digitalWrite(sw_3_3v, HIGH);
  delay(1);
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

  #ifdef DHT
  //possible dht solution?
  #endif

  // --- log data to SD card (backup) ---  
  #ifdef SD_log
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
  #endif //sd log

  // --- log to INFLUXDB ---
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
// recieve commands
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//currently done on every change of hour, which should save some power

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(thirsty){
  digitalWrite(sw_3_3v, HIGH); delay(10); //switch on shift register!
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set shift reigster to value (0)
  digitalWrite(st_cp_shft, HIGH); //write our to shift register output
  delay(1);
  digitalWrite(st_cp_shft, LOW);
  #ifdef DEBUG
  Serial.println(F("start watering phase"));
  #endif
  delay(30);
  // --- Watering ---
  //trigger at specific time
  //alternate the solenoids to avoid heat damage, let cooldown time if only one remains
  //Hints:  main mosfet probably get warm (check that!)
  //        pause procedure when measure events needs to happen
  //        NEVER INTERUPT WHILE WATERING!
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  while((loop_t + measure_intervall > millis()) & (thirsty)){
    int len = sizeof(group)/sizeof(group[0]);
    int finish = 0; //finished groups
    int set = 0; //groups set
    unsigned int pump_t = 0;
    struct solenoid* ptr = group;

    for (int i=0; i<len; i++, ptr++) {
      Serial.print("Temp statment watertime: "); Serial.println(ptr->watering_time);
      if(ptr->is_set){
        set+=1;
      }
      if(ptr->watering_time == 0){
        finish+=1;
        Serial.println(ptr->watering_time == 0);
      }
      delay(10);
      if((ptr->last_t + cooldown < millis()) & (ptr->watering_time > 0) & (ptr->is_set)) //minimum cooldown of 30 sec
      {
        unsigned int water_timer = 0;
        //set ptr variable to 0 when finished
        if (ptr->watering_time < max_active_time_sec)
        {
          water_timer = ptr->watering_time;
          ptr->watering_time = 0;
        }
        else{ //reduce ptr variable
          water_timer = (unsigned int) max_active_time_sec;
          ptr->watering_time -= (unsigned int) max_active_time_sec;
        }
        if(water_timer > (unsigned int) max_active_time_sec){ //sanity check
          #ifdef DEBUG
          Serial.println(F("Warning watering timer over 60!"));
          #endif
          water_timer = (unsigned int) max_active_time_sec;
        }
        #ifdef DEBUG
        Serial.print(F("Watering group: ")); Serial.print(ptr->name); Serial.println(F(";  "));
        Serial.print(water_timer); Serial.print(F(" seconds")); Serial.println(F(";  "));
        #endif
        pump_t += water_timer;
        watering(data_shft, sh_cp_shft, st_cp_shft, water_timer, ptr->pin, ptr->pump_pin, sw_3_3v, vent_pwm);
        ptr->last_t = millis();
        #ifdef DEBUG
        Serial.println(F("Done!"));
        delay(10);
        #endif
      }

      #ifdef DEBUG
      Serial.println(F("----------------------"));
      #endif

      if(pump_t > (unsigned int) max_active_time_sec){
        pump_t = 0;
        //sleep while cooldown
        #ifdef DEBUG
        Serial.print(F("cooling down: ")); Serial.println(2UL + (unsigned long) (pump_cooldown_sec / ((unsigned long) TIME_TO_SLEEP * 1000UL)));
        delay(100);
        #endif
        for(unsigned long i = 0; i < 2UL + (unsigned long) (pump_cooldown_sec / ((unsigned long) TIME_TO_SLEEP * 1000UL)); i++){
        //for(int i=0; i > 15; i++){
          esp_light_sleep_start();
        }
      }
      else{
      //send esp to sleep
      esp_light_sleep_start();
      }
    }

    if(finish == set){
      Serial.print("Temp stat fin and set: "); Serial.print(finish); Serial.print(" "); Serial.println(set);
      thirsty = false;
      break;
    }
    #ifdef DEBUG
    Serial.println(F("++++++++++++++++++++++++++++"));
    #endif
  }
  digitalWrite(sw_3_3v, LOW); //switch OFF logic gates (5V) and shift register
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sleep
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if (loop_t + measure_intervall > millis()){
  //sleep till next measure intervall + 2 times sleep time
  unsigned long sleep = loop_t + measure_intervall - millis() + TIME_TO_SLEEP * 1000UL * 2UL; 
  #ifdef DEBUG
  Serial.print(F("sleep in s: ")); Serial.println(sleep / 1000);
  #endif
  delay(10);
  for(unsigned long i = 0; i < (unsigned long) (sleep / TIME_TO_SLEEP * 1000); i++){
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