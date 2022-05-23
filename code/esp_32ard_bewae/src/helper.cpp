
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

#include <Helper.h>

//#include "PubSubClient.h" // wont work with platformio use esp idf

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
