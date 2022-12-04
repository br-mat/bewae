########################################################################################################################
# Python program to handle MQTT and save significant data to InfluxDB
#
# ARGUMENTS
# -l log specifies if log should be saved into a file
# -d debug specifies verbose of log
# -sn session name specifies the name of the mqtt client, should be unique
#
# ABOUT
# This script should be called on reboot, i provided a basic shell script to launch it from crontab.
# Be aware that it should be delayed as some error occurs with influxdb if started too early.
# It creats a log into a specified file.
# The mqtt-topics used to transmitt data to the pi should be home/*/*
# purpose is to read incomming data and store them into influx database,
# if relevant.
#
# no waranty for the program
#
# Copyright (C) 2022  br-mat
########################################################################################################################

########################################################################################################################
# Imports
########################################################################################################################

import re
import json
import os
from typing import NamedTuple

import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient

from datetime import datetime
import time
import argparse
import logging

########################################################################################################################
# conection details
########################################################################################################################

INFLUXDB_ADDRESS = 'raspberrypi' #hostname or IP
INFLUXDB_USER = '*********'
INFLUXDB_PASSWORD = '*********'
INFLUXDB_DATABASE = 'main'

MQTT_ADDRESS = 'raspberrypi' #hostname or IP
MQTT_USER = '*********'
MQTT_PASSWORD = '*********'
MQTT_TOPIC = 'home/+/+'
MQTT_REGEX = 'home/([^/]+)/([^/]+)'
MQTT_CLIENT_ID = 'MQTTInfluxBridge' #ID SHOULD BE UNIQUE

########################################################################################################################
# setup & get arguments
########################################################################################################################

# setup

configfile = '/home/pi/py_scripts/bewae_config.json'

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, 8086, INFLUXDB_USER, INFLUXDB_PASSWORD, None)

# get arguments

parser = argparse.ArgumentParser(description='Program writes measurements data to specified influx db.')
# Add arguments
parser.add_argument('-l','--log', type=bool, help='log to file on/off', required=False, default=False)
parser.add_argument('-d', '--debug', type=bool,
                    help='details turned on will print everything, off will only output warnings',
                    required=False, default=False)
parser.add_argument('-sn', '--sname', type=str, help='session name, defines mqtt client name it should be unique.',
                    required=False, default=MQTT_CLIENT_ID)
# Array of all arguments passed to script
args=parser.parse_args()

# turn on for debuging
#args.log=True
#args.details=True

#init logging
if args.debug:
    logging.basicConfig(filename="/home/pi/py_scripts/log/mqtt_data.log", level=logging.DEBUG, format="%(asctime)s:%(levelname)s:%(message)s")
else:
    logging.basicConfig(filename="/home/pi/py_scripts/log/mqtt_data.log", level=logging.INFO, format="%(asctime)s:%(levelname)s:%(message)s")
if not args.log:
    logging.disable(level=CRITICAL)

########################################################################################################################
# Classes and Helper functions
########################################################################################################################

# data class
class SensorData(NamedTuple):
    location: str
    measurement: str
    value: float

# on_connect handler
def on_connect(client, userdata, flags, rc):
    """ The callback for when the client receives a CONNACK response from the server."""
    logging.debug('Connected with result code ' + str(rc))
    client.subscribe(MQTT_TOPIC)
    
# on_publish handler
def on_publish(client, userdata, result):
    logging.debug('data published \n')
    pass

# mqtt message handler
def _parse_mqtt_message(topic, payload, client):
    match = re.match(MQTT_REGEX, topic)
    if match:
        #base = match.group(0)
        location = match.group(1)
        measurement = match.group(2)
        #operation = match.group(3)
        logging.debug(f' incomming {location} {measurement} : {payload}')
        
        #some exceptions should not be parsed and stored in influxdb
        if measurement in ['config','status','comms', 'test', 'tester', 'water-time', 'timetable', 'config_status']:
            return None
        
        else: #real data
            return SensorData(location, measurement, float(payload))
    else:
        return None

# saving data to influxDB
def _send_sensor_data_to_influxdb(sensor_data):
    json_body = [
        {
            'measurement': sensor_data.measurement,
            'tags': {
                'location': sensor_data.location
            },
            'fields': {
                'value': sensor_data.value
            }
        }
    ]
    influxdb_client.write_points(json_body)

# message handler
def on_message(client, userdata, msg):
    # callback when a PUBLISH msg is received
    logging.debug(msg.topic + ' ' + str(msg.payload))
    sensor_data = _parse_mqtt_message(msg.topic, msg.payload.decode('utf-8'), client)
    if sensor_data is not None:
        _send_sensor_data_to_influxdb(sensor_data)

def _init_influxdb_database():
    databases = influxdb_client.get_list_database()
    if len(list(filter(lambda x: x['name'] == INFLUXDB_DATABASE, databases))) == 0:
        influxdb_client.create_database(INFLUXDB_DATABASE)
    influxdb_client.switch_database(INFLUXDB_DATABASE)

########################################################################################################################
    # main
########################################################################################################################

def main():
    #TODO: write handler for error case, influxdb seems to take long time for startup after boot process,
    #      so the following call can result in an error. Which would stop the script eventually. BAD.
    #    temporary solved by just shifting back start of this script within launcher1.sh script
    _init_influxdb_database()
    
    mqtt_client = mqtt.Client(args.sname,
                              transport = 'tcp',
                              protocol = mqtt.MQTTv311,
                              clean_session=True,
                              )
    mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.on_publish = on_publish
    
    mqtt_client.connect(MQTT_ADDRESS, 1883)
    mqtt_client.loop_forever()


if __name__ == '__main__':
    logging.info(f'Start MQTT to InfluxDB bridge as {args.sname}')
    main()

