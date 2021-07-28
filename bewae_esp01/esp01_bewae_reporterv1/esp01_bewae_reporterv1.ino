#include "PubSubClient.h" // Connect and publish to the MQTT broker
#include <Wire.h>

#include <ESP8266WiFi.h> // Enables the ESP01 to connect to the local network (via WiFi)


// iic
byte slave_adr = 9;
bool newdata = false;
const int max_values = 20;
int data[max_values]; //mqtt reporting array

// WiFi
const char* ssid = "A1-08e52c";         // Your personal network SSID
const char* wifi_password = "mabritck"; // Your personal network password

// MQTT
const char* mqtt_server = "10.0.0.18";  // IP of the MQTT broker
const char* topic_prefix = "home/sens3/";
const char* mqtt_username = "mbraun"; // MQTT username
const char* mqtt_password = "wdsaibuii123"; // MQTT password
const char* clientID = "client_sens3"; // MQTT client ID

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

// configure interupt event
void receiveEvent(int bytes){
  byte barr[bytes]; 
  newdata = true;
  int i=0;
  while(1<Wire.available()){
    byte d = Wire.read(); 
    barr[i] = d;
    i++;
  }
  int j=0;
  if(bytes > max_values*2){
    bytes=max_values*2;
  }
  for(int i=0; i<bytes; i=i+2){
    data[j] = barr[i] | barr [i+1] << 8;
    j++;
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wire.begin(slave_adr);
}

void loop() {
  // put your main code here, to run repeatedly:
if(newdata){
  connect_MQTT();
  Serial.setTimeout(2000);
  delay(200);

  for(int i = 0; i<max_values; i++){
    const char* topic = topic_prefix + (char)i;
    if (client.publish(topic, String(data[i]).c_str())){
      Serial.println("Datapoint sent!");
      Serial.println(data[i]);
    }
    else {
    Serial.println(F("Temperature failed to send. Reconnecting to MQTT Broker and trying again"));
    client.connect(clientID, mqtt_username, mqtt_password);
    delay(500); // This delay ensures that client.publish doesn't clash with the client.connect call
    client.publish(topic_prefix + (char)i, String(data[i]).c_str());
    }
  }
  
  client.disconnect();  // disconnect from the MQTT broker
  newdata=false;
}
//modem sleep
WiFi.mode(WIFI_OFF);
WiFi.forceSleepBegin();
Serial.println("WiFi is down");
delay(292000); //~5 min (294)sek
 

WiFi.forceSleepWake();
delay(1);

}
