# Python program to handle MQTT traffic
#
# no waranty for the program
#
# Copyright (C) 2022  br-mat

# importing the modules
import re
import json
import os
from typing import NamedTuple

import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient

import time


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
MQTT_CLIENT_ID = 'MQTTInfluxDBBridge'

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, 8086, INFLUXDB_USER, INFLUXDB_PASSWORD, None)

########################################################################################################################
# Classes and Helper functions
########################################################################################################################
# Data class
class SensorData(NamedTuple):
    location: str
    measurement: str
    value: float

# on_connect handler
def on_connect(client, userdata, flags, rc):
    """ The callback for when the client receives a CONNACK response from the server."""
    print('Connected with result code ' + str(rc))
    client.subscribe(MQTT_TOPIC)

# on_publish handler
def on_publish(client, userdata, result):
    print('data published \n')
    pass

# mqtt message handler
def _parse_mqtt_message(topic, payload, client):
    match = re.match(MQTT_REGEX, topic)
    if match:
        location = match.group(1)
        measurement = match.group(2)
        print(measurement)
        #some exceptions should not be parsed and stored in influxdb
        if measurement in ['config','status','comms', 'test', 'tester']:
            return None
        #planed watering hours passed as lst [7, 10, 19]
        elif measurement == 'planed':
            payload=payload.replace(" ", "")
            lst=payload.split(",")
            hours = [1<<int(x) for x in lst if int(x) in list(range(0,24))]
            plan_long = sum(hours)
            print(float(plan_long))
            return SensorData(location, 'timetable', float(plan_long))
        elif measurement == 'timetable':
            try:
                float(payload)
            except ValueError:
                return None
            return SensorData(location, measurement, float(payload))
        elif measurement == 'comms':
            pass
        elif measurement == 'config_status':
            # answer config status request from esp32
            # open JSON config file read and send all data
            with open('bewae_config.json') as json_file:
                data = json.load(json_file)
            # message command format (letter) + (32 digits)
            # watering groups
            for entry in list(data['group'].keys()):
                mqtt_msg = 'W'
                mqtt_msg += f"{int(data['group'][entry]['VPin']):16d}"
                mqtt_msg += f"{int(data['group'][entry]['Time']):16d}"
                client.publish('home/bewae/comms', mqtt_msg) 
            # switches
            for entry in list(data['switches'].keys()):
                mqtt_msg = 'S'
                mqtt_msg += F"{entry[2]:16d}"
                mqtt_msg += f"{int(data['group'][entry]['value']):16d}"
                client.publish('home/bewae/comms', mqtt_msg)
            # timetable(s?)
            # hint arduino int 2 byte! python int 4 bytes!
            client.publish('home/bewae/comms', 'T'+ f"{int(data['timetable'][0]):32d}")
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
    """The callback for when a PUBLISH message is received from the server."""
    print(msg.topic + ' ' + str(msg.payload))
    sensor_data = _parse_mqtt_message(msg.topic, msg.payload.decode('utf-8'), client)
    if sensor_data is not None:
        _send_sensor_data_to_influxdb(sensor_data)

def _init_influxdb_database():
    databases = influxdb_client.get_list_database()
    if len(list(filter(lambda x: x['name'] == INFLUXDB_DATABASE, databases))) == 0:
        influxdb_client.create_database(INFLUXDB_DATABASE)
    influxdb_client.switch_database(INFLUXDB_DATABASE)

# config handler (outdated? using JSON format now)
def config(name, filename, method, value=None):
    #this function should be able to read and set elements in a config file
    #it only accepts numeric values
    #type checks
    for arg in [name, filename, method]:
        if not isinstance(arg, str):
            raise TypeError(f'Wrong argument: {arg} is not str.')
    name = name.strip(' ')
    
    #hint: escape handles special characters in string
    RE = r"(" + re.escape(name) + r"\s*=)(\s*[a-zA-Z0-9]*)"
    pat = re.compile(RE)
    with open(filename, 'r') as f:
        content = f.read()
    if method == 'get':
        return pat.search(content).group(2)
        
    if method == 'set':
        #sanity checks
        if value is None:
            raise ValueError(f'No value to change passed.')
        if not isinstance(value, (int, float)):
            raise ValueError(f'Not a valid number, must be int float.')
        change=pat.sub(pat.search(content).group(1)+str(value), content)
        with open(filename, 'w') as f:
            f.write(change)
            print(change)
        return None

########################################################################################################################
    # main
    ########################################################################################################################

def main():
    _init_influxdb_database()
    mqtt_client = mqtt.Client(MQTT_CLIENT_ID)
    mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.on_publish = on_publish
    
    mqtt_client.connect(MQTT_ADDRESS, 1883)
    mqtt_client.loop_forever()


if __name__ == '__main__':
    print('MQTT to InfluxDB bridge')
    main()