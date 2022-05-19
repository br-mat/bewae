/*
#include <headers.h>

using namespace Helper;
*/
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
#include <PubSubClient.h>

#include <config.h>
#include <helper.cpp>

using namespace std;

#define DEBUG
//#define ESP32

namespace Helper{
    String timestamp();
    void shiftvalue8b(uint8_t val);
    void system_sleep();
    void copy(int* src, int* dst, int len);
    void watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, 
        uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm);
    void controll_mux(uint8_t control_pins[], uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val);
    void save_datalog(String data, uint8_t cs, const char * file);
    void set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
    void read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year);
    void disableWiFi();
    bool enableWifi();
    void setModemSleep();
    void wakeModemSleep();
    bool find_element(int *array, int item);
};

using namespace Helper;

Adafruit_BME280 bme; // use I2C interface
/*
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();
*/

void setup() {
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // configure pin mode                                                                 ESP32 port?
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
  /*
  bme_temp->printSensorDetails();
  bme_pressure->printSensorDetails();
  bme_humidity->printSensorDetails();
  */

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
//delay(200);
byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;
read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
if(hour_ != hour1){
//if(true){
  up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
  //if(1){
  if(bitRead(timetable, hour1)){
  //if(find_element() & (hour1 == (byte)11) | (hour1 == (byte)19)){
    config = true; //only activate once per cycle
    thirsty = true;
    //TODO? request or derive the watering amount!
    copy(group_st_time, watering_base, 6); //fill watering placeholder array
  }
}
unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL; //time in ms, comming from rtc module
                                                                                                //give acurate values despide temperature changes


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
  int value=0; //mux reading value
  int data2[28] = {0}; //create data array
  controll_mux(mux, 11, sig_mux_1, en_mux_1, "read", &value); //take battery measurement bevore devices are switched on
  digitalWrite(sw_sens2, HIGH);   //activate sensor rail
  data2[4] = value;                                       //--> battery voltage (low load)
  delay(100); //give sensor time to stabilize voltage
  for(int i=0; i<sens_count; i++){
  controll_mux(mux, i, sig_mux_1, en_mux_1, "read", &value);
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
  digitalWrite(sw_sens, LOW);   //deactivate mux & SD
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
  delay(250); //give time
  save_datalog(data, chip_select, "datalog2.txt");
  #ifdef DEBUG
  Serial.print(F("data:"));
  Serial.println(data);
  #endif
  data=""; //clear string
  #endif //sd log

  // --- log to INFLUXDB ---
  #ifdef RasPi

  #endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// recieve commands
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// watering
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
if(thirsty){


}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// sleep
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}

/*
void loop() {

  #ifdef DEBUG
  Serial.println(F("start loop setup"));
  #endif

  // --- init phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //check if RTC answers on IIC bus
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

  #ifdef DEBUG
  Serial.println(F("enter init & check phase"));
  #endif

  //delay(200);
  byte sec1, min1, hour1, day_w1, day_m1, mon1 , y1;
  read_time(&sec1, &min1, &hour1, &day_w1, &day_m1, &mon1, &y1);
  if(hour_ != hour1){
  //if(true){
    up_time = up_time + (unsigned long)(60UL* 60UL * 1000UL); //refresh the up_time every hour, no need for extra function or lib to calculate the up time
    //if(1){
    if((hour1 == (byte)11) | (hour1 == (byte)19)){
      config = true; //only activate once per cycle
      thirsty = true;
      //TODO? request or derive the watering amount!
      copy(group_st_time, watering_base, 6); //fill watering placeholder array
    }
  }
  unsigned long actual_time = (unsigned long)up_time+(unsigned long)sec1*1000+(unsigned long)min1*60000UL; //time in ms, comming from rtc module
                                                                                                 //give acurate values despide temperature changes
  #ifdef DEBUG
  Serial.println(F("start loop debug 1"));
  #endif

  // --- datalog phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //saving data to SD, shape:
  //timestamp,pressure,temp,hum,analogdata(10 sensors+)
  // INFO:
  //Attention! RAM might get critical here. Backup string to the SD card and the Data int array itself cost a lot of memory!
  //           The SD lib need additional space to work.
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
    
    delay(100);
    #ifdef DEBUG
    Serial.print(F("Signaled esp to update data"));
    #endif
    
    delay(100);
    #ifdef DEBUG
    Serial.println(F("enter datalog phase"));
    #endif
    int value=0; //mux reading value
    int data2[28] = {0}; //all sensors via mux plus bme readings and additional space
    controll_mux(mux, 11, sig_mux_1, en_mux_1, "read", &value); //take battery measurement bevore devices are switched on
    data2[5] = 0;                                       //  (Supply voltage is not stressed)
    data2[4] = value;
    delay(1);
    digitalWrite(sw_sens, HIGH);   //deactivate mux & SD
    digitalWrite(sw_sens2, HIGH);   //deactivate sensor rail
    // --- prepare for datalog ---
    delay(100); //give sensors time to balance voltage

    // --- collect data ---
    //delay(5000); //give some time to esp01 on iic bus (remove after testing)
    //data2[0] = (float)bme280.getPressure() / (float)10+0;
    //data2[1] = (float)bme280.getHumidity() * (float)100+0;
    //data2[2] = (float)bme280.getTemperature() * (float)100+0;
    //data2[3] = readVcc();
    //data2[4] = battery_indicator; //measurement is done early without stressed supply voltage
    //data2[5] = 0;                 //vcc
    //data2[22]=0;
    //data2[23]=0;
    //data2[24]=0;
    //data2[25]=0;
    //data2[26]=0;
    //data2[27]=0;
    for(int i=0; i<sens_count; i++){
      controll_mux(mux, i, sig_mux_1, en_mux_1, "read", &value);
      delay(1); //was 3
      data2[6+i] = value;
    }
    //delay(500);
    digitalWrite(sw_sens, LOW);   //deactivate mux & SD
    digitalWrite(sw_sens2, LOW);   //deactivate sensor rail
    digitalWrite(sw_3_3v, HIGH);  //reactivate shift register and set it all to zero
    shiftvalue8b(0);

    // --- log data to SD card (backup) ---
    String data="";
    data += timestamp();
    data += ",";
    for(uint8_t i=0; i<28; i++){
      data += data2[i];
      data += ",";
    }
    delay(500); //give some time
    save_datalog(data,chip_select, "datalog2.txt");
    #ifdef DEBUG
    Serial.print(F("data:"));
    Serial.println(data);
    #endif
    data=""; //clear string
    // log to INFLUXDB ---

    delay(500); //give some time to establish connection
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implement: MQTT messaging
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    byte stat = 0; //ADD boolean check value
    if(stat != 0x00)
    {
      #ifdef DEBUG
      Serial.print(F("Transmission Error! Value: "));//transmissiion error
      Serial.println(stat);
      #endif
    }
    else
    {
      #ifdef DEBUG
      Serial.println(F("Transmission OK!"));
      #endif
    }
    delay(10);
  }
  
  // --- data recieving phase ---
  //IMPORTANT! this should only be used once in the begin off every watering circle. To avoid pumping out too much water.
  //
  //expecting i2c to send all values as int (2 bytes) with max 32 bytes due to i2c limitation
  //the bool values can be optimized if needed because 2 bytes for true or false is a big waste
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  if(config){
    if(!esp_busy){ //ask for updates from raspberrypi
          esp_busy = true;
          esp_time = actual_time;
          esp_status=(byte)0;
          delay(8000); //wait for esp to get the data
    }
    byte count = 0;
    while(esp_status == 0){
      //wait for esp to finish processing, it is important to wait for esp to finish iic messaging
      //  recieve event is triggered by interupt and can randomly halt and so freeze arduino from time to time
      //  this MUST NOT happen during the watering procedure
      
      //FINISH HANDLING OF ERROR CODES LATER
      //4 should probably shut down esp or set thirsty false to avoid conflict
      //probably skipp thisty for a while then?
      if(count>(byte)120){
        esp_status = (byte)4; //4 indicating an error, no answer from esp after about 1 min
      }
      count++;
      delay(1000);
    }
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
  
  // --- watering phase ---
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //thirsty = false;  //uncomment if not wanted
  if(thirsty){
    #ifdef DEBUG
    Serial.println(F("start watering phase"));
    #endif
    // --- Watering ---
    //trigger at specific time
    //alternate the solenoids to avoid heat damage, let cooldown time if only one remains
    //Hints: it can happen that solenoids work without delay for 60+60 sec should be no problem when it happens once
    //       main mosfet probably get warm (check that!)
    //       overall time should not go over 40 min while watering
    //       NEVER INTERUPT WHILE WATERING!!!!!!!!!!!!!!!!!!
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    uint8_t watering_done[6]= {1,1,1,1,1,1};
    #ifdef DEBUG
    Serial.println(F("watering while"));
    #endif
    unsigned long start_t = millis();
    //bool finished_watering = false; //using thirsty instead
    while((start_t+measure_intervall > millis()) & (thirsty)){
      //solenoids need time to cool down after 60 sek on time
      //controlling for loop to alternate the groups and work on the "todo watering"
      #ifdef DEBUG
      Serial.println(F("watering while inside"));
      #endif
      for(uint8_t i=0; i<6; i++){
        int water_timer=0;
        uint8_t group = groups[i];
        #ifdef DEBUG
        Serial.println(group);
        Serial.print(F("watering base: ")); Serial.println(watering_base[i]);
        #endif
        delay(500);
        if(watering_base[i]<1){
          water_timer=0;
          watering_done[i]=0;
          #ifdef DEBUG
          Serial.println(F("watering group finished"));
          #endif
        }
        else if(watering_base[i]<60){
          water_timer=watering_base[i];
          watering_base[i]=watering_base[i]-water_timer;
        }
        else{
          water_timer=60;
          watering_base[i]=watering_base[i]-60;
        }
        
        //watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm)
        if(water_timer > 60){
          //sanity check (should not be possible)
          water_timer = 0;
          #ifdef DEBUG
          Serial.println(F("Error: watering over 60 sec"));
          #endif
        }
        //water_timer = 0;
        if(water_timer != 0){
          #ifdef DEBUG
          Serial.println(F("start watering"));
          #endif
          watering(s0_mux_1, s2_mux_1, s1_mux_1, water_timer, group, pump1, sw_3_3v, vent_pwm);
          //Serial.print(F("cooldown time: ")); Serial.println(1000UL * water_timer / 2+10000);
          //delay(1000UL * water_timer / 2+10000); //cooldown pump (OLD)
          
          //While doing nothing put arduino to sleep when it waits to cooldown
          //sleep keep brown out detection (BOD) on in case of problems, so the system can reset
          //and end possibly harming situations
          unsigned long sleeptime = 1000UL * water_timer / 2+8000; //let it rest for half of active time + 8sec
          unsigned int sleep8 = sleeptime/8000;
          for(unsigned int i = 0; i<sleep8; i++){
            esp_sleep_enable_timer_wakeup(8000);
            esp_light_sleep_start();
          }
          #ifdef DEBUG
          Serial.print(F("sleeptime%8: ")); Serial.println(sleeptime%8);
          #endif
          delay(sleeptime%8); //delay rest time not devidable by 8000ms
        }
      }
      // check how many groups still need watering
      byte sum = 0;
      for(int i=0; i<6; i++){
        //sum it up, could be done in the other for loop too
        sum += watering_done[i];
      }
      if(sum == (byte)1){
        //trigger if only one group remains in the list
        //delay for 30 sec for cooldown on solenoid
        delay(30000UL);
      }
      if(sum == (byte)0){
        //break the loop
        thirsty=false;
      }
      #ifdef DEBUG
      Serial.print(F("Groups left: ")); Serial.println(sum); Serial.println(thirsty);
      #endif
    }
  }

  delay(500);
  // --- end back to sleep ---
  ///////////////////////////////////////////////////////////////////////////
  //update global time variable
  read_time(&sec_, &min_, &hour_, &day_w_, &day_m_, &mon_, &year_);
  #ifdef DEBUG
  Serial.print(F("Hour:")); Serial.println(hour_);
  #endif
  system_sleep();

  //if not busy with watering sleep for about 2 min (15 iterations)
  if(!thirsty){
    #ifdef DEBUG
    Serial.println(F("enter sleep phase"));
    delay(100); //remove when not debuging
    #endif
    for(int i = 0; i<15; i++){
      esp_sleep_enable_timer_wakeup(8000);
      esp_light_sleep_start();
    }
  }
}
*/


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  HELPER     functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// seting shiftregister to defined value (8bit)
void Helper::shiftvalue8b(uint8_t val){
  //Function description: shiftout 8 bit value, MSBFIRST
  //FUNCTION PARAMETER:
  //val         -- 8bit value writte out to shift register                             uint8_t
  //------------------------------------------------------------------------------------------------
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, val); //take byte type as value
  digitalWrite(st_cp_shft, HIGH); //update output of the register
  delayMicroseconds(100);
  digitalWrite(st_cp_shft, LOW);
}


void Helper::system_sleep(){
  //Function: deactivate the modules, prepare for sleep & setting mux to "lowpower standby" mode:
  digitalWrite(vent_pwm, LOW);     //pulls vent pwm pin low
  digitalWrite(sw_sens, LOW);  //deactivates sensors
  digitalWrite(sw_sens2, LOW);      //deactivates energy hungry devices
  digitalWrite(sw_3_3v, LOW);      //deactivates energy hungry devices

  digitalWrite(en_mux_1, HIGH);    //deactivates mux 1
  digitalWrite(s0_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s1_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s2_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s3_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)

  esp_light_sleep_start();
}


// Function to copy 'len' elements from 'src' to 'dst'
// code: https://forum.arduino.cc/index.php?topic=274173.0
void Helper::copy(int* src, int* dst, int len) {
  //Function description: copy strings
  //FUNCTION PARAMETER:
  //src         -- source array                                                       int
  //dst         -- destiny array                                                      int
  //len         -- length of array                                                    int
  //------------------------------------------------------------------------------------------------
  memcpy(dst, src, sizeof(src[0])*len);
}


void Helper::watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm){
  //Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  //FUNCTION PARAMETER:
  //datapin     -- pins to set the mux binaries [4 pins]; mux as example;             uint8_t
  //clock       -- selected channel; 0-15 as example;                                 uint8_t
  //latch       -- signal input signal output; free pin on arduino;                   uint8_t
  //time        -- time in seconds, max 60 seconds                                    uint8_t
  //vent_pin    -- virtual pin number with shift register                             uint8_t
  //pump_pin    -- virtual pin number with shift register                             uint8_t 
  //pwm         -- pwm pin for fast pwm                                               uint8_t
  //------------------------------------------------------------------------------------------------
  //initialize state
  digitalWrite(en, LOW);
  digitalWrite(clock, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(latch, LOW);
  digitalWrite(datapin, LOW);
  #ifdef DEBUG
  Serial.print(F("uint time:")); Serial.println(_time);
  #endif
  digitalWrite(en, HIGH);
  unsigned long time_s = (unsigned long)_time * 1000UL;
  if (time_s > 60000UL){
    time_s = 60000UL;
    #ifdef DEBUG
    Serial.println(F("time warning: time exceeds 60 sec"));
    #endif
  }

  // seting shiftregister to 0
  shiftOut(datapin, clock, MSBFIRST, 0); //take byte type as value
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);
  #ifdef DEBUG
  Serial.print(F("Watering group: "));
  Serial.println(vent_pin); Serial.print(F("time: ")); Serial.println(time_s);
  #endif
  // perform actual function
  byte value = (1 << vent_pin) + (1 << pump_pin);
  #ifdef DEBUG
  Serial.print(F("shiftout value: ")); Serial.println(value);
  #endif
  shiftOut(datapin, clock, MSBFIRST, value);
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);
  delay(100); //wait balance load after pump switches on
  //control this PWM pin by changing the duty cycle:
  // ledcWrite(PWM_Ch, DutyCycle);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 1);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.95);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.9);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.88);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.86);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.84);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.82);
  delay(time_s);

  // reset
  // seting shiftregister to 0
  shiftOut(datapin, clock, MSBFIRST, 0); //take byte type as value
  digitalWrite(latch, HIGH); //enables the output of the register
  delay(1);
  digitalWrite(latch, LOW);

  digitalWrite(clock, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(latch, LOW);
  digitalWrite(datapin, LOW);
  digitalWrite(en, LOW);
}


//controll mux function
void Helper::controll_mux(uint8_t control_pins[], uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val){
  //Function description: Controlls the mux, only switches for a short period of time for reading and sending short pulses
  //FUNCTION PARAMETER:
  //control_pins  -- pins to set the mux binaries [4 pins]; mux as example;            uint8_t array [4]
  //NOT IN USE channel_setup -- array to define the 16 different channels; mux_channel as example; uint8_t array [16][4]
  //channel       -- selected channel; 0-15 as example;                                 uint8_t
  //sipsop        -- signal input signal output; free pin on arduino;                   uint8_t
  //enable        -- enable a selected mux; free pin on arduino;                        uint8_t
  //mode          -- mode wanted to use; set_low, set_high, read;                       String
  //val           -- pointer to reading value; &value in function call;                 int (&pointer)   
  //------------------------------------------------------------------------------------------------
  
  uint8_t channel_setup[16][4]={
    {0,0,0,0}, //channel 0
    {1,0,0,0}, //channel 1
    {0,1,0,0}, //channel 2
    {1,1,0,0}, //channel 3
    {0,0,1,0}, //channel 4
    {1,0,1,0}, //channel 5
    {0,1,1,0}, //channel 6
    {1,1,1,0}, //channel 7
    {0,0,0,1}, //channel 8
    {1,0,0,1}, //channel 9
    {0,1,0,1}, //channel 10
    {1,1,0,1}, //channel 11
    {0,0,1,1}, //channel 12
    {1,0,1,1}, //channel 13
    {0,1,1,1}, //channel 14
    {1,1,1,1}  //channel 15
  };
  
  //make sure sig in/out of the mux is disabled
  digitalWrite(enable, HIGH);
  
  //selecting channel
  for(int i=0; i<4; i++){
    digitalWrite(control_pins[i], channel_setup[channel][i]);
  }
  //modes
  //"set_low" mode
  if(mode == String("set_low")){
    pinMode(sipsop, OUTPUT); //turning signal to output
    delay(1);
    digitalWrite(sipsop, LOW);
    digitalWrite(enable, LOW);
    delay(1);
    digitalWrite(enable, HIGH);
    pinMode(sipsop, INPUT); //seting back on input to not accidentally short the circuit somewhere
  }
  //"set_high" mode
  if(mode == String("set_high")){
    pinMode(sipsop, OUTPUT); //turning signal to output
    delay(1);
    digitalWrite(sipsop, HIGH);
    digitalWrite(enable, LOW);
    delay(1);
    digitalWrite(enable, HIGH);
    pinMode(sipsop, INPUT); //seting back to input to not accidentally short the circuit somewhere
  }
  //"read" mode
  if(mode == String("read")){
    double valsum=0;
    pinMode(sipsop, INPUT); //make sure its on input
    digitalWrite(enable, LOW);
    delay(2); //give time to stabilize reading
    *val=analogRead(sipsop); //throw away 
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    *val=analogRead(sipsop); //throw away
    for(int i=0; i<15; i++){
      valsum += analogRead(sipsop);
    }
    *val=(int)(valsum/15.0+0.5); //take measurement (mean value)
    delayMicroseconds(100);
    digitalWrite(enable, HIGH);
  }
}


void Helper::save_datalog(String data, uint8_t cs, const char * file){
  //Function: saves data given as string to a sd card via spi
  //FUNCTION PARAMETER
  //data       --      a string to save on SD card;    String
  //cs         --      Chip Select of SPI;             int
  //file       --      filename as string;             String
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Serial.begin(9600);
  pinMode(cs, OUTPUT);
  // see if the card is present and can be initialized: NEEDED ALTOUGH NOTHING IS DONE!!! DONT DELETE
  bool sd_check;
  byte iteration = 0;
  if (!SD.begin(cs)) {
    // don't do anything more: 
    //return;
    #ifdef DEBUG
    Serial.println(F("some problem occured: SD card not there"));
    #endif
    sd_check=false;
  }
  else{
    sd_check=true;
  }
  while(!sd_check){
    delay(5000);
    if(!SD.begin(cs)){
      //do again nothing
      #ifdef DEBUG
      Serial.print(F(" - again not found"));
      #endif
    }
    else{
      sd_check=true;
    }
    iteration = iteration + 1;
    if(iteration > 5){
      #ifdef DEBUG
      Serial.println(F(" - not trying again"));
      #endif
      sd_check=false;
    }
  }
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open(file, FILE_WRITE);
  // if the file is available, write to it:
  if (dataFile){
    dataFile.println(data);
    dataFile.close();
    // print to the serial port too:
    Serial.println(data);
  }   //ln 
  delay(1000);  //need time to save for some reason to work without mistakes
}
// Convert normal decimal numbers to binary coded decimal
byte dec_bcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcd_dec(byte val)
{
  return( (val/16*10) + (val%16) );
}


//took basically out of the librarys example as the rest of the time functions, slightly modded
void Helper::set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
//Function: sets the time on the rtc module (iic)
//FUNCTION PARAMETERS:
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(dec_bcd(second)); // set seconds
  Wire.write(dec_bcd(minute)); // set minutes
  Wire.write(dec_bcd(hour)); // set hours
  Wire.write(dec_bcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(dec_bcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(dec_bcd(month)); // set month
  Wire.write(dec_bcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}


void Helper::read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year)
{
//Function: read the time on the rtc module (iic)
//FUNCTION PARAMETERS:
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
//month      --                     month -- byte
//year       --   year as number 2 digits -- byte
//comment: took basically out of the librarys example as the rest of the time functions, slightly modded
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *second = bcd_dec(Wire.read() & 0x7f);
  *minute = bcd_dec(Wire.read());
  *hour = bcd_dec(Wire.read() & 0x3f);
  *dayOfWeek = bcd_dec(Wire.read());
  *dayOfMonth = bcd_dec(Wire.read());
  *month = bcd_dec(Wire.read());
  *year = bcd_dec(Wire.read());
}


String Helper::timestamp(){
//Function: give back a timestamp as string (iic)
//FUNCTION PARAMETERS:
// NONE
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  String time_data="";
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  read_time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
  &year);
  time_data += String(hour, DEC);
  time_data += String(F(","));
  time_data += String(minute, DEC);
  time_data += String(F(","));
  time_data += String(second, DEC);
  time_data += String(F(","));
  time_data += String(dayOfMonth, DEC);
  time_data += String(F(","));
  time_data += String(month, DEC);
  return time_data;
}

void Helper::disableWiFi(){
    WiFi.disconnect(true);  // Disconnect from the network
    delayMicroseconds(100);
    WiFi.mode(WIFI_OFF);    // Switch WiFi off
}


bool Helper::enableWifi(){
  WiFi.disconnect(false);  // Reconnect the network
  delayMicroseconds(100);
  WiFi.mode(WIFI_STA);    // Switch WiFi on

  Serial.println("START WIFI");
  WiFi.begin(ssid, wifi_password);

  int iterator = 0;
  while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.print(".");
      if(iterator > 30){
        return false;
      }
      iterator++;
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

void Helper::setModemSleep() {
    WiFi.setSleep(true);
    if (!setCpuFrequencyMhz(80)){
        Serial2.println("Not valid frequency!");
    }
    // Use this if 40Mhz is not supported
    // setCpuFrequencyMhz(80); //(40) also possible
}
 
void Helper::wakeModemSleep() {
    setCpuFrequencyMhz(240);
}

bool Helper::find_element(int *array, int item){
  int len = sizeof(array);
  for(int i = 0; i < len; i++){
      if(array[i] == item){
          return true;
      }
  }
  return false;
}