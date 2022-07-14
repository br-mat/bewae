/*

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// Monitor all sensor values, handle watering procedure and get/send data to RasPi                                      
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 
//change frequency to save power
//#define F_CPU (80 * 1000000U)

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

#define DEBUG 1

//#define ESP32

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

//stay global for access through more than one iteration of loop
//is_set, pin, pump_pin, name, watering default, watering base, watering time, last act,
solenoid group[max_groups] =
{ 
  {true, 0, pump1, "Tom1", 65, 0, 0 , 0}, //group1
  {true, 1, pump1, "Tom2", 47, 0, 0, 0}, //group2
  {true, 2, pump1, "Gewa", 10, 0, 0, 0}, //group3
  {true, 3, pump1, "Chil", 20, 0, 0, 0}, //group4
  {true, 6, pump1, "Krtr", 15, 0, 0, 0}, //group5
  {true, 7, pump1, "Erdb", 35, 0, 0, 0}, //group6
};


//maybe shift into function to save ram?
//is_set, pin, name, val (float keep memory in mind! esp32 can handle a lot tho)
sensors measure_point[16] =
{
  {true, 0, "Tom-RR", 0, 0},
  {true, 1, "Tom-GR", 0, 0},
  {true, 2, "Tom-JC", 0, 1},
  {true, 3, "Tom-RC", 0, 1},
  {true, 4, "Tom-FL", 0, 0},
  {true, 5, "Pep-Bl", 0, 2},
  {true, 6, "Pep-5f", 0, 2},
  {true, 7, "Krt-Ba", 0, 3},
  {true, 8, "HoB-1", 0, 4},
  {true, 9, "HoB-2", 0, 5},
  {false, 10, "test", 0, max_groups+1},
  {false, 11, "test", 0, max_groups+1},
  {false, 12, "test", 0, max_groups+1},
  {false, 13, "test", 0, max_groups+1},
  {false, 14, "test", 0, max_groups+1},
  {true, 15, "Bat-12", 0, max_groups+1},
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
//                                   2523211917151311 9 7 5 3 1
//                                    | | | | | | | | | | | | |
unsigned long int timetable = 0b00000000000100001000000100000000;
//                                     | | | | | | | | | | | | |
//                                    2422201816141210 8 6 4 2 0

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
    client.setCallback(callback);
    client.subscribe(watering_topic, 0); //(topic, qos) qos 0 fire and forget, qos 1 confirm at least once, qos 2 double confirmation reciever
    client.subscribe(bewae_sw, 0); //switch on/off default values for watering
    client.subscribe(watering_sw, 0); //switch on/off watering procedure
    client.subscribe(testing, 0);
    client.subscribe(comms, 0); //commands from Pi
    //client.setCallback(callback);
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
      group[i].watering_time = val.toInt(); //changing watering value of the valve

      #ifdef DEBUG
      Serial.print(F("MQTT val received:"));
      Serial.println(val.toInt());
      Serial.print(F("index: "));
      Serial.println(i);
      #endif
    }
  }
  if(String(bewae_sw) == topic){
    sw0 = (byte)msg.toInt();
    #ifdef DEBUG
    Serial.println(F("watering switched"));
    #endif
  }

  if(String(watering_sw) == topic){
    sw1 = (byte)msg.toInt();
    #ifdef DEBUG
    Serial.println(F("watering-values changed, transmitted values will be considered"));
    #endif
    //data_collected=true; //!!!!!!!!!!!!!!!!!!!! replace later last sent by raspi
  }

  // TODO implement command function consisting of two letters and a msg part
  //   this function should be able to set watering times (probably values too?)
  //   and turn on/off measureing watering etc.
  if(String(comms) == topic){
    int length = msg.length();
    String comm_pre = msg.substring(0, 2);
    String comm_suf = msg.substring(2, length);

    #ifdef DEBUG
    Serial.println(F("command recieved"));
    #endif
  }
}

byte msg_mqtt(String topic, String data){
  //This function take topic and data as String and publishes it via MQTT
  //it returns a byte value, where 0 means success and 1 is a failed attemt
  //try publishing
  if(client.publish(topic.c_str(), data.c_str())){
    #ifdef DEBUG
    Serial.println(F("Data sent!"));
    #endif
    return (byte)0;
  }
  else{
    //handle retry
    #ifdef DEBUG
    Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
    #endif
    client.connect(clientID, mqtt_username, mqtt_password);
    delay(1000); // This delay ensures that client.publish doesn't clash with the client.connect call
    if(!client.publish(topic.c_str(), data.c_str())){
      #ifdef DEBUG
      Serial.println(F("ERROR no data sent!"));
      #endif
      return (byte)1;
    }
    else{
      #ifdef DEBUG
      Serial.println(F("Data sent!"));
      #endif
      return (byte)0;
    }
  }
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
  pinMode(sw_sens2, OUTPUT);      //switch sensor rail                            GPIO36() 25?!
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
  delay(1);
  #ifdef DEBUG
  Serial.println("Hello!");
  #endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// initialize bme280 sensor
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  digitalWrite(sw_3_3v, HIGH); delay(5);
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //take byte type as value data_shft, sh_cp_shft, st_cp_shft
  digitalWrite(st_cp_shft, HIGH);
  delay(1);
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(sw_sens, HIGH);
  digitalWrite(sw_sens2, HIGH);
  //delay(60000UL);

  Serial.println(F("BME280 Sensor event test"));
  if (!bme.begin()) {
    #ifdef DEBUG
    Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    #endif
    int it = 0;
    while (1){
      delay(100);
      it++;
      #ifdef DEBUG
      Serial.println(F("."));
      #endif
      if(it > 100){
        break;
      }
    }
  }
  delay(1000);
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
  //set_time(01,45,19,02,17,06,22);

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
  delay(100);
  #endif

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
shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //take byte type as value data_shft, sh_cp_shft, st_cp_shft
digitalWrite(st_cp_shft, HIGH);
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
    shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //take byte type as value data_shft, sh_cp_shft, st_cp_shft
    digitalWrite(st_cp_shft, HIGH);
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
if (!rtc_status)
{
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
  #ifdef DEBUG
  Serial.print(F("Time hour: ")); Serial.print(hour1); Serial.print(F(" last ")); Serial.println(hour_);
  #endif
}

if((hour_ != hour1) & (!rtc_status)){
//if(true){
  up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  //if(true){
  if(bitRead(timetable, hour1)){
  //if(find_element() & (hour1 == (byte)11) | (hour1 == (byte)19)){ //OLD!
    #ifdef RasPi
    config = true; //request config update from pi
    #endif
    thirsty = true; //initialize watering phase

    // take default values
    //if(sw1){
    if(true){
      int len = sizeof(group)/sizeof(group[0]);
      struct solenoid* ptr = group;
      for (int i=0; i<len; i++, ptr++ ) {
        ptr->watering_time = ptr->watering_default;
      }
    }
    else{
    // take recieved values
      int len = sizeof(group)/sizeof(group[0]);
      struct solenoid* ptr = group;
      for (int i=0; i<len; i++, ptr++ ) {
        if(ptr->is_set){
          ptr->watering_time = ptr->watering_default;
        }
      }
    }
/*
    //TODO? request or derive the watering amount!
    connect_MQTT();
    //request instructions from Pi
    if((config) & (connect_MQTT())){ 
      if(!client.publish(config_status, String(stat_request).c_str())){
        #ifdef DEBUG
        Serial.print(F("Error: sending status value, retry"));
        Serial.println(String(stat_request).c_str());
        #endif
        delay(500);
        int i=0;
        while(!client.publish(config_status, String(stat_request).c_str())){
          if(i>5){
            #ifdef DEBUG
            Serial.print(F("Error: sending statusquestion value "));
            #endif
            //return_value = false;
            break;
          }
          #ifdef DEBUG
          Serial.println(F(" . "));
          #endif
          delay(2000);
          i++;
        }
      }
      else{
        #ifdef DEBUG
        Serial.println(F("Success status: request sent!"));
        #endif
      }
    }
    */
   /*
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
//if((unsigned long)(actual_time-last_activation) > (unsigned long)(measure_intervall)) //use ds3231 based time read for more stability
//if(true)
if(false)
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
  #ifdef BME280
  data2[0] = (float)bme.readPressure() / (float)10+0;     //--> press reading
  data2[1] = (float)bme.readHumidity() * (float)100+0;    //--> hum reading
  data2[2] = (float)bme.readTemperature() * (float)100+0; //--> temp reading
  #endif

  #ifdef DHT
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
  digitalWrite(sw_sens2, LOW);   //deactivate sensor rail
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
 /*



 
#ifdef RasPi
if (connect_MQTT())
{
  Serial.println(" "); Serial.println(F("Connection established!"));
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(thirsty){
  digitalWrite(sw_3_3v, HIGH); delay(10); //switch on shift register!
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, 0); //set register to zero
  digitalWrite(st_cp_shft, HIGH);
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
  //        NEVER INTERUPT WHILE WATERING!!!!!!!!!!!!!!!!!!
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
    }
    if((unsigned long)pump_t > 1UL + pump_cooldown_sec/1000UL){
      pump_t = 0;
      //sleep while cooldown
      #ifdef DEBUG
      Serial.print(F("Sleeping: ")); Serial.println((unsigned long) (pump_cooldown_sec / TIME_TO_SLEEP * 1000));
      delay(10);
      #endif
      for(unsigned long i = 0; i < 2UL + (unsigned long) (pump_cooldown_sec / (unsigned long) TIME_TO_SLEEP * 1000UL); i++){
        esp_light_sleep_start();
      }
    }
    else{
    //send esp to sleep
    esp_light_sleep_start();
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
  unsigned long sleep = loop_t + measure_intervall - millis();
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

*/