////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
//
// This file contains a collection of helper functions for the irrigation system. It includes functions for
// generating timestamps, controlling solenoids and pumps, reading and writing to config files, and interacting
// with various hardware components.
//
// Dependencies:
// - Arduino.h
// - Wire.h
// - SPI.h
// - SD.h
// - Adafruit_Sensor.h
// - Adafruit_BME280.h
// - WiFi.h
// - Helper.h
//
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

#include <Helper.h>

#define SD_MOSI      13
#define SD_MISO      5
#define SD_SCK       14

SPIClass spiSD(HSPI);
//spiSD.begin(SD_SCK, SD_MISO, SD_MOSI);

// Create an instance of the InfluxDBClient class with the specified URL, database name and token
InfluxDBClient influx_client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_DB_NAME, INFLUXDB_TOKEN);

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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper::system_sleep(){
  //Function: deactivate the modules, prepare for sleep & setting mux to "lowpower standby" mode:
  digitalWrite(vent_pwm, LOW);     //pulls vent pwm pin low
  digitalWrite(sw_sens, LOW);  //deactivates sensors
  digitalWrite(sw_sens2, LOW);      //deactivates energy hungry devices
  digitalWrite(sw_3_3v, LOW);      //deactivates energy hungry devices
  delay(1);
  digitalWrite(en_mux_1, HIGH);    //deactivates mux 1 on HIGH
  digitalWrite(s0_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s1_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s2_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(s3_mux_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)


  disableWiFi();
  //esp_light_sleep_start();
  Serial.println(F("Output on standby"));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper::copy(int* src, int* dst, int len) {
  //Function description: copy strings
  //FUNCTION PARAMETER:
  //src         -- source array                                                       int
  //dst         -- destiny array                                                      int
  //len         -- length of array                                                    int
  //------------------------------------------------------------------------------------------------
  memcpy(dst, src, sizeof(src[0])*len);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
// OLD FUNCTION NOW HANDLED IN IRRIGATIONCONTROLLER CLASS
void Helper::watering(uint8_t datapin, uint8_t clock, uint8_t latch, uint8_t _time, uint8_t vent_pin, uint8_t pump_pin, uint8_t en, uint8_t pwm){
  //Function description: Controlls the watering procedure on valves and pump
  // Careful with interupts! To avoid unwanted flooding.
  //FUNCTION PARAMETER:
  //datapin     -- serial data pin seting bit of shift register                       uint8_t
  //clock       -- shiftout clock pin                                                 uint8_t
  //latch       -- shift register set output pin                                      uint8_t
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
  if (time_s > (unsigned long)max_active_time_sec * 1000UL){
    time_s = (unsigned long)max_active_time_sec * 1000UL;
    #ifdef DEBUG
    Serial.println(F("time warning: time exceeds sec"));
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
  delay(2);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.96);
  delay(2);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.92);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.9);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.88);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.87);
  delay(1);
  ledcWrite(pwm_ch0, pow(2.0, pwm_ch0_res) * 0.86);
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
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper::controll_mux(uint8_t channel, uint8_t sipsop, uint8_t enable, String mode, int *val){
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
  int control_pins[4] = {s0_mux_1,s1_mux_1,s2_mux_1,s3_mux_1};
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

/*
bool Helper::save_datalog(String data, uint8_t cs, const char * file){
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
  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(cs, spiSD, 1000000U)) {
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
    if(!SD.begin(cs, spiSD)){
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
      return false;
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
  return true;
}
*/

// Convert normal decimal numbers to binary coded decimal
byte  Helper::dec_bcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
byte  Helper::bcd_dec(byte val)
{
  return( (val/16*10) + (val%16) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Attempts to enable the WiFi and connect to a specified network.
// Returns true if the connection was successful, false if not.
bool Helper::connectWifi(){
  if (WiFi.status() != WL_CONNECTED) {
    // Disconnect from any current WiFi connection and set the WiFi mode to station mode.
    WiFi.disconnect(true);  
    delayMicroseconds(100);
    WiFi.mode(WIFI_STA);    

    // Begin the process of connecting to the specified WiFi network.
    #ifdef DEBUG
    Serial.println(F("Connecting:"));
    #endif
    
    WiFi.begin(ssid, wifi_password);
  }

  // Initialize a counter to keep track of the number of connection attempts.
  int tries = 0;

  // Loop until the WiFi connection is established or the maximum number of attempts is reached.
  while (WiFi.status() != WL_CONNECTED) {
    // Wait 1 second before trying again.
    delay(1000);
    //WiFi.begin(ssid, wifi_password);
    #ifdef DEBUG
    Serial.print(F(" ."));
    #endif
    

    // Increment the counter.
    tries++;

    // If 30 attempts have been made, exit the loop and return false.
    if(tries > 30){
      #ifdef DEBUG
      Serial.println();
      Serial.print(F("Error: Wifi connection could not be established! "));
      Serial.println(tries);
      #endif
      return false;
    }
  }

  // If the loop exits normally, the WiFi connection was successful. Print the IP address of the device and return true.
  #ifdef DEBUG
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper::setModemSleep() {
  WiFi.setSleep(true);
  if (!setCpuFrequencyMhz(80)){
      Serial.println("Not valid frequency!");
  }
  // Use this if 40Mhz is not supported
  // setCpuFrequencyMhz(80); //(40) also possible
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disables the WiFi on the device.
// Returns true if the WiFi was successfully disabled, false if an error occurred.
bool Helper::disableWiFi(){
  // Disconnect from the WiFi network.
  WiFi.disconnect(true);  
  // Set the WiFi mode to off.
  WiFi.mode(WIFI_OFF);    
  #ifdef DEBUG
  Serial.print(F("Wifi status: ")); Serial.println(WiFi.status());
  #endif
  return true;
  
  /*
  // Check the WiFi status to make sure that it was successfully disabled.
  if (WiFi.status() == WL_DISCONNECTED) {
    Serial.println("");
    Serial.println("WiFi disconnected!");
    // Set the CPU frequency to 80 MHz.
    if (!setCpuFrequencyMhz(80)){
      Serial.println("Error: Not valid frequency!");
      return false;
    }
    return true;
  } else {
    Serial.println("Error: WiFi could not be disabled!");
    return false;
  }
  */
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper::disableBluetooth(){
  // Quite unusefully, no relevable power consumption
  btStop();
  #ifdef DEBUG
  Serial.println("");
  Serial.println("Bluetooth stop!");
  #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper::wakeModemSleep() {
  #ifdef DEBUG
  Serial.println(F("Waking up modem!"));
  #endif
  setCpuFrequencyMhz(240);
  Helper::connectWifi();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Helper::find_element(int *array, int item){
  int len = sizeof(array);
  for(int i = 0; i < len; i++){
      if(array[i] == item){
          return true;
      }
  }
  return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads the JSON file at the specified file path and returns the data as a DynamicJsonDocument.
// If the file path is invalid or if there is an error reading or parsing the file, an empty DynamicJsonDocument is returned.
DynamicJsonDocument Helper::readConfigFile(const char path[PATH_LENGTH]) {
  DynamicJsonDocument jsonDoc(CONF_FILE_SIZE); // create JSON doc, if an error occurs it will return an empty jsonDoc
                                     // which can be checked using jsonDoc.isNull()

  if (path == nullptr) { // check for valid path
    #ifdef DEBUG
    Serial.println("Invalid file path");
    #endif
    return jsonDoc;
  }

  File configFile = SPIFFS.open(path, "r"); // open the config file for reading
  if (!configFile) { // check if file was opened successfully
    #ifdef DEBUG
    Serial.println("Failed to open config file for reading");
    #endif
    return jsonDoc;
  }

  // Load the JSON data from the config file
  DeserializationError error = deserializeJson(jsonDoc, configFile);
  configFile.close();
  if (error) { // check for error in parsing JSON data
    #ifdef DEBUG
    Serial.println("Failed to parse config file");
    #endif
    jsonDoc.clear();
    return jsonDoc;
  }
  
  return jsonDoc;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Writes the specified DynamicJsonDocument to the file at the specified file path as a JSON file.
// Returns true if the file was written successfully, false if the file path is invalid or if there is an error writing the file.
bool Helper::writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]) {
  // TODO improve error handling on file path!
  if (path == nullptr) { // check for valid path, a function could possibly use this and set a nullptr as path
    #ifdef DEBUG
    Serial.println("Invalid file path");
    #endif
    return false;
  }

  File newFile = SPIFFS.open(path, "w"); // open the config file for writing
  if (!newFile) { // check if file was opened successfully
    #ifdef DEBUG
    Serial.println("Failed to open config file for writing");
    #endif
    newFile.close();
    return false; // error occured
  }

  // Write the JSON data to the config file
  serializeJson(jsonDoc, newFile);
  newFile.close();
  return true; // all good
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// send data to influxdb, return true when everything is ok
bool Helper::pubInfluxData(String sensor_name, String field_name, float value) {

  bool cond = Helper::connectWifi();
  if (!cond) {
    // if no connection is possible exit early
    #ifdef DEBUG
    Serial.print(F("Error in Wifi connection."));
    #endif
    return false;
  }

  Point point(sensor_name);
  point.addField(field_name, value);  // Add temperature field to the Point object

  // Write the Point object to InfluxDB
  if (!influx_client.writePoint(point)) {
    #ifdef DEBUG
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(influx_client.getLastErrorMessage());  // Print error message if write operation is unsuccessful
    #endif
    return false;
  }
  else{
    #ifdef DEBUG
    Serial.println(F("InfluxDB points sent"));
    #endif
    return true;
  }

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// blink onboard LED
void Helper::blinkOnBoard(String howLong, int times) {
   int duration;
  
   if (howLong == "long") {
     duration = 1000;
   } else if (howLong == "short") {
     duration = 250;
   } else {
     return;
   }
  
   for (int i = 0; i < times; i++) {
     digitalWrite(2, HIGH);
     delay(duration);
     digitalWrite(2, LOW);
     delay(1500-duration); //1.5 seconds between blinks
   }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// loads JSON object from file using its key
JsonObject Helper::getJsonObjects(const char* key, const char* filepath) {
  // load the stored file and get all keys
  DynamicJsonDocument doc(CONF_FILE_SIZE);
  doc = Helper::readConfigFile(filepath);

  JsonObject jsonobj;

  // Check if doc is empty
  if (doc.isNull()) {
    Serial.println(F("Warning: Failed to read file or empty JSON object."));
    return jsonobj;
  }
  
  // Access the key object and close file
  jsonobj = doc[key];
  delay(1);
  doc.clear();

  #ifdef DEBUG
  String jsonString;
  serializeJson(jsonobj, jsonString);
  Serial.println(F("JsonObj:"));
  Serial.println(jsonString); Serial.println();
  #endif
  
  // Check if key exists in the JSON object
  if (jsonobj.isNull()) {
    Serial.println(F("Warning: Key not found in JSON object."));
    return jsonobj;
  }
  
  return jsonobj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

DynamicJsonDocument Helper::getJSONData(const char* server, int serverPort, const char* serverPath) {
  // HTTP GET request to the Raspberry Pi server
  DynamicJsonDocument JSONdata(CONF_FILE_SIZE);
  HTTPClient http;
  http.begin(String("http://") + server + ":" + serverPort + serverPath);
  int httpCode = http.GET();

  // Check the status code
  if (httpCode == HTTP_CODE_OK) {
    // Parse the JSON data
    DeserializationError error = deserializeJson(JSONdata, http.getString());

    if (error) {
      #ifdef DEBUG
      Serial.println(F("Error parsing JSON data"));
      #endif
      return JSONdata;
    } else {
      return JSONdata;
    }
  } else {
    #ifdef DEBUG
    Serial.println(F("Error sending request to server"));
    #endif
    return JSONdata;
  }

  http.end();
  return JSONdata;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Helper::updateConfig(const char* path){
  // call this function once per hour it uses timetable to determine if it should set water_time variable with content either
  // watering_default or watering_mqtt depending on a modus variable past with the function (this can be an integer about 0-5)
  // water time variable holds information about the water cycle for the active hour if its 0 theres nothing to do
  // if its higher then it should be recognized by the readyToWater function, this value

  if (WiFi.status() == WL_CONNECTED) {
    DynamicJsonDocument newdoc = Helper::getJSONData(SERVER, SERVER_PORT, path);
    if(newdoc.isNull()){
      #ifdef DEBUG
      Serial.println(F("Warning: Wifi is disabled. Not able to get file from server!"));
      #endif
      return false;
    }

    return Helper::writeConfigFile(newdoc, CONFIG_FILE_PATH); // write the updated JSON data to the config file
  }
  else{
    #ifdef DEBUG
    Serial.println(F("Error no Wifi connection established"));
    #endif
    return false;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////