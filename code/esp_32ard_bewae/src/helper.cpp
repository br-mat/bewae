////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// see gitHub for author info
//
// This file containes the implementation of the Heleper Classes
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Standard
#include <Arduino.h>
#include <Wire.h>
//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>

#include <Helper.h>
#include "LogFire.h"

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

//Function: sets the time on the rtc module (iic)
bool HelperBase::setTime(struct tm timeinfo)
{
  if(!verifyTM(timeinfo)){
    LogFire.log("setTime: invalid tm struct", 3);
    return false;
  }

  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  byte status = Wire.endTransmission();
  if (status != 0) {
    LogFire.log("setTime: DS3231 not connected", 3);
    return false;
  }
  delay(2);
  // DS3231 ist verbunden, setze Zeit und Datum
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(dec_bcd(timeinfo.tm_sec)); // set seconds
  Wire.write(dec_bcd(timeinfo.tm_min)); // set minutes
  Wire.write(dec_bcd(timeinfo.tm_hour)); // set hours
  Wire.write(dec_bcd(timeinfo.tm_wday)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(dec_bcd(timeinfo.tm_mday)); // set date (1 to 31)
  Wire.write(dec_bcd(timeinfo.tm_mon + 1)); // set month
  Wire.write(dec_bcd(timeinfo.tm_year - 2000)); // set year (0 to 99)
  status = Wire.endTransmission();

  if (status != 0) {
    LogFire.log("setTime: DS3231 write failed", 3);
    return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// new implementation using tm struct
bool HelperBase::readTime(struct tm* timeinfo)
{
#ifdef OFFLINE_TEST
  // Time simulation: 10 real seconds = 1 simulated hour. Cycles through full days.
  // Scheduled hours in plantConfig timetable 8399362: 1, 9, 11, 13, 23
  unsigned long elapsed_hours = millis() / 10000UL;
  memset(timeinfo, 0, sizeof(struct tm));
  timeinfo->tm_hour = elapsed_hours % 24;
  timeinfo->tm_min  = 0;
  timeinfo->tm_sec  = 0;
  timeinfo->tm_mday = 1 + (int)((elapsed_hours / 24) % 28);
  timeinfo->tm_mon  = 2;
  timeinfo->tm_year = 125;
  return true;
#endif
  LogFire.log("readTime function started", 0);
  bool status = false;
  memset(timeinfo, 0, sizeof(struct tm)); // set all values to zero

  *timeinfo = readTimeRTC();
  status = verifyTM(*timeinfo);

  // read time
  if (status) {
    LogFire.log("RTC time: " + timestamp(*timeinfo), 1);
    return true;
  }
  *timeinfo = readTimeNTP();
  status = verifyTM(*timeinfo);
  if (status) {
    LogFire.log("NTP time: " + timestamp(*timeinfo), 1);
    return true;
  }

  LogFire.log("readTime: neither NTP nor RTC working", 3);
  return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function: get time from RTC module
struct tm HelperBase::readTimeRTC()
{
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(struct tm)); // set all values to zero

  // enable rtc module
  HWHelper.enablePeripherals();
  delay(2);

  // set up transmission
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  byte status = Wire.endTransmission(); // check if the transmission was successful

  int i = 0;
  while(status != 0){
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    status = Wire.endTransmission();
    if (i == static_cast<int>(5)) HWHelper.enablePeripherals();
    if (i > static_cast<int>(10)) break;
    delay(100);
    i++;
  }

  if (status == 0) {
    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);

    timeinfo.tm_sec = bcd_dec(Wire.read() & 0x7f);
    timeinfo.tm_min = bcd_dec(Wire.read());
    timeinfo.tm_hour = bcd_dec(Wire.read() & 0x3f);
    timeinfo.tm_wday = bcd_dec(Wire.read());
    timeinfo.tm_mday = bcd_dec(Wire.read());
    timeinfo.tm_mon = bcd_dec(Wire.read()) - 1;
    timeinfo.tm_year = bcd_dec(Wire.read());
    LogFire.log("RTC read: OK", 0);
  } else {
    LogFire.log("readTimeRTC: DS3231 not connected", 2);
  }

  return timeinfo;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function: get local time from network
struct tm HelperBase::readTimeNTP()
{
  // Verbindung zum WLAN herstellen
  HWHelper.connectWifi();

  // Zeitkonfiguration
  configTime(gmtOffset_hours * SECONDS_PER_HOUR, daylightOffset_hours * SECONDS_PER_HOUR, NTP_Server);

  struct tm timeinfo = {};  // zero-initialize so verifyTM() catches failures
  if(!getLocalTime(&timeinfo)){
    LogFire.log("readTimeNTP: failed to get local time", 3);
    return timeinfo;
  }
  return timeinfo;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Function: check if give pin is valid
bool HelperBase::checkAnalogPin(int pin_check)
{
  LogFire.log("checkAnalogPin: base class stub called — override not implemented", 2);
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

//Function: give back a timestamp as string
//FUNCTION PARAMETERS:
// struct tm data format
String HelperBase::timestamp(struct tm timedata){
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "Localtime: %d.%m.%y %H:%M:%S", &timedata);
  return String(timestamp);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Function: give back timestamp as string
String HelperBase::timestampNTP(){
  struct tm timeinfo;
  // retrieve data from DS3231
  timeinfo = readTimeNTP();
  return HelperBase::timestamp(timeinfo);
}

// Attempts to enable the WiFi and connect to a specified network.
// Returns true if the connection was successful, false if not.
bool HelperBase::connectWifi(){
  if (WiFi.status() == WL_CONNECTED) {
    LogFire.log("Wifi already connected!", 0);
    return true;
  }
  WiFi.disconnect(true);
  delayMicroseconds(100);
  WiFi.mode(WIFI_STA);

  LogFire.log("Connecting:", 1);
  WiFi.begin(ssid, wifi_password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    tries++;
    if(tries > 30){
      LogFire.log("connectWifi: failed after " + String(tries) + " attempts", 3);
      return false;
    }
  }

  LogFire.log("WiFi connected: IP: " + WiFi.localIP().toString() + " RSSI: " + String(WiFi.RSSI()) + " dBm", 1);
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// force modem sleep
void HelperBase::setModemSleep() {
  WiFi.setSleep(true);
  if (!setCpuFrequencyMhz(80)){
      LogFire.log("setModemSleep: 80MHz not supported, using default", 2);
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
  btStop();
  LogFire.log("Bluetooth stopped", 0);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// waking up system
void HelperBase::wakeModemSleep() {
  if (WiFi.status() == WL_CONNECTED) {
    LogFire.log("Wifi connection already established", 0);
    return;
  }
  LogFire.log("Waking up modem", 0);
  setCpuFrequencyMhz(240);
  HelperBase::connectWifi();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Reads the JSON file at the specified file path and returns the data as a DynamicJsonDocument.
// If the file path is invalid or if there is an error reading or parsing the file, an empty DynamicJsonDocument is returned.
DynamicJsonDocument HelperBase::readConfigFile(const char path[PATH_LENGTH]) {
  // create buffer file
  DynamicJsonDocument jsonDoc(CONF_FILE_SIZE); // create JSON doc, if an error occurs it will return an empty jsonDoc
                                     // which can be checked using jsonDoc.isNull()
  if (path == nullptr) {
    LogFire.log("readConfigFile: null path", 3);
    return jsonDoc;
  }

  File configFile = SPIFFS.open(path, "r");
  if (!configFile) {
    LogFire.log("readConfigFile: failed to open " + String(path), 2);
    return jsonDoc;
  }

  DeserializationError error = deserializeJson(jsonDoc, configFile);
  configFile.close();
  if (error) {
    LogFire.log("readConfigFile: parse error in " + String(path), 2);
    jsonDoc.clear();
    return jsonDoc;
  }

  return jsonDoc;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Writes the specified DynamicJsonDocument to the file at the specified file path as a JSON file.
// Returns true if the file was written successfully, false if the file path is invalid or if there is an error writing the file.
bool HelperBase::writeConfigFile(DynamicJsonDocument jsonDoc, const char path[PATH_LENGTH]) {
  if (path == nullptr) {
    LogFire.log("writeConfigFile: null path", 3);
    return false;
  }
  DynamicJsonDocument oldFile_buff(CONF_FILE_SIZE);
  oldFile_buff = readConfigFile(path);
  if (oldFile_buff.isNull()) {
    LogFire.log("writeConfigFile: no existing file at " + String(path) + ", creating", 1);
  }
  // calculate old hash value
  //String oldhash = calculateJSONHash(oldFile_buff);
  String oldhash = oldFile_buff["checksum"].as<String>();
  bool valid_hash = verifyChecksum(oldFile_buff);
  /*
  Serial.println("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  Serial.println("OLD FILE:");
  serializeJson(oldFile_buff, Serial);
  Serial.println();
  Serial.println("NEW FILE:");
  serializeJson(jsonDoc, Serial);
  Serial.println("----------------------------------------------------------------------------------------");
  Serial.print("OLDHash (calc)"); Serial.println(calculateJSONHash(oldFile_buff));*/

  // clear memory before opening new file
  oldFile_buff.clear();

  // Create a new DynamicJsonDocument with the same size as the old file buffer
  //DynamicJsonDocument newFile_buff(CONF_FILE_SIZE);
  //jsonDoc = jsonDoc;
  // At the end of your function, if you need to clear jsonDoc for some reason
  //jsonDoc.clear();

  // calculate new hash
  String newhash = calculateJSONHash(jsonDoc);
  bool changed_hash = !(oldhash == newhash);
  // update & apppend hash
  jsonDoc["checksum"] = newhash;

  /*
  Serial.print("OLDHash "); Serial.println(oldhash);
  Serial.print("NEWHash "); Serial.println(newhash);
  Serial.print("Free heap memory: ");
  Serial.println(ESP.getFreeHeap());*/

  // check if file has changed or if old file does not have a checksum
  if(!changed_hash && valid_hash){
    LogFire.log("Not Saving file: File unchanged and checksum present! Path:" + String(path), 0);
    return true;
  }

  fs::File newFile = SPIFFS.open(path, "w");
  if (!newFile) {
    LogFire.log("writeConfigFile: failed to open for writing " + String(path), 3);
    newFile.close();
    return false;
  }
  if (serializeJson(jsonDoc, newFile) == 0) {
    LogFire.log("writeConfigFile: serialize failed for " + String(path), 3);
    newFile.close();
    return false;
  }
  newFile.close();
  LogFire.log("Updated: " + String(path), 1);
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// send data to influxdb, return true when everything is ok
bool HelperBase::pubInfluxData(InfluxDBClient* influx_client, String sensor_name, String field_name, float value) {
  LogFire.log("influx: publish sensor=" + sensor_name + " field=" + field_name + " val=" + String(value), 0);

  bool cond = HelperBase::connectWifi();
  if (!cond) {
    LogFire.log("influx: WiFi unavailable, skipping publish", 3);
    return false;
  }

  Point point(sensor_name);
  point.addField(field_name, value);

  if (!influx_client->writePoint(point)) {
    LogFire.log("influx: write failed: " + influx_client->getLastErrorMessage(), 3);
    return false;
  }
  LogFire.log("influx: write OK", 1);
  return true;
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
DynamicJsonDocument HelperBase::getJsonDoc(const char* filepath, const char* key) {
  // load the stored file and get all keys
  DynamicJsonDocument doc(CONF_FILE_SIZE);
  doc = readConfigFile(filepath);

  if (doc.isNull()) {
    LogFire.log("getJsonDoc: failed to read " + String(filepath), 2);
    return doc;
  }

  if (key != nullptr) {
    doc = doc[key];
    if (doc.isNull()) {
      LogFire.log("getJsonDoc: key \"" + String(key) + "\" not found in " + String(filepath), 2);
    }
  }
  String objStr;
  serializeJson(doc, objStr);
  LogFire.log("OBJ RETURN: " + objStr, 1);
  return doc;
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
    String serverAddress = String("http://") + server + ":" + serverPort + serverPath;
    http.begin(serverAddress);
    int httpCode = http.GET();
    String databuffer = http.getString();

    // Check the status code
    if (httpCode == HTTP_CODE_OK) {
      // Parse the JSON data
      // check for errors
      DeserializationError error = deserializeJson(jsonConfdata, databuffer);
      bool empty = false;
      if (!(databuffer != String(""))) {
        LogFire.log("getJSONConfig: empty response from " + serverAddress, 2);
        empty = true;
      }
      // NOTE: Network-level checksum verification is not used here because ArduinoJson and
      // JavaScript produce different serializations for the same JSON, making cross-platform
      // hashing unreliable. Flash write-avoidance is handled locally by writeConfigFile()
      // which compares hashes using the same serializer on both sides.

      // check problems if all good return data
      if ((bool)error || empty) {
        LogFire.log("getJSONConfig: parse error from " + serverAddress, 2);
      } else {
        // correct return
        return jsonConfdata;
      }

    } else {
      LogFire.log("getJSONConfig: HTTP " + String(httpCode) + " from " + serverAddress, 2);
    }
    http.end();
    retries++;
  }
  LogFire.log("getJSONConfig: all retries failed, returning empty doc", 2);
  DynamicJsonDocument empty(CONF_FILE_SIZE);
  return empty;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// HTTP POST JSON payload to server, returns true on HTTP 200
bool HelperBase::postJSON(const char* server, int serverPort, const char* serverPath, const String& payload) {
  int max_retries = 3;
  int retries = 0;
  while (retries < max_retries) {
    HTTPClient http;
    String serverAddress = String("http://") + server + ":" + serverPort + serverPath;
    http.begin(serverAddress);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);
    http.end();

    if (httpCode == HTTP_CODE_OK) {
      return true;
    }
    LogFire.log("postJSON: HTTP " + String(httpCode) + " from " + serverAddress, 2);
    retries++;
  }
  LogFire.log("postJSON: all retries failed", 3);
  return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// (HEX) This function calculates the SHA-256 hash of the input content and returns the hash as a hexadecimal string.
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
  if(!JSONdata.containsKey("checksum")){
    LogFire.log("verifyChecksum: no checksum field found", 2);
    return false;
  }

  // Extract the received checksum from the JSON data
  String receivedChecksum = JSONdata["checksum"].as<String>();

  String calculatedChecksum = calculateJSONHash(JSONdata);

  // Compare the received checksum with the calculated checksum
  if (receivedChecksum != calculatedChecksum) {
    LogFire.log("verifyChecksum: mismatch", 2);
    return false;
  }

  // Return true to indicate successful verification
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Function: check tm struct values
bool HelperBase::verifyTM(struct tm timeinfo){
  // check timeinfo
  if (timeinfo.tm_sec == 0 && timeinfo.tm_min == 0 && timeinfo.tm_hour == 0 &&
      timeinfo.tm_wday == 0 && timeinfo.tm_mday == 0 && timeinfo.tm_mon == 0 &&
      timeinfo.tm_year == 0) {
    return false;
  }
  if ((timeinfo.tm_year == 1970) || (timeinfo.tm_year == 70)) {
    return false;
  }
  return true; // all good
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Calculate hash of a JSON doc excluding the "checksum" field itself.
// Used by writeConfigFile() to detect changes and avoid unnecessary flash writes.
String HelperBase::calculateJSONHash(DynamicJsonDocument& JSONdata) {
  // Create a copy of the JSON data
  DynamicJsonDocument tempDoc(CONF_FILE_SIZE);
  //tempDoc.set(JSONdata);
  tempDoc = JSONdata;

  // Remove the checksum field if it exists
  tempDoc.remove("checksum");

  // Convert JSON to string
  String fileStr;
  serializeJson(tempDoc, fileStr);
  fileStr.trim(); // Remove any whitespace at the start or end

  // Calculate hash
  String hash = sha256(fileStr);

  // Return the calculated hash
  return hash;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The updateConfig function retrieves new JSON data from a server and updates the config file,
// paths get automatically converted to WEB and LOCAL file path (adding Pre-/Suffix where needed)
// Returns a bool value indicating the success
bool HelperBase::updateConfig(const char* fileType){
  if (WiFi.status() == WL_CONNECTED) {
    String fileTypeNoSlash = String(fileType).substring(1); // Remove the first character
    String pt1 = String(WEB_PREFIX) + String(F("?deviceName=")) + String(DEVICE_NAME);
    String pt2 = String(F("&fileType=")) + fileTypeNoSlash; // Use the modified fileType
    String webPath = pt1 + pt2;
    // Retrieve new JSON data from the server
    DynamicJsonDocument newdoc(CONF_FILE_SIZE);
    newdoc = HelperBase::getJSONConfig(SERVER, NODERED_PORT, webPath.c_str());
    //DynamicJsonDocument newdoc = HelperBase::getJSONData(SERVER, SERVER_PORT, path);
    // Check if the retrieved data is not null
    if(newdoc.isNull()){
      LogFire.log("updateConfig: failed to fetch " + String(fileType) + " from server", 2);
      return false;
    }
    // Write the updated JSON data to the config file
    String localPath = String(fileType) + String(JSON_SUFFIX);
    return HelperBase::writeConfigFile(newdoc, localPath.c_str());
  }
  else{
    LogFire.log("updateConfig: no WiFi, skipping " + String(fileType), 3);
    return false;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HelperBase::syncConfig(){
  byte count = 0;
  //count += HelperBase::updateConfigOLD(CONFIG_FILE_PATH);
  count += !HelperBase::updateConfig(DEVICE_CONFIG_PATH);
  count += !HelperBase::updateConfig(IRRIG_CONFIG_PATH);
  count += !HelperBase::updateConfig(SENS_CONFIG_PATH);
  if (!count) {
    LogFire.log("Successfully synched Config!", 1);
  }
  else{
    LogFire.log("syncConfig: " + String(count) + " file(s) failed to sync", 2);
  }
  return count; // return true if a problem occured
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool HelperBase::createFile(const char* filePath) {
  File file = SPIFFS.open(filePath, "w");
  bool isSuccess = true;

  if (!file) {
    isSuccess = false;
    LogFire.log("createFile: failed to create " + String(filePath), 3);
  }
  
  file.close(); // Close the file to ensure no resources are leaked
  return isSuccess;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO ! TODO: create funciton to clear up running file from old unused but configured irrigation groups
// use update function to remove all key and value pairs from running file which are not in irrig conf file
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

  // Split the long value into two bytes
  byte highByte = (val >> 8) & 0xFF;
  byte lowByte = val & 0xFF;

  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, highByte);
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, lowByte);

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
  digitalWrite(Pins::SW_3_3V, HIGH); digitalWrite(Pins::SW_SENS, HIGH);
  HWHelper.enableSensor();
  delay(50); // give shift register time to react
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable system
void Helper_config1_Board1v3838::disablePeripherals() {
    digitalWrite(Pins::SW_3_3V, LOW); delay(25);
    HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
    digitalWrite(Pins::SW_SENS, LOW);
    HWHelper.disableSensor();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Activate additional sensor rail
void Helper_config1_Board1v3838::enableSensor() {
  digitalWrite(Pins::SW_SENS2, HIGH);
  digitalWrite(Pins::SW_3_3V, HIGH);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable additional sensor rail
void Helper_config1_Board1v3838::disableSensor() {
  digitalWrite(Pins::SW_SENS2, LOW);
  digitalWrite(Pins::SW_3_3V, LOW);
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
      return true;
    }
  }
  LogFire.log("checkAnalogPin: pin " + String(pin_check) + " not in valid pin list", 2);
  return false;
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

  byte highByte = (val >> 8) & 0xFF;
  byte lowByte = val & 0xFF;

  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, highByte);
  shiftOut(Pins::DATA_SHFT, Pins::SH_CP_SHFT, MSBFIRST, lowByte);

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
  digitalWrite(Pins::SW_3_3V, HIGH); digitalWrite(Pins::SW_SENS, HIGH);
  delay(50); // give shift register time to react
  HWHelper.shiftvalue(0, max_groups, INVERT_SHIFTOUT);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Disable system
void Helper_config1_Board5v5::disablePeripherals() {
    digitalWrite(Pins::SW_3_3V, LOW); delay(25);
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
    if ((uint8_t)pin_check == (uint8_t)input_pins[i]) {
      return true;
    }
  }
  LogFire.log("checkAnalogPin: pin " + String(pin_check) + " not in valid pin list", 2);
  return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////