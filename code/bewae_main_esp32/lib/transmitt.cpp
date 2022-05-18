#include <headers.h>

// WiFi
const char* ssid = "XXXXXXXXX";         // Your personal network SSID
const char* wifi_password = "XXXXXXXXX"; // Your personal network password

// MQTT
const char* mqtt_server = "XXXXXXXXX";  // IP of the MQTT broker
//const char* topic_prefix_original="home/test/temperature";
String topic_prefix = "home/sens3/z";
const char* mqtt_username = "XXXXXXXXX"; // MQTT username
const char* mqtt_password = "XXXXXXXXX"; // MQTT password
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
/*
// 1883 is the listener port for the Broker
PubSubClient client(mqtt_server, 1883, wifiClient);

*/
/*
byte transmitt(){
  WiFi.begin(ssid, wifi_password);
  return 0;
}
*/