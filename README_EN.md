
# Bewae - irrigation project v3.3

v3.3 WIP versions now are not stable

- added openweather scripts (saving current conditions & air condition to influxDB)
- added corresponding . sh launcher files
- added ESP32 build
- added 2nd circuit handling pumps & solenoids
- updated (and tested) main circuit, corrected bugs & problems on ESP32 build
- updated english translation of new version
- updated documentation

todo:
- add openwather forecast script
- add dynamic watering controlled by Pi (using ML later)
- reimplement SD card module?
- implement config file using SPIFFS
- add new configuration file feature to replace default programming settings

open problems:
- sensor rail needs 5V not 3V! (hooked onto logic switch for now)
- backup SD card module malfunctions
- reverse voltage protection on 12V IN get really hot (bridged for now)

### About

Automated Sensor Controlled Irrigation with Raspberry Pi & Arduino <br>
**Targets:**
+ Sensor controlled Automated watering of the balcony plants.
+ Store & display sensor data
+ Monitoring & control on the go
+ (In the future) Query weather data & irrigation with machine learning.

## Content:
- [Introduction (EN)](#introduction-(en))
- [Introduction (EN)](#introduction-(en))
  * [Current build](#currentbuild)
  * [description](#description)
  * [system diagram](#systemdiagram)
- [control](#control-of-irrigation)
- [details](#details)
  * [boards](#boards)
  * [Code Arduino Nano](#code-arduino-nano)
  * [code ESP8266-01](#code-esp8266-01)
  * [RaspberryPi](#raspberrypi)
- [Images & Creation](#images--creation)

## Introduction (EN)
This version is more of a test version. It has been reworked to fit an ESP32 board. <br>
This is **not** a step-by-step guide - a little basic knowledge in dealing with Linux and microcontrollers is required. It should only be given an insight into the project and make it easier for me to rebuild in spring or at new locations. That's why I decided to use German, but I would like to add an English version later. <br>

## Einleitung (DE)
Diese Version ist eher als Testversion zu sehen. <br>
Bei der bisherigen Aufarbeitung handelt es sich **nicht** um eine Step-by-step Anleitung es ist ein wenig Grundwissen im Umgang mit Linux und Mikrokontrollern vorausgesetzt. Es soll lediglich ein Einblick in das Projekt gegeben werden und mir den Wiederaufbau im Frühjahr oder an neuen Standorten erleichtern. Deshalb habe ich mich auch für Deutsch entschieden, möchte jedoch noch eine englische Version ergänzen. <br>

### current structure
- ESP32
- RaspberryPi 4 B (*4GB*)
- *6* Valves *12V
- *2* pumps (currently only one *12V* in operation)
- *16* Analog/digital pins for sensors & other measurements (soil moisture, photoresistor, etc.)
- BME280 Temperature/Humidity/Pressure Sensor
- RTC DS3231 Real Time Clock
- micro SD module & micro SD card
- Irrigation kit hoses, sprinklers, etc.
- *10W 12V* Solar Panel
- *12 Ah* *12V* lead acid battery
- Solar charge controller

### Description
Overview of the circuit structure in the attached [*systemdiagram.png*](#systemdiagram) as block diagram. An [*ESP-32*](https://de.wikipedia.org/wiki/ESP32) is used for control. Sensors and modules record lots of data, including soil moisture sensors, temperature, humidity and barometric pressure data, and sunshine duration. The data is sent to the [RaspberryPi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) via [*MQTT*](#mqtt), stored there, and displayed as nice graphs thanks to Grafana. <br> 
 The irrigation follows a schedule. Which either works with programmed values or can be controlled via the network. With a [*MQTT*](#mqtt) messaging client it is possible to intervene in the irrigation. With an established [*VPN*](https://www.pivpn.io/) it is possible to keep an eye on your plants and water them even when you are on the road.
The other folders contain the code for both controllers as well as the json export for the Grafana Dashboard and the Python scripts for processing the [*MQTT*](#mqtt) messages. The database used is [*InfluxDB*](#influxdb) where all sent data is stored. All relevant programs and scripts start automatically thanks to crontab on every boot. <br>

<br>

**Status now:** < br>

**Technique:** For easier handling a plan for a [PCB](#boards) was created and printed with [fritzing](https://fritzing.org/). Sensor data from various sensors (soil moisture, temperature, etc.) are sent to a [RaspberryPi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) where they are stored in a database. Irrigation control works either automated sensor-controlled or via a [MQTT-messaging app](#mqttdash-app-optional) via smartphone. Thanks to [VPN](https://www.pivpn.io/), control and monitoring are also possible while on the road. <br>

**In the meantime, it has become an autonomously functioning system, equipped with a small PV module, a lead battery and a small *100l* water tank. Depending on the temperature, it is now only necessary to remember to fill the tank once a week.
<br>

## System diagram

### V3.3 preview:
![System](/docs/pictures/Systemdiagramm3_3.png "Systemdiagramm 3.3")

## Irrigation control

To control the irrigation system 2 variables are used, timetable and water-time:
-timetable is represented by a 4 bytelong int number, where every bit stands for a clock hour. The 8 most significant bits represent the group id. It is possible to set an individual timetable for each group.
-Water time is used to set the active time in seconds for the specified group (solenoid and pump). It can be set individually for each group.
The control itself can be set in 3 ways:
-Programming the microcontroller
-MQTT via W-LAN (phone or pi)
-Config file on SD-card (to be added)

### Control via MQTT (phone or pi):

To be able to set something via MQTT the commands must be sent under the correct topic. These are conf/set-timetable/group-id and conf/set-water-time/group-id. <br>

#### **timetable**:

To set the timetable: 
```
conf/set-timetable/gerneral
```
The watering values can be changed as follows:
```
#water group 0 at 7, 10 and 18 o'clock
0:7,10,18
#water group 2 at same time as master group (group 0)
2:master
```
Important:
Group 0 is the "master" group, so group 0 can be set with the code master. The group ID must start at 0 and be ascending because these numbers are used as indices for the call on the microcontroller. The values sent with the code represent the full hours to be cast. A "," is accepted as a separator. It is also possible to adjust the group to the master group with the string "master", "off" to turn off the circuit (not implemented yet).

#### **water-time**:

To set the water-time variable of the individual groups this scheme should be used:
```
conf/set-water-time/groupID
```
Important: Pay attention to the ID that is also defined on the microcontroller for the respective group. The ID's must start at 0 and be ascending. The value is set in seconds that the valves operate.
<br>

Enter group: | Overview:
:-------------------------:|:-------------------------:
![config](/docs/pictures/mqtt-app.jpg) | ![config](/docs/pictures/watering-config.jpg)

### control via programming (default):

Open main.cpp file and search for the two code snippets below, there you can change the values directly in code. <br>

Timetable: 
```
// change timetable 2523211917151311 9 7 5 3 1
// | | | | | | | | | | | | |
unsigned long int timetable_default = 0b000000000100000000000000;
// | | | | | | | | | | | | |
// 2422201816141210 8 6 4 2 0
```

<br>

Water-time, where the last 3 are just important during run time and should be set zero. With the first boolean entry we can switch this group on/off. The v-pin and pump pin depend on the setup at the boards.

```
// change water-time
// is_set, v-pin, pump_pin, name, watering-time default, timetable, watering base, watering time, last act,
solenoid group[max_groups] =
{ 
  {true, 0, pump1, "Tom1", 100, 0.0f, 0, 0, 0}, //group0 tomatoes 1
  {true, 1, pump1, "Tom2", 80, 0.0f, 0, 0, 0}, //group1 tomatoes 2
  {true, 2, pump1, "Gewa", 30, 0.0f, 0, 0, 0}, //group2 greenhouse
  {true, 3, pump1, "Chil", 40, 0.0f, 0, 0, 0}, //group3 Chillis
  {true, 6, pump1, "Krtr", 20, 0.0f, 0, 0, 0}, //group4 herbs
  {true, 7, pump1, "earthb", 50, 0.0f, 0, 0, 0}, //group5 strawberries
}
```
<br>

# Details

## Circuit boards
The circuits were created with fritzing, the plug-in board view offers a good overview and is ideal for prototypes. From a certain size of the project, however, fritzing is no longer ideal. <br>

Board 1: | Board 2:
:-------------------------:|:-------------------------:
![config](/docs/pictures/bewae3_3_board1v3_7_board.png) | ![config](/docs/pictures/bewae3_3_board2v6_board.png)

### **bewae3_3_board1v3_7.fzz** Motherboard (PCB)
Has the purpose to control the system and to collect all data. All relevant sensors and modules as well as controls are combined on this board and processed by an [*ESP-32*](https://de.wikipedia.org/wiki/ESP32).

<br>

### **bewae3_3_board2v6.fzz** as extension (PCB)
It is intended as an extension. The board offers 13 slots for sensors as well as 2 pumps (*12V*) and 6 valves (*12V*). The voltage of the lead battery (accumulator) should be able to be tapped via the voltage divider.
<br> 
This board could be individually adapted to meet changed requirements. For example, instead of the complex (but economical) logic circuitry to control the transistors, relays could be used to switch the individual components.
<br>

## Code ESP32
[Arduino-Nano Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>

### General
The code is served up using Visual Studio Code and the Platformio extension, VS code provides a comprehensive IDE and is excellent for larger projects. <br>

For the program to run, the RTC module must be properly connected and working. The SD card, the BME280 and network control can be deactivated via config. Simply comment out the definitions in the *config.h* file. <br>

For the control and communication via the network to work, the WLAN settings must be configured correctly. Here, only the data of the respective network must be filled in. <br>

For debugging purposes it is recommended to tap RX and TX pins on the microcontroller and set the *DEBUG* definition. <br>

## Raspberrypi
### General
Personally, I am currently using a [**RaspberryPi 4B**](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) with *4GB* Ram on which a [*VPN*](https://www.pivpn.io/) (**wireguard**) and [*Pihole*](https://pi-hole.net/) are also installed. Through the [*VPN*](https://www.pivpn.io/) I can also access the data on the road or if necessary intervene in the irrigation. I do not go into further detail on the installation, but it is relatively simple and well described in numerous tutorials. <br>For the operation is also sufficient the availability in the local network, here are necessary:
- SSid (network name): ExampleNetworkXY
- Password: Example passwordXY
- IP or hostname of the RaspberryPi (IP statically assigned)
For easier configuration either screen, ssh, teamviewer, vnc viewer ... on the Raspberrypi also on these points I do not go further, because they are good to find elsewhere. <br>

### InfluxDB
If influx has not been installed yet, after a short search there are lots of useful tutorials e.g.: [(pimylifeup)](https://pimylifeup.com/raspberry-pi-influxdb/).
<br>

To set up the database, we first start by creating a user. To do this, start InfluxDB by entering the following line in the terminal.
```
influx
```
Influx should now have started, now continue to enter into the interface:
```
CREATE USER "username" WITH PASSWORD "password"
CREATE DATABASE "database
GRANT ALL ON "username" TO "database".
```
This ***username*** as well as ***password*** and the ***database*** must be filled in the microcontroller code.
<br>
To get out of this "interface" you simply have to enter the command **exit** and you are back in the terminal.
```
exit
```

### Grafana
![grafana dashboard example](/docs/pictures/grafanarainyday.png "Grafana rainy day") <br>

Again, there are very good [tutorials](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) to fall back on, so I won't go into detail about the installation process. Grafana can be used to display the collected data in nice plots. So monitoring the moisture levels of the plants is very easy. <br>

Once Grafana is installed, the [json](/grafana_dashboard/bewaeMonitor.json) export can be imported via the web interface. <br>

Under 'Configuration' you have to enter the 'Datasources'. Here you must now make sure to use the same databases that you create and in which the data is stored. In my case this would be ***main*** for all data of the irrigation. As well as ***pidb*** for the CPU temperature on the RaspberryPi which is included in the import, since this is purely optional and has nothing to do with the irrigation I will not go into it further. The panels if they are not used you can simply remove. <br>

![Datasource configuration](/docs/pictures/datasources.png "Datasource configuration example") <br>

### MQTT&Python

#### MQTT
**MQTT** (Message Queuing Telemetry Transport) Is an open network protocol for machine-to-machine (M2M) communication that enables the transmission of telemetry data in the form of messages between devices. How it works [(link)](http://www.steves-internet-guide.com/mqtt-works/). <br>
The sent messages are composed of topic and payload. Topics are used for easy assignment and have a fixed structure in my case:
```
#topic:
exampletopic=home/location/measurement
```
In addition to this topic, the data is appended in the payload, the content as a string. Listing of relevant topics:
```
#example topics to publish data on
  humidity_topic_sens2 = "home/sens2/humidity";
  #bme280 sensor read

  temperature_topic_sens2 = "home/sens2/temperature";
  #bme280 sensor read

  pressure_topic_sens2 = "home/sens2/pressure";
  #bme280 sensor read

  config_status = "home/bewae/config_status";
  #signaling RaspberryPi to repost relevant data so ESP can receive it

#subsciption topics to receive data
  watering_topic = "home/bewae/config";
  #bewae config recive watering instructions (as csv)
```

#### Installation
There is also a [link](https://pimylifeup.com/raspberry-pi-mosquitto-mqtt-server/) here.
Username and password must be adjusted again in all places in the code.

#### Python
To read or write the data sent via [*MQTT*](#mqtt) into the database the Python script [MQTTInfluxDBBridge3.py](/code/pi_scripts/MQTTInfluxDBBridge3.py) is used. The script itself comes from a [tutorial](https://diyi0t.com/visualize-mqtt-data-with-influxdb-and-grafana/) and was adapted to fit the requirements in my project. The Python code can be started with the shell script [launcher1.sh](/code/pi_scripts/launcher1.sh) automatically with crontab at every boot. Since the Pi takes some time to start everything error-free at boot time, I delay the start of the script by *20* seconds. <br>
To avoid errors only **int** values should be sent via [*MQTT*](#mqtt) (*2* **byte**), the data type **int** is *2* **byte** large on the [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano). <br>
```
sudo crontab -e
```
Opens cron with any editor to now enter the desired programs. As an example:
```
@reboot /path/file.sh
```
Additional [information](https://pimylifeup.com/cron-jobs-and-crontab/) to cron.

#### mqttdash app (optional)
On my **Android** smartphone I have downloaded the app [mqttdash](https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=de_AT&gl=US). This is very easy and intuitive to use, you have to enter the address user and password created above and can then configure the topics. It is important that only **int** values can be sent. If you have more than *6* groups you have to adjust the variable max_groups again in every place in all programs. All topics sent via [*MQTT*](#mqtt) and entered as *'measurement'* *'water_time'* are forwarded to the [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) via [*MQTT*](#mqtt) and entered into the database. By the entry *'location'* they can be distinguished therefore it is recommended to use the same name and a numbering because the *'locations'* are sorted and sent in ascending order from the [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) via [*MQTT*](#mqtt) to the [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano).

An example screenshot from the app, the numbers stand for the time (s) in which the respective valve is open and water is pumped (approx. 0.7l/min):
![mqttdash app](/docs/pictures/mqttdash.jpg) <br>

## Pictures & Origin

**Evolution:** < br>
The project itself arose from an absence of several weeks in which the balcony plants would have been without supply. The green should be kept alive with a watering kit, hoses and a few nozzles and a pump. A regular watering like a timer seemed too boring to me and my interest in a small tinkering project was aroused. Capacitive soil moisture sensors and 4 small *12V* valves were quickly ordered, Arduinos I had enough at home, so it came within a week to [version 1](#v1) and the survival of the plants was secured. Out of necessity a project was born that kept me busy for a while, continuously extended and improved it fulfills far more than first planned. <br>
<br>

Finally, a small collection of photos of several versions of the project that have been created over time:

### V3

![Image](/docs/pictures/bewaeV3(summer). jpg) <br>
![Image](/docs/pictures/bewaeV3(autumn). jpg) <br>
![Image](/docs/pictures/bewaeV3(Box). jpg) <br>
![Image](/docs/pictures/MainPCB.jpg) <br>

### V2

![Image](/docs/pictures/bewaeV2.jpg) <br>

### V1

![Image](/docs/pictures/bewaeV1(2).jpg) <br>
![Image](/docs/pictures/bewaeV1.jpg) <br>
