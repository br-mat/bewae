# Bewae - Irrigation Project v3.3

v3.3 final versions are not stable (work in progress).

## About

Automated Sensor-Controlled Irrigation with Raspberry Pi & Arduino
<br>

**Objectives:**

+ Sensor-Controlled Automated Irrigation of Balcony Plants
+ Storage & Visualization of Sensor Data
+ Remote Monitoring & Control
+ (Future) Weather Data Query & Irrigation with Machine Learning

## Contents:

- [Introduction (EN)](/README_EN.md)
- [System Control](#system-controll)
- [Setup](#setup)
- [Details](#details)
- [Images & Development](#images--development)

## Introduction (DE)
This version is more of a test version. <br>
The previous documentation is not a step-by-step guide. It requires a certain level of knowledge in Linux and microcontroller usage. The goal is to provide an overview of the project and facilitate its reconstruction in the spring or at new locations. <br>

### Description

An ESP-32 is used for control. Various sensors and modules collect a variety of data, including soil moisture, temperature, humidity, air pressure data, and sunlight duration. This data is sent to a Raspberry Pi using an InfluxDB client and stored in InfluxDB 2.0. <br>

Irrigation follows a schedule, which can either work with pre-programmed values or be controlled over the network. By setting up a VPN (e.g., PiVPN), you can also monitor and irrigate your plants remotely. <br>

**Power Supply:**
The system is equipped with a small PV module, a lead-acid battery, and a small 100L water tank. Depending on the temperature, you only need to remember to fill the tank once a week. <br>

## System Diagram

### V3.3 Figure:

 System Setup:                  | Solar Setup:
:-------------------------:|:-------------------------:
![System](/docs/pictures/SystemdiagrammV3_3.png "System Diagram 3.3") | ![Solar](/docs/pictures/systemdiagramSolar.png "Solar Diagram 3.3")
(Shows a rough outline of the system)

## System Control

### config.JSON

This file contains all relevant settings. On the controller, this file is stored in flash. To make it easier to respond to changes, this file can be made available using Node-Red (on a Raspberry Pi) within the network. For this purpose, the address in connection.h needs to be adjusted.

Note: It is also possible to use this file without connecting to a Raspberry Pi, but it would need to be re-uploaded for changes.

### Irrigation Control

Irrigation is controlled using the settings in the config.JSON file. Two variables are used - timetable and water-time:

- 'timetable': Represented by the first 24 bits of an integer, with each bit representing an hour of the day. To save space, it is stored as a long integer.
- 'water-time': The time (in seconds) that the respective group is irrigated. Multiple valves or valve + pump can be set simultaneously.

<br>

```cpp
// Example Schedule                           23211917151311 9 7 5 3 1
//                                            | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                             | | | | | | | | | | | |
// Decimal representation: 1049600            22201816141210 8 6 4 2 0
//
```

The control itself can be carried out in the following ways:

- Configuration file: Either locally in Flash (SPIFFS) or provided via Node-Red.
- (currently not implemented) MQTT over Wi-Fi (App or Pi) (not yet re-implemented)

<br>

In the file, irrigation can be configured as shown in the example below:

```json
// JSON group template
{"group": {
    "Tomatoes": {         // name of grp
    "is_set":1,           // indicates status
    "vpins":[0,5],        // list of pins bound to group
    "water-time":10,      // water time in seconds
    "timetable":1049600,  // timetable (first 24 bit)
    "lastup":[0,0],       // runtime variable - dont touch!
    "watering":0          // runtime variable - dont touch!
    },
    .
    .
    .
  }
}
```

### Sensor Configuration:

Sensors can be modified at any time using 'config.JSON'. Several measurement functions with common sensors are implemented. You can also make changes to modify returned values or output relative measurement values (%). <br>

To correctly configure a sensor, it is necessary to use a unique ID as a JSON key. Under this key, 'name', 'field', and 'mode' must be assigned. 'name' and 'field' correspond to the entry in the database where the measurement value can be found in InfluxDB. Implemented 'mode' variants can be found in the table below; it may be necessary to include additional items such as 'pin', etc.

Example Configuration:


```json
  "sensor": {
    "id00": {
      "name": "bme280",
      "field": "temp",
      "mode": "bmetemp"
    },
    .
    .
    .
    "id03": {
      "name": "Soil",
      "field": "temp",
      "mode": "soiltemp"
    },
    .
    .
    .
    "id08": {
      "name": "Soil",
      "field": "moisture",
      "mode": "vanalog",
      "vpin": 15,
      "hlim": 600,
      "llim": 250
    }
  },
  .
  .
  .
```

<br>

## Configuration & Modifiers:
| Property     | Description        |
|--------------|--------------------|
| `vpin`       | Defines a virtual pin as input, necessary for advanced measurement functions    |
| `pin`        | Defines a regular analog pin as input    |
| `add`        | (OPTIONAL) Adds a constant to the result, optional, applied only when >0     |
| `fac`        | (OPTIONAL) Multiplies a constant with the result, optional, applied only when >0     |
| `hlim`       | (OPTIONAL) Upper limit for relative measurement (%), optional, considered only when >0   |
| `llim`       | (OPTIONAL) Lower limit for relative measurement (%), optional, considered only when >0    |

<br>

Measurement Modes: <br>
| Measuring Mode | Description            | Requirements           |
|----------------|------------------------|------------------------|
| `analog`       | Reads an analog pin    | 'pin' configured in config.JSON      |
| `vanalog`      | Reads a virtual analog pin | 'vpin' configured in config.JSON    |
| `bmetemp`      | Reads temperature from a bme280 Sensor at the default address      | Connected BME280 |
| `bmehum`       | Reads humidity from a bme280 Sensor at the default address       | Connected BME280 |
| `bmepress`     | Reads pressure from a bme280 Sensor at the default address`     | Connected BME280 |
| `soiltemp`     | Reads temperature from a ds18b20 sensor at the default 1-wire pin     | Board needs to have a 1-wire pin & connected Sensor |

<br>

## Setup Guide

### Microcontroller ESP32:

[ESP32 Code](./code/esp_32ard_bewae/) <br>

#### PlatformIO IDE

First, ensure that PlatformIO is installed in Visual Studio Code. To do this, press the 'Extensions' icon in the left taskbar and install 'PlatformIO IDE'. <br>

Then, open the project folder 'bewae' in VS Code. Next, connect your ESP-32 controller. To upload the code to the ESP32, click the arrow symbol in the PlatformIO taskbar. The device should be automatically recognized, but it may be necessary to press the boot button during the upload process. Furthermore, it is important to upload the 'Filesystem,' which can be done using the 'Upload Filesystem Image' sub-option of the PlatformIO extension. <br>

#### Config & WIFI (Optional)

Now, it's essential to enter your credentials for the local network in the 'connection.h' files. <br>
Also, you would need to update the IP of the system where Node-Red and the database are installed, along with the associated data. <br>
Additionally, the address of the time server may need to be changed.  <br>

In case nothing has been entered, the program should still work. However, adjusting the settings would be more challenging since the FileSystem needs to be re-uploaded each time there are changes. Additionally, the time must be set manually! For this purpose, there is a 'set_time' function in the helper class. <br>

Notes:
Don't forget to configure the hardware. To do this, adapt the used helper class to the hardware board used in 'Helper.h' and 'main.cpp'. <br>

### Raspberry Pi

InfluxDB 2.0 requires a 64-bit OS. Therefore, I'm using a Raspberry Pi 4B with a full 64-bit image.

#### Node-Red

How to [install](https://nodered.org/docs/getting-started/raspberrypi). <br>

In the browser, go to Your.Raspberry.Pi.IP:1880 to create or log in as a user. Then, you can import the [flow](/node-red-flows/flows.json) via the web interface. <br>
Open the flow and double-click on the orange 'read config' node. Change the path to match your configuration file.

#### InfluxDB 2.0

How to [install](https://docs.influxdata.com/influxdb/v2/install/?t=Raspberry+Pi). <br>
Influx can be set up in two ways, either through the UI or the CLI. You can find descriptions online (install link):

Set up InfluxDB through the UI
- With InfluxDB running, visit http://localhost:8086
- Click Get Started
- Set up your initial user
- Enter a Username for your initial user.
- Enter a Password and Confirm Password for your user.
- Enter your initial Organization Name.
- Enter your initial Bucket Name.
- Click Continue.

Permissions for buckets can be easily configured through the UI, and a token can be generated for that.

- In the 'Load Data' option, click on 'API TOKENS'
- Click on 'Generate API TOKEN' -> Custom API Token
- Enter a token description and choose permissions (Write) -> Click 'Generate'
- Replace the InfluxDB token in the 'connection.h' file

---

Now everything should be prepared. The configuration file ('config.JSON) can be adjusted as needed by configuring the required functions (see tables) as shown in the example. <br>

If a relay module is used with inverted logic, there is an option to invert the outputs of the shift register by setting INVERT_SHIFTOUT to true in the 'config.h' file. Additionally, flags can be set to facilitate debugging. <br>

## Details

### Circuit Boards

The circuits were created with Fritzing, and the breadboard view provides a good overview and is ideal for prototypes. However, for larger projects, Fritzing may not be ideal. Gerber files are available. For the Fritzing files, only the board view is relevant. All boards use the ESP-32 microcontroller. <br>


 Board 1:                  | Board 3:                  | Board 5:
:-------------------------:|:-------------------------:|:-------------------------:
![Board1](/docs/pictures/bewae3_3_board1v3_838_Leiterplatte.png) | ![Board3](/docs/pictures/bewae3_3_board3v22_Leiterplatte.png) | ![Board5](/docs/pictures/bewae3_3_board5v5_final_Leiterplatte.png)
Main Board | Extension Board | Main Board

#### **bewae3_3_board1v3_838.fzz** Main Board (PCB)

Large main board. Room for up to 16 analog sensors as well as BME280 and RTC module via I2C. 8 pins for valves/pumps (expandable). These can be easily used either via relays or PCB Board 3.<br>

#### **bewae3_3_board3v22.fzz** as Extension (PCB)

Intended as an extension. It aims to provide slots for sensors and additional consumers. To expand the 8 pins of the main board, a shift register can be used. A total of 16 slots for sensors including power supply, as well as 2 pumps (*12V*) and 10 smaller valves (*12V*).<br>

#### **bewae3_3_board5v5_final.fzz** as Main Board (PCB)

Smaller main board. Avoids a large number of sensor connections. Room for 2x 5V sensors, 2x 3V sensors, 1x LDR, I2C bus, 1-Wire bus. BME280 and RTC module are also provided.<br>

### Current Setup

- ESP32 Board 1 & 3; ESP32 Board 5
- Raspberry Pi 4 B (4GB)
- 10 Valves 12V
- 2 Pumps (currently only one 12V in operation)
- 16 Analog/Digital pins for sensors & other measurements (soil moisture, photoresistor, etc.)
- BME280 Temperature/Humidity/Pressure Sensor
- RTC DS3231 Real-Time Clock
- Irrigation Kit: Hoses, Sprinklers, etc.
- 20W 12V Solar Panel
- 12 Ah 12V Lead-Acid Battery
- Solar Charge Controller

## Images & Development

**Development:** <br>
The project originated from a multi-week absence during which the balcony plants would have been without care. The idea was to keep the plants alive using an irrigation kit, hoses, a few nozzles, and a pump. However, a regular irrigation like a timer seemed too boring, and my interest in a small DIY project was piqued. Capacitive soil moisture sensors and 4 small 12V valves were quickly ordered, as I had enough Arduinos at home. Within a week, [Version 1](#v1) was born, and the survival of the plants was assured. Out of necessity, a project emerged that kept me busy for a while, and through continuous expansion and improvement, it fulfills much more than originally planned. <br>
<br>

To conclude, here is a collection of photos from various versions of the project that have been created over time:

### V3

![Bild](/docs/pictures/bewaeV3(Sommer).jpg) <br>
![Bild](/docs/pictures/bewaeV3(Herbst).jpg) <br>
![Bild](/docs/pictures/bewaeV3(Box).jpg) <br>
![Bild](/docs/pictures/MainPCB.jpg) <br>

### V2

![Bild](/docs/pictures/bewaeV2.jpg) <br>

### V1

![Bild](/docs/pictures/bewaeV1(2).jpg) <br>
![Bild](/docs/pictures/bewaeV1.jpg) <br>