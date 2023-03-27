This is a small roadmap to help me remember what I am working on.

Changes:
- Added OpenWeather scripts (saving current conditions and air condition to InfluxDB)
- Added corresponding .sh launcher files
- Added ESP32 build
- Added a second circuit to handle pumps and solenoids
- Updated (and tested) main circuit, corrected bugs and problems on ESP32 build
- Updated English translation of new version
- Added a new IrrigationController class
- Implemented a config file system using SPIFFS
- Implemented HTTP GET functionality to update config file
- Added Node-RED flow to provide config file

To-do:
- Add OpenWeather forecast script to the Pi
- Add dynamic watering controlled by Raspberry Pi
- Rework circuit board 2
- Add option to upload config file via bluetooth
- Updated documentation (new config. node-red)

Future ideas:
- Add display and interface menu
- Using machine learning to controll watering process based on collected data
- Implement an app for configuration and visualizing basic data

Open problems:
- Sensor rail needs 5V, not 3V! (Currently hooked onto logic switch)
- Backup SD card module malfunctioning
- Reverse voltage protection on 12V IN gets very hot (bridged for now, calculation was wrong?)