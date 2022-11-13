#!/bin/bash

#give influx service startup time
sleep 180s

cd /home/pi/py_scripts
sudo /usr/bin/python3 MQTTInfluxBridge.py -l=True -d=True -sn=mqttdatabridge
