
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
- [system diagram](#systemdiagram)
- [control](#control-of-irrigation)
- [details](#details)
  * [boards](#boards)
  * [Code ESP 32](#code-esp32)
  * [RaspberryPi](#raspberrypi)
- [Images & Creation](#images--creation)

## Introduction (EN)
This version is to be seen more as a test version. <br>
The current processing is not a step-by-step guide. It requires some basic knowledge of working with Linux and microcontrollers. The purpose is merely to provide an insight into the project and facilitate its reconstruction in spring or at new locations. Therefore, I have opted for German, but I also plan to add an English version. <br>

## Systemdiagramm

### V3.3 Diagram:

 System Setup:                  | Solar Setup:
:-------------------------:|:-------------------------:
![System](/docs/pictures/SystemdiagrammV3_3.png "Systemdiagramm 3.3") | ![Solar](/docs/pictures/systemdiagramSolar.png "Solar diagramm 3.3")
(zeigt grobe Skizzierung des Systems)

### Current Setup:
- ESP32
- Raspberry Pi 4 B (4GB)
- 6 Valves 12V
- 2 Pumps (currently only one 12V pump in operation)
- 16 Analog/Digital Pins for sensors & other measurements (soil moisture, photoresistor, etc.)
- BME280 Temperature/Humidity/Pressure Sensor
- RTC DS3231 Real-Time Clock
- Irrigation Kit: Hoses, Sprinklers, etc.
- 20W 12V Solar Panel
- 12Ah 12V Lead-Acid Battery
- Solar Charge Controller

### Description
An ESP-32 is used for control. Various sensors and modules record a variety of data, including soil moisture, temperature, humidity, air pressure, and duration of sunlight. This data is sent to a Raspberry Pi using an InfluxDB client and stored in InfluxDB 2.0. <br>

The irrigation follows a schedule that can either work with pre-programmed values or be controlled over the network. By setting up a VPN (e.g. PiVPN), you can also monitor and water your plants from remote locations. <br>

**Current Status:** <br>

The system is equipped with a small PV module, a lead-acid battery, and a small 100l water tank. Depending on the temperature, the tank only needs to be filled once a week. <br>


## System diagram

### V3.3 preview:
![System](/docs/pictures/Systemdiagramm3_3.png "Systemdiagramm 3.3")

## Irrigation Control

The irrigation is controlled using the settings in the config.JSON file. Two variables are used for this purpose - timetable and water-time:

- timetable: It is represented by the first 24 bits of an integer, with each bit corresponding to an hour of the day. To save space, it is stored as a long integer.
- water-time: This is the time (in seconds) for which the respective group is watered. Multiple valves or valve + pump combinations can be set simultaneously.

<br>

```cpp
// Example timetable                      23211917151311 9 7 5 3 1
//                                       | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                       | | | | | | | | | | | |
// Decimal representation: 1049600      22201816141210 8 6 4 2 0
//
```
The control can be done in the following ways:

MQTT over W-LAN (phone or Pi) (not yet re-implemented)
Config file in Flash (SPIFFS) if no network was found

### config.JSON

This file contains all relevant settings. Thanks to Node-Red, it is provided by the Raspberry Pi within the network and can be queried by the microcontroller. For this purpose, the address in connection.h must be adjusted. <br>
Thus, the irrigation can be influenced by changes to the two mentioned variables. <br>

```json
// JSON group template
{"group": {
    "Tomatoes": {         // Group name
    "is_set": 1,           // Indicates status
    "vpins": [0, 5],        // List of pins bound to the group
    "lastup": [0, 0],       // Runtime variable - do not touch!
    "watering": 0,         // Runtime variable - do not touch!
    "water-time": 10,      // Water time in seconds
    "timetable": 1049600   // Timetable (first 24 bits)
    },
    .
    .
    .
  }
}
```

### Node-Red Flow:

The used Node-Red flow is located in the node-red-flows folder. If necessary, change the address or path to the file.

# Details

## Boards

The circuits were created using Fritzing, and the breadboard view provides a good overview and is ideal for prototypes. However, Fritzing may not be the most suitable option for larger projects. Gerber files are available. For the Fritzing files, only the board view is relevant. All boards use the ESP-32 as the microcontroller. <br>

 Board 1:                  | Board 3:                  | Board 5:
:-------------------------:|:-------------------------:|:-------------------------:
![Board1](/docs/pictures/bewae3_3_board1v3_838_Leiterplatte.png) | ![Board3](/docs/pictures/bewae3_3_board3v22_Leiterplatte.png) | ![Board5](/docs/pictures/bewae3_3_board5v5_final_Leiterplatte.png)

### **bewae3_3_board1v3_838.fzz** Main Board (PCB)
Large main board. Space for up to 16 analog sensors, as well as BME280 and RTC module via I2C. 8 pins for valves/pumps (expandable). These can be used either with relays or directly with the PCB Board 3.<br>

### **bewae3_3_board3v22.fzz** as an Extension (PCB)
Designed as an extension. Its purpose is to provide slots for sensors and other devices. The shift register can be used to expand the 8 pins of the main board. A total of 16 slots for sensors including power supply, as well as 2 pins for pumps (*12V*) and 10 smaller valves (*12V*).<br>

### **bewae3_3_board5v5_final.fzz** as Main Board (PCB)
Smaller main board. Dispenses with a large number of sensor connections. Space for 2x 5V sensors, 2x 3V sensors, 1x LDR, I2C bus, and 1-Wire bus. BME280 and RTC module are also provided.<br>


## Code ESP32
[Arduino-Nano Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>

### How to deploy the code:
First, make sure you have PlatformIO installed in Visual Studio Code. Then, open the project folder in VS Code. Next, plug in your ESP-32 Controller and click on the arrow icon in the PlatformIO taskbar to upload your code. The device should be detected automatically. <br>

Before executing the program on the controller, it will check if some devices are responding, so be sure to connect them properly. <br>

Don’t forget to update your connection settings. Simply change the ‘connection.h’ file within the src folder to match your (network) configuration. <br>

### Change time of RTC:

Make sure to set the time of the RTC either setup a code example for this device or uncomment my function in the setup. If my approach is used don’t forget to comment it again otherwise time will be reset every reboot. <br>

## Raspberrypi

### Node-Red

TODO: update documentation

### InfluxDB 2.0

TODO: update documentation

### Grafana
![grafana dashboard example](/docs/pictures/grafanarainyday.png "Grafana rainy day") <br>

Again, there are very good [tutorials](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) to fall back on, so I won't go into detail about the installation process. Grafana can be used to display the collected data in nice plots. So monitoring the moisture levels of the plants is very easy. <br>

Once Grafana is installed, the [json](/grafana_dashboard/bewaeMonitor.json) export can be imported via the web interface. <br>

Under 'Configuration' you have to enter the 'Datasources'. Here you must now make sure to use the same databases that you create and in which the data is stored. In my case this would be ***main*** for all data of the irrigation. As well as ***pidb*** for the CPU temperature on the RaspberryPi which is included in the import, since this is purely optional and has nothing to do with the irrigation I will not go into it further. The panels if they are not used you can simply remove. <br>

![Datasource configuration](/docs/pictures/datasources.png "Datasource configuration example") <br>

### MQTT (OUTDATED!!)

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

#### mqttdash app (optional)
On my **Android** smartphone I have downloaded the app mqttdash. This is very easy and intuitive to use, you have to enter the address user and password created above and can then configure the topics. It is important that only **int** values can be sent. If you have more than 6 groups you have to adjust the variable max_groups again in every place in all programs. All topics sent via MQTT and entered as 'measurement' 'water_time' are forwarded to the ESP-01 via MQTT and entered into the database. By the entry 'location' they can be distinguished therefore it is recommended to use the same name and a numbering because the 'locations' are sorted and sent in ascending order from the ESP-01 via MQTT to the Arduino Nano.

An example screenshot from the app, the numbers stand for the time (s) in which the respective valve is open and water is pumped (approx. 0.7l/min):
mqttdash app <br>

## Pictures & Development

**Development:** <br>
The project emerged from a several-week absence during which the balcony plants would have been left without care. The idea was to keep the greens alive using an irrigation set, hoses, a few nozzles, and a pump. However, a regular watering schedule like a timer seemed too boring, and it sparked my interest in a small DIY project. Capacitive soil moisture sensors and 4 small *12V* valves were quickly ordered, as I had enough Arduinos at home. Within a week, [Version 1](#v1) came to life, securing the survival of the plants. It evolved from a necessity into a project that kept me busy for a while, and through continuous expansion and improvement, it now fulfills much more than originally planned. <br>

Finally, here is a small collection of photos depicting various versions of the project taken over time: <br>

### V3

![Image](/docs/pictures/bewaeV3(summer).jpg) <br>
![Image](/docs/pictures/bewaeV3(autumn).jpg) <br>
![Image](/docs/pictures/bewaeV3(Box).jpg) <br>
![Image](/docs/pictures/MainPCB.jpg) <br>

### V2

![Image](/docs/pictures/bewaeV2.jpg) <br>

### V1

![Image](/docs/pictures/bewaeV1(2).jpg) <br>
![Image](/docs/pictures/bewaeV1.jpg) <br>
