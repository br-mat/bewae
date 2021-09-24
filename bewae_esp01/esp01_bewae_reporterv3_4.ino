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
bool update_data;            //check if nano requests config data update
//bool wifi_status = false;
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

// Custom function to connet to the MQTT broker via WiFi
void connect_MQTT(){
  WiFi.forceSleepWake();
  delay(1);
  WiFi.mode(WIFI_STA);
  delay(1);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Connect to the WiFi
  WiFi.begin(ssid, wifi_password);
  int tries = 0;
  // Wait until the connection has been confirmed before continuing
  while ((WiFi.status() != WL_CONNECTED) && (tries++ < 20)) {
    delay(500);
    Serial.print(" . ");
    if(tries == 10) {
      WiFi.begin(ssid, wifi_password);
      Serial.print(F(" Trying again "));
      Serial.print(tries);
      delay(2500);
    }
  }

  // Debugging - Output the IP Address of the ESP8266
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Connect to MQTT Broker
  // client.connect returns a boolean value to let us know if the connection was successful.
  // If the connection is failing, make sure you are using the correct MQTT Username and Password (Setup Earlier in the Instructable)
  if (client.connect(clientID, mqtt_username, mqtt_password)) {
    Serial.println("Connected to MQTT Broker!");
    client.setCallback(callback);
    client.subscribe(watering_topic, 0); //(topic, qos) qos 0 fire and forget, qos 1 confirm at least once, qos 2 double confirmation reciever
    client.subscribe(bewae_sw, 0);
    client.subscribe(watering_sw, 0);
    client.subscribe(testing, 0);
    client.setCallback(callback);
    //client.loop();
  }
  else {
    Serial.println("Connection to MQTT Broker failed...");
  }
}


//transmitt function
byte transmitt_config(int16_t data[], byte len, byte stat){
  Serial.println("Transmitt process triggered");
  if(len<32+1){ //max 32 bytes
    Serial.println(groups[5]);
    byte rstatus = 0;
    //const int arrsize=sizeof(arr); //first and last byte needed for identification
//    Serial.print("arrsize -2:"); Serial.println(arrsize);
    byte bdata[(max_groups+2)*2+2]={0}; //!!!!! hard coded because len does not work fore some reason "variable size object may not be initialized"
    bdata[sizeof(bdata)-1]=stat;
    //memcpy( &bdata[1], arr[0], tsize);
    memcpy(bdata+1, data, len);
    Wire.beginTransmission(nano_adr);
    //Wire.write(0); //added zero to front, first byte cant read correctly
    Wire.write(bdata, sizeof(bdata));
    //Wire.write(statusbyte); //status to tail end
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


//callback function to receive mqtt data
//watering topic payload rules:
//',' is the sepperator -- no leading ',' -- msg can end with or without ',' -- only int numbers
// -- max_groups should be the same as in code for nano!!!
void callback(char *topic, byte *payload, unsigned int msg_length){
  #ifdef DEBUG
  Serial.print(F("Enter callbacktopic: "));
  Serial.println(topic);
  #endif
  // copy payload to a string
  String msg="";
  if (msg_length > MAX_MSG_LEN){ //limit msg length
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
      if((int)c_index == (int)-1){            //index = -1 means char nor found
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
    data_collected=true; //!!!!!!!!!!!!!!!!!!!! replace later last sent by raspi
  }
  /*
  if(msg_count+1>3){
    data_collected=true;
    statusbyte=111;
  }
  */
}
/*
  #ifdef DEBUG
  Serial.print(F("Message arrived in topic: ");
  Serial.println(topic);
  #endif
  
  static char message[MAX_MSG_LEN+1];
  if (msg_length > MAX_MSG_LEN) {
    msg_length = MAX_MSG_LEN;
  }
  strncpy(message, (char *)payload, msg_length);
  message[msg_length] = '\0';
  #ifdef DEBUG
  Serial.printf("topic %s, message received: %s\n", topic, message);
  #endif
  
  // decode message
  if (strcmp(message, "off") == 0) {
    //setLedState(false);
  } else if (strcmp(message, "on") == 0) {
    //setLedState(true);
  }
*/


bool status_report(int stat){
  //sending a status variable with mqtt to the status topic
  //stat --> int
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


/*
void requestEvent(){
Wire.write(0);
}
*/
/*
//UNFINISHED!!! const int decleration as size not nice?
void requestEvent(){
  //delay(3000);
  // data - FIXED SIZE 14: int array should be packed in max [14];
  // less values will send zeros from esp01 to influxdb
  //int tsize = sizeof data;
  //Serial.println(tsize);
  #ifdef DEBUG
  Serial.println(F("Request triggered"));
  #endif
  int msgdata[8] = {0,1,2,3,4,5,6,7};
  //fill array
  for(int i = 0;i<max_groups;i++){
    //TODO IF override data received!
    msgdata[i] = groups[i];
  }

  msgdata[1] = 1; //TEST VALUE remove later
  msgdata[4] = 255;
  
  msgdata[max_groups] = sw0;
  msgdata[max_groups+1] = sw1;
  const int tsize = 16; //initialize size of array
  Serial.print(F("Sizeof data (tsize):"));
  Serial.println(tsize);
  byte bdata[tsize]={0};
  memcpy(bdata, msgdata, tsize);
  Wire.write(bdata, tsize);
  for(int i=0;i<tsize/2;i++){
    #ifdef DEBUG
    Serial.print(F("int value sent: "));
    Serial.println(msgdata[i]);
    #endif
  }
  Serial.println("temp debug statement requestevent end");
}
*/

// configure interupt event
void receiveEventnew(unsigned int bytes){
  //only important definitions, less important definitions follow later
  //bytes = bytes; //ignore the last byte
  byte barr[bytes]; //received byte array
  int i=0;
  while(0<Wire.available()){
    //reads every byte except the last one
    barr[i] = Wire.read();
    #ifdef DEBUG
    Serial.print(F("Bytes received:"));
    Serial.println(barr[i]);
    #endif
    i++;
  }
  int part_i;
  Serial.print("TEST bytes: ");
  Serial.println((byte)bytes);  
  Serial.print("TEST: ");
  Serial.println((int)barr[bytes-1]);
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
    //prepare config update (Nano will request it soon)
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
    //Serial.print("part: ");
    //Serial.println(part_i);
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
  statusbyte=0;
  //data_collected = false; //only on initial set and then in functions
  if(new_data){
    new_data=false; //new_data only true when 2nd part of data arives
    //delay(20000); //wait for 2nd part of data
    //prepare the data
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
    
    
    Serial.println("New data!");
    WiFi.forceSleepWake();
    delay(200);
    Serial.setTimeout(5000);
    connect_MQTT();
    delay(200);
    
    if (client.publish(pressure_topic_sens2, String((float)int_data[0]*(float)10).c_str())){
      Serial.print("Datapoint sent: ");
      Serial.println(String((float)int_data[0]*(float)10).c_str());
      Serial.print("Adress: ");
      //Serial.println(topic);
      Serial.println(topic_prefix);
    }
    else {
      Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
      client.connect(clientID, mqtt_username, mqtt_password);
      delay(1000); // This delay ensures that client.publish doesn't clash with the client.connect call
      if(!client.publish(pressure_topic_sens2, String((float)int_data[0]*(float)10).c_str())){
        //new_data=true;  //retries every 5 min, old data will get deleted if new arrives
      }
    }
    delay(100);
    if (client.publish(humidity_topic_sens2, String((float)int_data[1]/(float)100).c_str())){
      Serial.print("Datapoint sent: ");
      Serial.println(String((float)int_data[1]/(float)100).c_str());
      Serial.print("Adress: ");
      //Serial.println(topic);
      Serial.println(topic_prefix);
    }
    else {
      Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
      client.connect(clientID, mqtt_username, mqtt_password);
      delay(2000); // This delay ensures that client.publish doesn't clash with the client.connect call
      if(!client.publish(humidity_topic_sens2, String((float)int_data[1]/(float)100).c_str())){
        //new_data=true;  //retries every 5 min, old data will get deleted if new arrives
      }
    }
    delay(100);
    if (client.publish(temperature_topic_sens2, String((float)int_data[2]/(float)100).c_str())){
      Serial.print("Datapoint sent: ");
      Serial.println(String((float)int_data[2]/(float)100).c_str());
      Serial.print("Adress: ");
      //Serial.println(topic);
      Serial.println(topic_prefix);
      }
    else {
      Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
      client.connect(clientID, mqtt_username, mqtt_password);
      delay(2000); // This delay ensures that client.publish doesn't clash with the client.connect call
      if(!client.publish(temperature_topic_sens2, String((float)int_data[2]/(float)100).c_str())){
        //new_data=true;  //retries every 5 min, old data will get deleted if new arrives
      }
    }
    delay(100);


    for(int i = 0; i<max_sens; i++){
      const char* test3;
      String adress = topic_prefix;
      adress += String(i);
      test3 = adress.c_str();
      //https://stackoverflow.com/questions/41201696/how-to-append-const-char-to-a-const-char
      //const char* test2 = topic_prefix+num_array[i];
      if (client.publish(test3, String(int_data[i]).c_str())){
        Serial.print("Datapoint sent: ");
        Serial.println(String(int_data[i]).c_str());
        Serial.print("Adress: ");
        //Serial.println(topic);
        Serial.println(test3);
      }
      else {
        Serial.println(F("Data failed to send. Reconnecting to MQTT Broker and trying again"));
        client.connect(clientID, mqtt_username, mqtt_password);
        delay(1000); // This delay ensures that client.publish doesn't clash with the client.connect call
        if(!client.publish(test3, String(int_data[i]).c_str())){
          //new_data=true;  //retries every 5 min, old data will get deleted if new arrives
        }
      }
      delay(100);
    }
  #ifdef DEBUG
  Serial.println(F("waited 25sec"));
  #endif
  client.disconnect();  // disconnect from the MQTT broker
  }

  //check if nano requests config data update
  //if(true){
  if(update_data){
    update_data=false;
    //sending a status variable with mqtt to the status topic
    //stat --> int
    //returns bool value, while false means no success after a few tries
    int stat = 2; //REPLACE!!!
    #ifdef DEBUG
    Serial.print(F("Asking for instructions stat:"));
    Serial.println(stat);
    #endif
    delay(100);
    bool return_value = true;
    WiFi.forceSleepWake();
    delay(200);
    Serial.setTimeout(5000);
    connect_MQTT();
    delay(200);
    //sending sequence
    #ifdef DEBUG
    Serial.println(F("Case 2"));
    #endif
    if(!client.publish(config_status, String(2).c_str())){
      #ifdef DEBUG
      Serial.print(F("Error: sending status value "));
      Serial.println(stat);
      #endif
      //delay(1000);
      int i=0;
      while(!client.publish(config_status, String(2).c_str())){
        if(i>5){
          #ifdef DEBUG
          Serial.print(F("Error: retry sending statusquestion value "));
          Serial.println(stat);
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
      Serial.println(arrsize);
      int16_t int16arr[8]={0,0,0,0,0,0,0,0};
      Serial.print("Sizeof arr");
      Serial.println(sizeof(int16arr));
      int16arr[0]=(int16_t)groups[0];
      int16arr[1]=(int16_t)groups[1];
      int16arr[2]=(int16_t)groups[2];
      int16arr[3]=(int16_t)groups[3];
      int16arr[4]=(int16_t)groups[4];
      int16arr[5]=(int16_t)groups[5];
      int16arr[6]=(int16_t)sw0;
      int16arr[7]=(int16_t)sw1;
      Serial.print("Sizeof arr");
      Serial.println(sizeof(int16arr));
      transmitt_config(int16arr, sizeof(int16arr), 111);
    }
    client.disconnect();
  }

  
  //modem sleep
  WiFi.mode(WIFI_OFF);
  delay(1);
  WiFi.forceSleepBegin();
  Serial.println("WiFi is down");
  delay(2500); //wait for new data
  //delay(292000); //~5 min (294)sek
}
