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
  digitalWrite(st_cp_shft, LOW);
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, val); //take byte type as value
  digitalWrite(st_cp_shft, HIGH); //update output of the register
  delayMicroseconds(100);
  digitalWrite(st_cp_shft, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// set shiftregister to defined value (32 bit)
void Helper::shiftvalue(uint32_t val, uint8_t numBits) {
  // Function description: shift out specified number of bits from a value, MSBFIRST
  // FUNCTION PARAMETERS:
  // val       -- value to be shifted out                      uint32_t
  // numBits   -- number of bits to be shifted out              uint8_t
  // ------------------------------------------------------------------------------------------------

  // Split the long value into two bytes
  byte highByte = (val >> 8) & 0xFF;
  byte lowByte = val & 0xFF;

  // Shift out the high byte first
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, highByte);

  // Then shift out the low byte
  shiftOut(data_shft, sh_cp_shft, MSBFIRST, lowByte);

  // Pulse the latch pin to activate the outputs
  digitalWrite(st_cp_shft, LOW);
  digitalWrite(st_cp_shft, HIGH);
  digitalWrite(st_cp_shft, LOW);
}


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
  #ifdef DEBUG
  Serial.println(F("Output on standby"));
  #endif
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Convert normal decimal numbers to binary coded decimal
byte  Helper::dec_bcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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