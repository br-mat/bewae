////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// email: matthiasbraun@gmx.at
//
// This file containes the implementation of the Heleper Classes
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Standard
#include <Arduino.h>
#include <Wire.h>
//#include <SPI.h> // TODO fix namespacing problem with spiffs fs::File and SDfs::File for example
//#include <SD.h>

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>

#include <Helper.h>

/*
#define SD_MOSI      13
#define SD_MISO      5
#define SD_SCK       14

SPIClass spiSD(HSPI);
//spiSD.begin(SD_SCK, SD_MISO, SD_MOSI);
*/



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  HELPER     functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function description: copy strings
//FUNCTION PARAMETER:
//src         -- source array                                                       int
//dst         -- destiny array                                                      int
//len         -- length of array                                                    int
//------------------------------------------------------------------------------------------------
void HelperBase::copy(int* src, int* dst, int len) {
  memcpy(dst, src, sizeof(src[0])*len);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Convert normal decimal numbers to binary coded decimal
byte  HelperBase::dec_bcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Convert binary coded decimal to normal decimal numbers
byte  HelperBase::bcd_dec(byte val)
{
  return( (val/16*10) + (val%16) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: sets the time on the rtc module (iic)
//FUNCTION PARAMETERS:
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
void HelperBase::set_time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
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

//Function: read the time on the rtc module (iic)
//FUNCTION PARAMETERS:
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
//month      --                     month -- byte
//year       --   year as number 2 digits -- byte
void HelperBase::read_time(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  byte status = Wire.endTransmission(); // check if the transmission was successful
  if (status == 0) { // no error
    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
    // request seven bytes of data from DS3231 starting from register 00h
    *second = bcd_dec(Wire.read() & 0x7f);
    *minute = bcd_dec(Wire.read());
    *hour = bcd_dec(Wire.read() & 0x3f);
    *dayOfWeek = bcd_dec(Wire.read());
    *dayOfMonth = bcd_dec(Wire.read());
    *month = bcd_dec(Wire.read());
    *year = bcd_dec(Wire.read());
    #ifdef DEBUG
    char timestamp[20];
    sprintf(timestamp, "%02d.%02d.%02d %02d:%02d:%02d", *dayOfMonth, *month, *year, *hour, *minute, *second);
    Serial.println(timestamp);
    #endif
  } else { // error occurred
    // set all values to zero
    *second = 0;
    *minute = 0;
    *hour = 0;
    *dayOfWeek = 0;
    *dayOfMonth = 0;
    *month = 0;
    *year = 0;
    #ifdef DEBUG
    Serial.println(F("Warning: DS3231 not connected"));
    #endif
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Function: read the time on the rtc module improved error handling
//FUNCTION PARAMETERS:                      (i2c)
//second     --                   seconds -- byte
//minute     --                   minutes -- byte
//hour       --                   hours   -- byte
//dayofweek  --         weekday as number -- byte
//dayofmonrh --    day of month as number -- byte
//month      --                     month -- byte
//year       --   year as number 2 digits -- byte
bool HelperBase::readTime(byte *second,byte *minute,byte *hour,byte *dayOfWeek,byte *dayOfMonth,byte *month,byte *year)
{
  // enable rtc module
  HWHelper.enablePeripherals();
  delayMicroseconds(300);

  // set up transmission
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  byte status = Wire.endTransmission(); // check if the transmission was successful

  // catch error case
  int i = 0;
  while(status != 0){
    //loop as long as the rtc module is unavailable!
    #ifdef DEBUG
    Serial.print(F(". "));
    #endif
    Wire.beginTransmission(DS3231_I2C_ADDRESS); // init
    status = Wire.endTransmission(); // check
    if (i == static_cast<int>(5)) HWHelper.enablePeripherals(); // reactivate power
    if (i > static_cast<int>(10)) break; // break loop
    delay(100); // give time
    i++;
  }

  // read time
  if (status == 0) { // no error
    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
    // request seven bytes of data from DS3231 starting from register 00h
    *second = bcd_dec(Wire.read() & 0x7f);
    *minute = bcd_dec(Wire.read());
    *hour = bcd_dec(Wire.read() & 0x3f);
    *dayOfWeek = bcd_dec(Wire.read());
    *dayOfMonth = bcd_dec(Wire.read());
    *month = bcd_dec(Wire.read());
    *year = bcd_dec(Wire.read());
    //#ifdef DEBUG
    //char timestamp[20];
    //sprintf(timestamp, "%02d.%02d.%02d %02d:%02d:%02d", *dayOfMonth, *month, *year, *hour, *minute, *second);
    //Serial.println(timestamp);
    //#endif
    return true; // all good
  } else { // error occurred
    // set all values to zero
    *second = 0;
    *minute = 0;
    *hour = 0;
    *dayOfWeek = 0;
    *dayOfMonth = 0;
    *month = 0;
    *year = 0;
    #ifdef DEBUG
    Serial.println(F("Warning: DS3231 not connected"));
    #endif
    return false; // problem occured
  }
  return false; // should never be reached
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: check if give pin is valid
bool HelperBase::checkAnalogPin(int pin_check) // check if passed pin is valid
{
  #ifdef DEBUG
  Serial.println(F("Warning: Empty function! Define a proper function for the inherrited class!"));
  #endif
  return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: measure routine analog pin
int HelperBase::readAnalogRoutine(uint8_t gpiopin)
{
    // test given pin
    if(!checkAnalogPin(gpiopin)){
      return 0;
    }

    pinMode(gpiopin, INPUT); // make sure pin is on input mode
    delayMicroseconds(50);

    int num = 15;
    float mean = 0;
    int throwaway;
    // throw away
    for(int j = 0; j<5; j++){
        throwaway = analogRead(gpiopin); // Read the value from the specified pin
        delayMicroseconds(50);
    }
    
    // take mean
    for(int j = 0; j<num; j++){
        mean += analogRead(gpiopin); // Read the value from the specified pin
        delayMicroseconds(100);
    }

    float resultf = (mean/(float)num)+0.5f;
    return (int)resultf;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: give back a timestamp as string (iic)
//FUNCTION PARAMETERS:
// NONE
String HelperBase::timestamp(){
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
bool HelperBase::connectWifi(){
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
    delay(250);
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
  Serial.println();
  Serial.println(F("WiFi connected"));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("RSSI: "));
  Serial.print(WiFi.RSSI()); Serial.println(" dBm");
  #endif
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// force modem sleep
void HelperBase::setModemSleep() {
  WiFi.setSleep(true);
  if (!setCpuFrequencyMhz(80)){
      #ifdef DEBUG
      Serial.println(F("Not valid frequency!"));
      #endif
  }
  // Use this if 40Mhz is not supported
  // setCpuFrequencyMhz(80); //(40) also possible
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disables the WiFi on the device.
// Returns true if the WiFi was successfully disabled, false if an error occurred.
bool HelperBase::disableWiFi(){
  // Disconnect from the WiFi network.
  WiFi.disconnect(true);  
  // Set the WiFi mode to off.
  WiFi.mode(WIFI_OFF);
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// unused
void HelperBase::disableBluetooth(){
  // Quite unusefully, no relevable power consumption
  btStop();
  #ifdef DEBUG
  Serial.println();
  Serial.println(F("Bluetooth stop!"));
  #endif
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// waking up system
void HelperBase::wakeModemSleep() {
  #ifdef DEBUG
  Serial.println(F("Waking up modem!"));
  #endif
  setCpuFrequencyMhz(240);
  HelperBase::connectWifi();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// search item in an array returning bool
bool HelperBase::find_element(int *array, int item){
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
DynamicJsonDocument HelperBase::readConfigFile(const char path[PATH_LENGTH]) {
  // create buffer file
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
bool HelperBase::writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]) {
  if (path == nullptr) { // check for valid path, a function could possibly use this and set a nullptr as path
    #ifdef DEBUG
    Serial.println(F("Path nullptr not allowed!"));
    #endif
    return false;
  }

  // open old file
  DynamicJsonDocument oldFile_buff(CONF_FILE_SIZE);
  oldFile_buff = readConfigFile(path);
  // Check if doc is empty
  if (oldFile_buff.isNull()) {
    #ifdef DEBUG
    Serial.println(F("Warning: Failed to read file or empty JSON object."));
    #endif
  }
  // calculate old hash value
  String oldhash = calculateJSONHash(oldFile_buff);
  
  // calculate new hash
  String newhash = calculateJSONHash(jsonDoc);

  // check if file has changed
  if(!(oldhash != newhash)){
    #ifdef DEBUG
    Serial.println(F("Not Saving file: File unchanged!"));
    #endif
    return true;
  }

  // clear memory before opening new file
  oldFile_buff.clear();

  // open new file to save changes
  fs::File newFile = SPIFFS.open(path, "w"); // open the config file for writing
  if (!newFile) { // check if file was opened successfully
    #ifdef DEBUG
    Serial.println(F("Error: Failed to open config file for writing"));
    #endif
    newFile.close();
    return false; // error occured
  }

  // update & apppend hash
  jsonDoc.remove("checksum");
  jsonDoc["checksum"] = newhash;

  // Write the JSON data to the config file
  serializeJson(jsonDoc, newFile);
  newFile.close();

  return true; // all good
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// send data to influxdb, return true when everything is ok
bool HelperBase::pubInfluxData(InfluxDBClient* influx_client, String sensor_name, String field_name, float value) {
  #ifdef DEBUG
  Serial.print(F("Publishing sensor: ")); Serial.println(sensor_name);
  Serial.print(F("Field: ")); Serial.println(field_name);
  #endif

  bool cond = HelperBase::connectWifi();
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
  if (!influx_client->writePoint(point)) {
    #ifdef DEBUG
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(influx_client->getLastErrorMessage());  // Print error message if write operation is unsuccessful
    #endif
    return false;
  }
  else{
    #ifdef DEBUG
    Serial.println(F("InfluxDB points sent"));
    #endif
    return true;
  }
  // default (should never be reached)
  return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// blink onboard LED
void HelperBase::blinkOnBoard(String howLong, int times) {
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
JsonObject HelperBase::getJsonObjects(const char* key, const char* filepath) {
  // load the stored file and get all keys
  DynamicJsonDocument doc(CONF_FILE_SIZE);
  doc = HelperBase::readConfigFile(filepath);

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
    #ifdef DEBUG
    Serial.println(F("Warning: Key not found in JSON object."));
    #endif
  }
  
  return jsonobj;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// HTTP GET request to the Raspberry Pi server
DynamicJsonDocument HelperBase::getJSONConfig(const char* server, int serverPort, const char* serverPath) {
  // create buffer file
  int max_retries = 3;
  int retries = 0;
  while (retries < max_retries) {
    DynamicJsonDocument jsonConfdata(CONF_FILE_SIZE);
    HTTPClient http;
    http.begin(String("http://") + server + ":" + serverPort + serverPath);
    int httpCode = http.GET();
    String databuffer = http.getString();

    // Check the status code
    if (httpCode == HTTP_CODE_OK) {
      // Parse the JSON data
      // check for errors
      DeserializationError error = deserializeJson(jsonConfdata, databuffer);
      bool empty = false;
      if (!(databuffer != String(""))) {
        #ifdef DEBUG
        Serial.println(F("Warning: retrieved file empty!"));
        #endif
        empty = true;
      }

      // veryfy content of file using sha256 hash
      bool verification = verifyChecksum(jsonConfdata);
      if(!verification){
        #ifdef DEBUG
        Serial.println(F("Warning: File verification went wrong!"));
        #endif
      }

      // check problems if all good return data
      if ((bool)error || empty) {
        #ifdef DEBUG
        Serial.println(F("Error: Problem when parsing JSON data"));
        #endif
      } else {
        // correct return
        return jsonConfdata;
      }

    } else {
      #ifdef DEBUG
      Serial.println(F("Warning: Sending request to server went wrong"));
      #endif
    }
    http.end();
    retries++;
  }
  #ifdef DEBUG
  Serial.println(F("Info: An error occurred while retrieving the JSON data! Exiting, returned empty doc."));
  #endif
  DynamicJsonDocument empty(CONF_FILE_SIZE);
  return empty;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// HTTP GET request to the Raspberry Pi server
DynamicJsonDocument HelperBase::getJSONData(const char* server, int serverPort, const char* serverPath) {
  // create buffer file
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

// This function calculates the SHA-256 hash of the input content and returns the hash as a hexadecimal string.
String HelperBase::sha256(String content) {
  // Create an instance of the SHA256 hasher
  SHA256 hasher;
  // Update the hasher with the content's data
  hasher.doUpdate(content.c_str(), content.length());
  // Prepare an array to hold the resulting hash
  byte hash[SHA256_SIZE];
  // Finalize the hash calculation and store it in the 'hash' array
  hasher.doFinal(hash);

  String result = "";
  // Convert each byte of the hash to a two-digit hexadecimal representation
  for (byte i = 0; i < SHA256_SIZE; i++) {
    // If the current byte's value is less than 0x10 (16 in decimal), add a leading '0'
    if (hash[i] < 0x10) {
      result += '0';
    }
    result += String(hash[i], HEX);
  }

  // Return the calculated SHA-256 hash as a hexadecimal string
  return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// This function verifies the integrity of received JSON data by comparing its checksum with a calculated checksum.
bool HelperBase::verifyChecksum(DynamicJsonDocument& JSONdata) {
  // to make it easier when editing the file manually:
  // if no hash i sprovided in checksum it will calculate and add the hash, however a warning is given in serial
  // Hint: this "feature" could be removed later on to allow detection of wrongly transmitted files
  if(!JSONdata.containsKey("checksum")){
    // generate hash if not contained
    String calculatedChecksum = calculateJSONHash(JSONdata);
    #ifdef DEBUG
    Serial.println(F("Warning: File could not be verified. No checksum hash found in file!"));
    Serial.println(F("Added file hash to passed file:"));
    Serial.print(F("Sha256 file hash:")); Serial.println(calculatedChecksum);
    #endif
    return true; // print warning and set true to make it optional
  }

  // Extract the received checksum from the JSON data
  String receivedChecksum = JSONdata["checksum"].as<String>();

  String calculatedChecksum = calculateJSONHash(JSONdata);

  // Compare the received checksum with the calculated checksum
  if (receivedChecksum != calculatedChecksum) {
    #ifdef DEBUG
    Serial.println(F("Checksum verification failed"));
    #endif
    return false;
  }

  // Return true to indicate successful verification
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// calculate hash of a json doc without optional contained hash value ("checksum")
String HelperBase::calculateJSONHash(DynamicJsonDocument& JSONdata) {
  bool containsHash = false;
  // prepare hash
  if(JSONdata.containsKey("checksum")){
    JSONdata.remove("checksum");
    containsHash = true;
  }
  String fileStr = "";
  serializeJson(JSONdata, fileStr);
  // calculate hash
  String hash = sha256(fileStr);
  // Add the checksum field back to the original JSON data
  if(containsHash){
    JSONdata["checksum"] = hash;
  }
  return hash;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The updateConfig function retrieves new JSON data from a server and updates the config file,
// while preserving the "watering" values of the old JSON data.
// This value is only relevant for the controller itself.
// The function takes in a const char* type argument named path which specifies the path to retrieve new JSON data from the server.
// Returns a bool value indicating the success of updating the config file with new JSON data from the server.
bool HelperBase::updateConfig(const char* path){
  // Check if the device is connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {
    // Retrieve new JSON data from the server
    DynamicJsonDocument newdoc = HelperBase::getJSONConfig(SERVER, SERVER_PORT, path);
    //DynamicJsonDocument newdoc = HelperBase::getJSONData(SERVER, SERVER_PORT, path);
    // Check if the retrieved data is not null
    if(newdoc.isNull()){
      #ifdef DEBUG
      Serial.println(F("Warning: Problem recieving config file from server!"));
      #endif
      return false;
    }

    // preserve some old values only relevant for the controller
    // Access the "group" object of the new JSON data
    JsonObject group = newdoc["group"];
    // Retrieve the old group object from the config file
    JsonObject oldGroup = HelperBase::getJsonObjects("group", CONFIG_FILE_PATH);
    // Iterate over all the keys in the new group object
    for (JsonObject::iterator it = group.begin(); it != group.end(); ++it) {
      const char* key = it->key().c_str();
      // Check if the key is within the keys of oldGroup
      // check if key exists
      if (oldGroup.containsKey(key)) {
          // preserve values
          it->value()["watering"] = oldGroup[key]["watering"].as<int16_t>();
          it->value()["lastup"] = oldGroup[key]["lastup"].as<JsonArray>();
        }
        else {
          // new group should be created and saved, or some error occured
          #ifdef DEBUG
          Serial.println(F("Warning: Failed to retrieve watering value of old group object"));
          #endif
        }
      }
    // Write the updated JSON data to the config file
    return HelperBase::writeConfigFile(newdoc, CONFIG_FILE_PATH);
  }
  else{
    #ifdef DEBUG
    Serial.println(F("Error no Wifi connection established"));
    #endif
    return false;
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void HelperBase::shiftvalue8b(uint8_t val, bool invert) {
    // Provide a new implementation of system_sleep specific to Helper_config1_alternate here
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HelperBase::shiftvalue(uint32_t val, uint8_t numBits, bool invert) {
    // Provide a new implementation of system_sleep specific to Helper_config1_alternate here
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HelperBase::system_sleep() {
    // Provide a new implementation of system_sleep specific to Helper_config1_alternate here
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HelperBase::controll_mux(uint8_t channel, String mode, int *val) {
    // Provide a new implementation of system_sleep specific to Helper_config1_alternate here
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate system
void HelperBase::enablePeripherals() {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable system
void HelperBase::disablePeripherals() {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate additional sensor rail
void HelperBase::enableSensor() {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable additional sensor rail
void HelperBase::disableSensor() {
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// seting shiftregister to defined value (8bit)
void Helper_config1_Board1v3838::shiftvalue8b(uint8_t val, bool invert){
  //Function description: shiftout 8 bit value, MSBFIRST
  //FUNCTION PARAMETER:
  //val         -- 8bit value writte out to shift register                             uint8_t
  //------------------------------------------------------------------------------------------------

  // invert val if needed
  if (invert) {
    val = ~val;  // Invert the value if the invert flag is set to true
  }
  digitalWrite(Pins::ST_CP_SHFT, LOW);
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, val); //take byte type as value
  digitalWrite(Pins::ST_CP_SHFT, HIGH); //update output of the register
  delayMicroseconds(100);
  digitalWrite(Pins::ST_CP_SHFT, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// set shiftregister to defined value (32 bit)
void Helper_config1_Board1v3838::shiftvalue(uint32_t val, uint8_t numBits, bool invert) {
  // Function description: shift out specified number of bits from a value, MSBFIRST
  // FUNCTION PARAMETERS:
  // val       -- value to be shifted out                      uint32_t
  // numBits   -- number of bits to be shifted out              uint8_t
  // ------------------------------------------------------------------------------------------------

  // invert val if needed
  if (invert) {
    val = ~val;  // Invert the value if the invert flag is set
  }

  #ifdef DEBUG_SPAM
  Serial.print(F("Shifting '"));
  Serial.print(val, BIN); Serial.println(F("'"));
  #endif

  // Split the long value into two bytes
  byte highByte = (val >> 8) & 0xFF;
  byte lowByte = val & 0xFF;

  // Shift out the high byte first
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, highByte);

  // Then shift out the low byte
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, lowByte);

  // Pulse the latch pin to activate the outputs
  digitalWrite(Pins::ST_CP_SHFT, LOW);
  digitalWrite(Pins::ST_CP_SHFT, HIGH);
  digitalWrite(Pins::ST_CP_SHFT, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: deactivate the modules, prepare for sleep & setting mux to "lowpower standby" mode:
void Helper_config1_Board1v3838::system_sleep(){
  digitalWrite(Pins::PWM, LOW);     //pulls vent pwm pin low
  digitalWrite(Pins::SW_SENS, LOW);  //deactivates sensors
  digitalWrite(Pins::SW_SENS2, LOW);      //deactivates energy hungry devices
  digitalWrite(Pins::SW_3_3V, LOW);      //deactivates energy hungry devices
  delay(1);
  digitalWrite(Pins::EN_MUX_1 , HIGH);    //deactivates mux 1 on HIGH
  digitalWrite(Pins::S0_MUX_1 , HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(Pins::S1_MUX_1 , HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(Pins::S2_MUX_1, HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)
  digitalWrite(Pins::S3_MUX_1 , HIGH);    // pull high to avoid leakage over mux controll pins (which happens for some reason?!)

  disableWiFi();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
void Helper_config1_Board1v3838::controll_mux(uint8_t channel, String mode, int *val){
  // shutdown wifi to avoid conflicts wif ADC2
  disableWiFi(); // make sure to free adc2
  enableSensor();
  enablePeripherals();
  delay(100); // give time to settle

//delay(5000); // TEMP! REMOVE!

  pinMode(SIG_MUX_1, INPUT);
  pinMode(S0_MUX_1, OUTPUT);
  pinMode(S1_MUX_1, OUTPUT);
  pinMode(S2_MUX_1, OUTPUT);
  pinMode(S3_MUX_1, OUTPUT);
  pinMode(EN_MUX_1, OUTPUT);

  // define important variables
  uint8_t sipsop = this->SIG_MUX_1;
  uint8_t enable = this->EN_MUX_1;

  // setup pin config
  int control_pins[4] = {S0_MUX_1,S1_MUX_1,S2_MUX_1,S3_MUX_1};
  
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
  delay(1);

  //selecting channel
  for(int i=0; i<4; i++){
    digitalWrite(control_pins[i], channel_setup[channel][i]);
  }

  delay(100); // give time to settle

//delay(10000); // TEMP! REMOVE!

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
    pinMode(sipsop, INPUT); //make sure its on input
    digitalWrite(enable, LOW);
    delay(250); //give time to stabilize reading

    int meas = readAnalogRoutine(sipsop);
    *val=meas;

    delayMicroseconds(10);
    digitalWrite(enable, HIGH);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate system
void Helper_config1_Board1v3838::enablePeripherals() {
  digitalWrite(Pins::SW_3_3V, HIGH); delay(5);
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
  digitalWrite(Pins::SW_SENS, HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable system
void Helper_config1_Board1v3838::disablePeripherals() {
    digitalWrite(Pins::SW_3_3V, LOW); delay(5);
    HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
    digitalWrite(Pins::SW_SENS, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate additional sensor rail
void Helper_config1_Board1v3838::enableSensor() {
  digitalWrite(Pins::SW_SENS2, HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable additional sensor rail
void Helper_config1_Board1v3838::disableSensor() {
  digitalWrite(Pins::SW_SENS, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// set pins to mode
void Helper_config1_Board1v3838::setPinModes() {
    for (auto pin : this->output_pins) {
        pinMode(pin, OUTPUT);
    }
    for (auto pin : this->input_pins) {
        pinMode(pin, INPUT);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: check if give pin is valid
bool Helper_config1_Board1v3838::checkAnalogPin(int pin_check)
{
  int arraySize = sizeof(input_pins) / sizeof(input_pins[0]);
  for (int i = 0; i < arraySize; i++) {
    if ((uint8_t)pin_check == (uint8_t)input_pins[i]) {
      return true; // Valid pin found
    }
  }
  #ifdef DEBUG
  Serial.println(F("Invalid pin detected!"));
  #endif
  
  return false; // Pin not found
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// shift 8 bit value (probably not tested, use shiftvalue instead!)
void Helper_config1_Board5v5::shiftvalue8b(uint8_t val, bool invert) {
  // init register
  digitalWrite(Pins::SW_3_3V, LOW);
  digitalWrite(Pins::SH_CP_SHFT, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(Pins::ST_CP_SHFT, LOW);
  digitalWrite(Pins::DATA_SHFT, LOW);
  delayMicroseconds(100);
  digitalWrite(Pins::SW_3_3V, HIGH);

  // invert val if needed
  if (invert) {
    val = ~val;  // Invert the value if the invert flag is set to true
  }

  digitalWrite(Pins::ST_CP_SHFT, LOW);
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, val); //take byte type as value
  digitalWrite(Pins::ST_CP_SHFT, HIGH); //update output of the register
  delayMicroseconds(100);
  digitalWrite(Pins::ST_CP_SHFT, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper_config1_Board5v5::shiftvalue(uint32_t val, uint8_t numBits, bool invert) {
  // Function description: shift out specified number of bits from a value, MSBFIRST
  // FUNCTION PARAMETERS:
  // val       -- value to be shifted out                      uint32_t
  // numBits   -- number of bits to be shifted out              uint8_t
  // ------------------------------------------------------------------------------------------------

  // init register
  digitalWrite(Pins::SW_3_3V, LOW);
  digitalWrite(Pins::SH_CP_SHFT, LOW); //make sure clock is low so rising-edge triggers
  digitalWrite(Pins::ST_CP_SHFT, LOW);
  digitalWrite(Pins::DATA_SHFT, LOW);
  delayMicroseconds(100);
  digitalWrite(Pins::SW_3_3V, HIGH);

  // invert val if needed
  if (invert) {
    val = ~val;  // Invert the value if the invert flag is set
  }

  #ifdef DEBUG_SPAM
  Serial.println();
  Serial.print(F("Shifting '"));
  Serial.print(val, BIN); Serial.println(F("'"));
  #endif

  // Split the long value into two bytes
  byte highByte = (val >> 8) & 0xFF;
  byte lowByte = val & 0xFF;

  // Shift out the high byte first
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, highByte);

  // Then shift out the low byte
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, lowByte);

  // Pulse the latch pin to activate the outputs
  digitalWrite(Pins::ST_CP_SHFT, LOW);
  digitalWrite(Pins::ST_CP_SHFT, HIGH);
  digitalWrite(Pins::ST_CP_SHFT, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Helper_config1_Board5v5::system_sleep() {
  digitalWrite(Pins::PWM, LOW);     //pulls vent pwm pin low
  digitalWrite(Pins::SW_SENS, LOW);  //deactivates sensors
  digitalWrite(Pins::SW_SENS2, LOW);      //deactivates energy hungry devices
  digitalWrite(Pins::SW_3_3V, LOW);      //deactivates energy hungry devices
  delay(1);

  disableWiFi();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate system
void Helper_config1_Board5v5::enablePeripherals() {
  digitalWrite(Pins::SW_3_3V, HIGH); delay(5);
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
  digitalWrite(Pins::SW_SENS, HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable system
void Helper_config1_Board5v5::disablePeripherals() {
    digitalWrite(Pins::SW_3_3V, LOW); delay(5);
    HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
    digitalWrite(Pins::SW_SENS, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate additional sensor rail
void Helper_config1_Board5v5::enableSensor() {
  digitalWrite(Pins::SW_SENS2, HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable additional sensor rail
void Helper_config1_Board5v5::disableSensor() {
  digitalWrite(Pins::SW_SENS2, LOW);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// set pins to mode
void Helper_config1_Board5v5::setPinModes() {
    for (auto pin : this->output_pins) {
        pinMode(pin, OUTPUT);
    }
    for (auto pin : this->input_pins) {
        pinMode(pin, INPUT);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: check if give pin is valid
bool Helper_config1_Board5v5::checkAnalogPin(int pin_check)
{
  int arraySize = sizeof(input_pins) / sizeof(input_pins[0]);
  for (int i = 0; i < arraySize; i++) {
Serial.print("Pin to check"); Serial.print(pin_check); Serial.print(" pin found: "); Serial.println(input_pins[i]);
    if ((uint8_t)pin_check == (uint8_t)input_pins[i]) {
      return true; // Valid pin found
    }
  }
  
  #ifdef DEBUG
  Serial.println(F("Invalid pin detected!"));
  #endif
  
  return false; // Pin not
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////