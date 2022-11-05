########################################################################################################################
# Python program to handle MQTT traffic regarding config and controll
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

configfile = 'bewae_config.json'

########################################################################################################################
# Classes and Helper functions
########################################################################################################################
# Data class
class SensorData(NamedTuple):
    title: str
    task: str
    value: float
    
# on_connect handler
def on_connect(client, userdata, flags, rc):
    """ The callback for when the client receives a CONNACK response from the server."""
    print('Connected with result code ' + str(rc))
    client.subscribe(MQTT_TOPIC)

# mqtt message handler
def _parse_mqtt_message(topic, payload, client):
    #return data class object containing information to publish or none
    match = re.match(MQTT_REGEX, topic)
    if match:
        title = match.group(1)
        task = match.group(2)
        print(task)
        
        #some exceptions should be ignored
        if task in ['config', 'status', 'comms', 'test', 'tester']:
            return None
        
        #planed watering hours passed as lst [7, 10, 19]
        elif task == 'planed':
            payload=payload.replace(" ", "")
            lst=payload.split(",")
            hours = [1<<int(x) for x in lst if int(x) in list(range(0,24))]
            plan_long = sum(hours)
            print(float(plan_long))
            return SensorData(title, 'timetable', float(plan_long))
        
        elif task == 'timetable':
            try:
                float(payload)
            except ValueError:
                return None
            return SensorData(title, task, float(payload))
        
        elif task == 'comms':
            pass
        
        elif task == 'water-time':
            # set water time value and update config data
            with open('bewae_config.json') as json_file:
                dict1 = json.load(json_file)
            dict1['group'][title]['measurment'] = int(payload)
            os.remove(configfile)
            with open(configfile, "w") as out_file:
                json.dump(dict1, out_file, indent = 4)

        elif task == 'config_status':
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
                client.publish('home/bewae/comms', mqtt_msg.replace(" ", "0")) 
            # switches
            for entry in list(data['switches'].keys()):
                mqtt_msg = 'S'
                mqtt_msg += F"{int(entry[2]):16d}"
                mqtt_msg += f"{int(data['switches'][entry]['value']):16d}"
                client.publish('home/bewae/comms', mqtt_msg.replace(" ", "0"))
            # timetable(s?)
            # hint arduino int 2 byte! python int 4 bytes!
            client.publish('home/bewae/comms', 'T'+ f"{int(data['timetable'][0]):32d}".replace(" ", "0"))
            return None

        else: #final case
            print(f'Warning: Ignored message, due to incorrect message.')
    else:
        return None


# publish config data via mqtt
def _send_config_data(config_data):
    json_body = [
        {
            'task': config_data.task,
            'tags': {
                'title': config_data.title
            },
            'fields': {
                'value': config_data.value
            }
        }
    ]
    ###############!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    # TODO IMPLEMENT MQTT PUB FUNCTION USING CONFIG DATA CLASS!
    pass

# message handler
def on_message(client, userdata, msg):
    print(msg.topic + ' ' + str(msg.payload))
    config_data = _parse_mqtt_message(msg.topic, msg.payload.decode('utf-8'), client)
    if config_data is not None:
        _send_config_data(config_data)
        
# on_publish handler
def on_publish(client, userdata, result):
    print('data published \n')
    pass
        
########################################################################################################################
    # main
########################################################################################################################
def main():
    mqtt_client = mqtt.Client(MQTT_CLIENT_ID)
    mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)

    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.on_publish = on_publish
    
    mqtt_client.connect(MQTT_ADDRESS, 1883)
    mqtt_client.loop_forever()


if __name__ == '__main__':
    print('MQTT to Config bridge')
    main()