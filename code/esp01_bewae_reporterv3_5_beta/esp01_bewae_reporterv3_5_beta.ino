//IIC SLAVE MQTT REPORTER
#include "PubSubClient.h" // Connect and publish to the MQTT broker
#include <Wire.h>
#include <String.h>

// Code for the ESP8266
#include <ESP8266WiFi.h> // Enables the ESP to connect to the local network (via WiFi)

//definitions
#define DEBUG

#define MAX_MSG_LEN (128)
#define max_groups 6

// iic
#define slave_adr 0x08 //adress of esp01
#define nano_adr 0x07 //adress of arduino nano


// WiFi
const char* ssid = "A1-08e52c";         // Your personal network SSID
const char* wifi_password = "mabritck"; // Your personal network password

// MQTT
const char* mqtt_server = "10.0.0.18";  // IP of the MQTT broker
//const char* topic_prefix_original="home/test/temperature";
String topic_prefix = "home/sens3/z";
const char* mqtt_username = "mbraun"; // MQTT username
const char* mqtt_password = "wdsaibuii123"; // MQTT password
const char* clientID = "client_test2"; // MQTT client ID
const char* humidity_topic_sens2 = "home/sens2/humidity";
const char* temperature_topic_sens2 = "home/sens2/temperature";
const char* pressure_topic_sens2 = "home/sens2/pressure";
const char* config_status = "home/bewae/config_status"; //send status with mqtt to raspberrypi

//subsciption topics
const char* watering_topic = "home/bewae/config";    //bewae config and config status indicate things not passed to influx recive watering instructions
const char* bewae_sw = "home/nano/bewae_sw";       //switching watering system no/off
const char* watering_sw = "home/nano/watering_sw"; //switching value override on/off (off - using default values on nano)
const char* testing = "home/test/tester";

bool data_collected = false; //confirms that all requested data has been collected and wifi can be shut down
bool update_data = false;            //check if nano requests config data update
bool new_data = false;
const int max_sens=28;
int int_data[max_sens] = {0}; //keep it as int array, in case all this is migrated to a single mcu and storage space becomes relevant again
                              //data stay comparable as a whole

//same value as in code for nano probably put base values here
byte statusbyte = 0;   //indicating status of collected data (111 all good ..)
int groups[max_groups] = {0};
byte sw0; //bewae switch
byte sw1; //water value override switch

// Initialise the WiFi and MQTT Client objects
WiFiClient wifiClient;
// 1883 is the listener port for the Broker
PubSubClient client(mqtt_server, 1883, wifiClient); 


void connect_MQTT(){
  // connet to the MQTT broker via WiFi
  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_STA);
  delay(1);
  #ifdef DEBUG
  Serial.print("Connecting to ");
  Serial.println(ssid);
  #endif
  WiFi.begin(ssid, wifi_password);
  int tries = 0;
  while ((WiFi.status() != WL_CONNECTED) && (tries++ < 20)) {
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
      delay(2000);
    }
  }
  #ifdef DEBUG
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
    client.subscribe(watering_topic, 0); //(topic, qos) quality of service: qos 0 "fire and forget",
    client.subscribe(bewae_sw, 0);       // qos 1 confirm at least once, qos 2 double confirmation reciever
    client.subscribe(watering_sw, 0);
    client.subscribe(testing, 0);
    client.setCallback(callback);
  }
  else {
    #ifdef DEBUG
    Serial.println("Connection to MQTT Broker failed...");
    #endif
  }
}


byte transmitt_config(int16_t data[], byte len, byte stat){
  //this function sends instruction data with I2C to the arduino nano
  #ifdef DEBUG
  Serial.println("Transmitt process triggered");
  #endif
  if(len<32+1){ //max 32 bytes
    #ifdef DEBUG
    Serial.println(groups[5]);
    #endif
    byte rstatus = 0;
    byte bdata[(max_groups+2)*2+2]={0}; //max_group+2 --> all watering groups + 2 bool values; +2 extra bytes as framing
    bdata[sizeof(bdata)-1]=stat; //tail framing with fixed value for identification on nano
    memcpy(bdata+1, data, len);  //framing on begin of transmission -->0
    Wire.beginTransmission(nano_adr);
    Wire.write(bdata, sizeof(bdata));
    rstatus += Wire.endTransmission();
    #ifdef DEBUG
    Serial.print(F("Sent byte array is: "));
    for(int i=0;i<sizeof(bdata);i++){
      Serial.println(bdata[i]);
    }
    #endif
  }
  else{
    #ifdef DEBUG
    Serial.println(F("ERROR: transmitted array to big"));
    #endif
  }
}


void callback(char *topic, byte *payload, unsigned int msg_length){
  //callback function to receive mqtt data
  //watering topic payload rules:
  //',' is the sepperator -- no leading ',' -- msg end with ',' -- only int numbers
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
      if((int)c_index == (int)-1){            //index = -1 --> char nor found
        c_index=msg_length+1;      //add 1 to get full message range in case there is no ',' on payload end
      }
      //extracting csv values
      for(start_index; start_index<c_index-1; start_index++){
        val+=msg[start_index];
      }
      groups[i]=val.toInt();
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
    data_collected=true;
  }
}


/*
bool status_report(int stat){
  //sending a status variable with mqtt to the status topic
  //returns bool value, while false means no success after a few tries
  #ifdef DEBUG
  Serial.print(F("Asking for instructions stat:"));
  Serial.println(stat);
  #endif
  delay(200);
  bool return_value = true;
  WiFi.forceSleepWake();
  delay(200);
  Serial.setTimeout(5000);
  connect_MQTT();
  //sending sequence
  #ifdef DEBUG
  Serial.println(F("Case 2"));
  #endif
  if(!client.publish(config_status, String(stat).c_str())){
    #ifdef DEBUG
    Serial.print(F("Error: sending status value "));
    Serial.println(stat);
    #endif
    delay(1000);
    int i=0;
    while(!client.publish(config_status, String(2).c_str())){
      if(i>5){
        #ifdef DEBUG
        Serial.print(F("Error: retry sending statusquestion value "));
        Serial.println(stat);
        #endif
        return_value = false;
        break;
      }
      #ifdef DEBUG
      Serial.println(F(" . "));
      #endif
      delay(1000);
      i++;
    }
  }
  else{
    #ifdef DEBUG
    Serial.println(F("Success status: 2 sent!"));
    #endif
  }
  return return_value;
}
*/


void receiveEventnew(unsigned int bytes){
  // configure interupt event for receiving data
  //only important definitions, less important definitions follow later
  byte barr[bytes]; //received byte array
  int i=0;
  while(0<Wire.available()){
    barr[i] = Wire.read();
    #ifdef DEBUG
    Serial.print(F("Bytes received:"));
    Serial.println(barr[i]);
    #endif
    i++;
  }
  int part_i;
  #ifdef DEBUG
  Serial.print("TEST bytes: ");
  Serial.println((byte)bytes);  
  Serial.print("TEST: ");
  Serial.println((int)barr[bytes-1]);
  #endif
  switch((int)barr[bytes-1])
  {
    case 0:
    // Part 1 of datatransmission
      #ifdef DEBUG
      Serial.println(F("Case 1"));
      Serial.print("Part 1: ");
      Serial.println((byte)barr[bytes-1]);
      #endif
      part_i = 0;
      break;
    case 1:
    // Part 2 of datatransmission
      #ifdef DEBUG
      Serial.println(F("Case 1"));
      Serial.println("Part 2: ");
      Serial.println((byte)barr[bytes-1]);
      #endif
      part_i = 14;
      new_data = true; //part 2 received transmission completed
      break;
    case 2:
    //prepare config update (Nano will request it soon) NOT USED right now
      #ifdef DEBUG
      Serial.println(F("Case 2"));
      #endif
      //status_report(2);
      //update_data = true;
      break;
    case 3:
    //permit iic master status
      #ifdef DEBUG
      Serial.println(F("Case 3"));
      #endif
      update_data = true;
      break;
    default:
    // problem case
      #ifdef DEBUG
      Serial.println(F("ERROR! Unknown case: Transmission went wrong or false value"));
      #endif
      break;
  }
  if(((int)barr[bytes-1] == 1) | ((int)barr[bytes-1] == 0)){
    //bring byte array back to int numbers
    int int_datasize=(bytes-1)/2;
    if(int_datasize == 0){
      int_datasize = 1;
    }
    int iarr[int_datasize];  //int conversion array
    int j=0;
    for(int i=0; i<bytes-1; i=i+2){
      iarr[j] = barr[i] | barr [i+1] << 8;
      #ifdef DEBUG
      Serial.print("IIC received: ");
      Serial.print(j);
      Serial.print(" val - ");
      Serial.println(iarr[j]);
      #endif
      j++;
    }
    int part = part_i;
    j=0;
    for(part_i; part_i < part+14; part_i++){
      int_data[part_i]=iarr[j];
      j++;
      #ifdef DEBUG
      Serial.print("--- for loop test ");
      Serial.println(int_data[part_i]);
      #endif
    }
  }
}

//new
byte mqttpub(String topic, String data){
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
    Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
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


void setup() {
  Serial.begin(9600);
  Serial.println("Hello!");
  Wire.begin(0,2,slave_adr);
  Wire.setClockStretchLimit(2000);
  Wire.onReceive(receiveEventnew);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  
}


void loop() {
//------------------------------------------------------------------------------------------------

//start loop
    WiFi.forceSleepWake();
    delay(200);
    Serial.setTimeout(5000);
    connect_MQTT();
    delay(200);
Serial.println(WiFi.status());
if(WiFi.status() == WL_CONNECTED){
  mqttpub(String(testing),String((float)int_data[0]*(float)10));
}

//------------------------------------------------------------------------------------------------
//prepare data
  statusbyte=0;
  //data_collected = false; //only on initial set and then in functions
  if(new_data){
  //if(1){
    new_data=false; //new_data only true when 2nd part of data arives
    //delay(20000); //wait for 2nd part of data
    //int_data[3] is the estimated vcc of arduino nano
    float lsb_ = (float)int_data[3] / 1023.0; //in mV
    int_data[18]= lsb_*1000.0; //debugging remove if not needed
    int_data[19]=int_data[16]; //same
    //calculate actuall voltage Uges --> base formula u2*(r1+r2)/r2
    float u_ges=lsb_*int_data[16]*3.206093023+0.5;  //(152+68.8)/68.8     3.2093023
    //round up data with +.5 and correct error with -100mV
    int_data[16]=u_ges;  //12vbattery
    int_data[17]=constrain(int_data[17],75,1000);
    int_data[17]=map(int_data[17],75,1000,0,100); //photoresistor value
    int_data[20]=int_data[6];
    int_data[21]=int_data[10];
    int_data[22]=int_data[8];
    int_data[23]=int_data[12];
    //map all analog moisture readings in rel. moisture %age
    //try to correct them because they vary a lot with changing VCC of arduino
    for(int i=6; i<16; i++){
      int_data[i]=(float)int_data[i]*lsb_+0.5; //round up data with +.5 and correct error with +100mV
        //elements in array from 6 to 15 come from analog reads by moisture sensors
        int_data[i]=constrain(int_data[i],1221,3176); //x within borders else x = border value; (1221 wet; 3176 dry)
                                                //avoid using other functions inside the brackets of constrain
        int_data[i]=map(int_data[i],1221,3176,100,0); //brings x in range from 0 to 100% rel. moisture;
                                                //constrain because values out of borders are not wanted
    }
    //------------------------------------------------------------------------------------------------
    //send data
    Serial.println("New data!");
    WiFi.forceSleepWake();
    delay(200);
    Serial.setTimeout(5000);
    connect_MQTT();
    delay(200);
    byte error_val = 0;
    //if(0){
    if(WiFi.status() == WL_CONNECTED){
      error_val+=mqttpub(String(pressure_topic_sens2),String((float)int_data[0]*(float)10));
      error_val+=mqttpub(String(humidity_topic_sens2),String((float)int_data[1]/(float)100));
      error_val+=mqttpub(String(temperature_topic_sens2),String((float)int_data[2]/(float)100));
      for(int i = 0; i<max_sens; i++){
        const char* test3;
        String adress = topic_prefix;
        adress += String(i);
        error_val+=mqttpub(adress,String(int_data[i]));
      }
    #ifdef DEBUG
    Serial.print(F("Sent data! Return value: "));
    Serial.println(error_val);
    #endif
    client.disconnect();  // disconnect from the MQTT broker
    }
    else{
      error_val=255;
      #ifdef DEBUG
      Serial.println(F("Error sending data: No connection established"));
      #endif
    }
  }

  //check if nano requests config data update
  //if(true){
  if(update_data){
    update_data=false;
    delay(100);
    bool return_value = true;
    WiFi.forceSleepWake();
    delay(200);
    Serial.setTimeout(5000);
    connect_MQTT();
    delay(200);
    if(!client.publish(config_status, String(2).c_str())){ //the variable doesnt matter function triggers on topic
      #ifdef DEBUG
      Serial.print(F("Error: sending status value "));
      #endif
      int i=0;
      while(!client.publish(config_status, String(2).c_str())){
        if(i>5){
          #ifdef DEBUG
          Serial.print(F("Error: retry sending statusquestion value "));
          #endif
          //return_value = false;
          break;
        }
        #ifdef DEBUG
        Serial.println(F(" . "));
        #endif
        delay(1000);
        i++;
      }
    }
    else{
      #ifdef DEBUG
      Serial.println(F("Success status: 2 sent!"));
      #endif
    }
    unsigned long t_start=millis();
    unsigned long t=0;
    while(t<60000UL){
      t=millis()-t_start;
      client.loop();
      delay(1);
      if(data_collected){
        t+=60000UL;
      }
    }
    if(data_collected){
      data_collected=false;
      #ifdef DEBUG
      Serial.println(F("data collected"));
      #endif
      const int arrsize=(int)max_groups + 2;
      int16_t int16arr[8]={0,0,0,0,0,0,0,0};
      int16arr[0]=(int16_t)groups[0];
      int16arr[1]=(int16_t)groups[1];
      int16arr[2]=(int16_t)groups[2];
      int16arr[3]=(int16_t)groups[3];
      int16arr[4]=(int16_t)groups[4];
      int16arr[5]=(int16_t)groups[5];
      int16arr[6]=(int16_t)sw0;
      int16arr[7]=(int16_t)sw1;
      transmitt_config(int16arr, sizeof(int16arr), (byte)111);
    }
    client.disconnect();
  }
  
  //modem sleep
  WiFi.mode(WIFI_OFF);
  delay(1);
  WiFi.forceSleepBegin();
  Serial.println("WiFi is down");
  delay(5000); //wait for new data
}
