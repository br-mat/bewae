//IIC SLAVE MQTT REPORTER
#include "PubSubClient.h" // Connect and publish to the MQTT broker
#include <Wire.h>
#include <String.h>

// Code for the ESP8266
#include <ESP8266WiFi.h> // Enables the ESP to connect to the local network (via WiFi)

// iic
#define slave_adr 0x08


// WiFi
const char* ssid = "A1-08e52c";         // Your personal network SSID
const char* wifi_password = "mabritck"; // Your personal network password

// MQTT
const char* mqtt_server = "10.0.0.18";  // IP of the MQTT broker
const char* topic_prefix_original="home/test/temperature";
String topic_prefix = "home/sens3/z";
const char* mqtt_username = "mbraun"; // MQTT username
const char* mqtt_password = "wdsaibuii123"; // MQTT password
const char* clientID = "client_test"; // MQTT client ID
const char* humidity_topic_sens2 = "home/sens2/humidity";
const char* temperature_topic_sens2 = "home/sens2/temperature";
const char* pressure_topic_sens2 = "home/sens2/pressure";

bool new_data = false;
const int max_sens=28;
int int_data[max_sens] = {0}; //keep it as int array, in case all this is migrated to a single mcu and storage space becomes relevant again
                              //data stay comparable as a whole

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
  }
  else {
    Serial.println("Connection to MQTT Broker failed...");
  }
}
/*
// configure interupt event
void receiveEvent(unsigned int bytes){
  byte val=Wire.read();
  new_data = true;
}
*/

void receiveEvent(unsigned int bytes){
  bytes = bytes -1; //ignore the last byte
  int int_datasize=(bytes-1)/2;
  byte barr[bytes];        //recieved byte array
  int iarr[int_datasize];
  
  int i=0;
  //Serial.print("\nWireavailable = ");
  //Serial.println(Wire.available());
  while(1<Wire.available()){ 
    barr[i] = Wire.read();
    Serial.print("Datapoints:");
    Serial.println(barr[i]);
    i++;
  }
  //determine which part of transmition is handled
  Serial.print("bytes: ");
  Serial.println(bytes);
  Serial.print("last byte: ");
  Serial.println((byte)barr[bytes-1]);
  Serial.println(barr[bytes]);
  int part_i;
  if((byte)barr[bytes-1] == (byte)0){
    part_i = 0 ;
    Serial.print("Part 1: ");
    Serial.println((byte)barr[bytes-1]);
  }
  else if((byte)barr[bytes-1] == (byte)1){
    part_i = 14;
    Serial.println("Part 2: ");
    Serial.println((byte)barr[bytes-1]);
    new_data = true; //part 2 recieved transmission completed
  }
  else{
    Serial.println(F("Transmission incomplete! Some Error occured!"));
  }

  //bring byte array back to int numbers
  int j=0;
  for(int i=0; i<bytes-1; i=i+2){
    iarr[j] = barr[i] | barr [i+1] << 8;
    Serial.print("IIC recieved: ");
    Serial.print(j);
    Serial.print(" val - ");
    Serial.println(iarr[j]);
    j++;
  }
  //Serial.print("part: ");
  //Serial.println(part_i);
  int part = part_i;
  j=0;
  for(part_i; part_i < part+14; part_i++){
    int_data[part_i]=iarr[j];
    j++;
    Serial.print("--- for loop test ");
    Serial.println(int_data[part_i]);
  }
}


void setup() {
  Serial.begin(9600);
  Serial.println("Hello!");
  //Wire.pins(0,2);
  Wire.begin(0,2,slave_adr);
  //Wire.setClockStretchLimit(2000);
  Wire.onReceive(receiveEvent);
}

void loop() {
  if(new_data){
    new_data=false;
    delay(20000); //wait for 2nd part of data

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
  
  client.disconnect();  // disconnect from the MQTT broker
  }
  //modem sleep
  WiFi.mode(WIFI_OFF);
  delay(1);
  WiFi.forceSleepBegin();
  Serial.println("WiFi is down");
  delay(20000); //wait for new data
  //delay(292000); //~5 min (294)sek
}
