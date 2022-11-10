########################################################################################################################
# Python program to handle MQTT traffic regarding commands and controll
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
MQTT_CLIENT_ID = 'MQTTCommsBridge' #ID SHOULD BE UNIQUE

########################################################################################################################
# setup & get arguments
########################################################################################################################

# setup

configfile = '/home/pi/py_scripts/bewae_config.json'
valid_tasks = ['set-water-time', 'set-switch', 'set-timetable', 'config-status']

parser = argparse.ArgumentParser(description='Program writes measurements data to specified influx db.')
# Add arguments
parser.add_argument('-l','--log', type=bool, help='log on/off', required=False, default=False)
parser.add_argument('-d', '--debug', type=bool,
                    help='details turned on will print everything, off will only output warnings',
                    required=False, default=False)
parser.add_argument('-sn', '--sname', type=str, help='session name, defines mqtt client name it should be unique.',
                    required=False, default=MQTT_CLIENT_ID)
# Array of all arguments passed to script
args=parser.parse_args()

# on true for debuging
args.log=True
args.debug=True

#init logging
if args.debug:
    logging.basicConfig(filename="/home/pi/py_scripts/log/bewae_config.log", level=logging.DEBUG, format="%(asctime)s:%(levelname)s:%(message)s")
else:
    logging.basicConfig(filename="/home/pi/py_scripts/log/bewae_config.log", level=logging.INFO, format="%(asctime)s:%(levelname)s:%(message)s")

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
    logging.debug('Connected with result code ' + str(rc))
    client.subscribe(MQTT_TOPIC)

# mqtt message handler
def _parse_mqtt_message(topic, payload, client):
    #return data class object containing information to publish or none
    match = re.match(MQTT_REGEX, topic)
    #print(topic)
    if match:
        title = match.group(2)
        task = match.group(1)
        #print(task)
        
        # check for valid task
        if task not in valid_tasks:
            logging.warning(f'Warning: Ignored, sent incorrect task!')
            return None
        
        # veriyfy payload value
        if not payload.isdigit():
            logging.warning(f'Warning: Wrong payload number!')
            return None

        # task section
        elif task == 'set-water-time':
            # set water time value and update JSON config data
            #print('hit watertime')
            #print(title)
            #print(payload)
            # load config
            with open(configfile) as json_file:
                dict1 = json.load(json_file)
            # sanity check
            if title not in list(dict1['group'].keys()):
                logging.warning(f'Warning: Incorrect title sent!')
                return None
            dict1['group'][title]['water-time'] = int(payload)
            os.remove(configfile)
            with open(configfile, "w") as out_file:
                json.dump(dict1, out_file, indent = 4)
            return None
        
        elif task == 'set-switch':
            # set water time value and update JSON config data
            #print('hit watertime')
            #print(title)
            #print(payload)
            # load config
            with open(configfile) as json_file:
                dict1 = json.load(json_file)
            # sanity check
            if title not in list(dict1['switches'].keys()):
                logging.warning(f'Warning: Incorrect title sent!')
                return None
            dict1['switches'][title]['value'] = int(payload)
            os.remove(configfile)
            with open(configfile, "w") as out_file:
                json.dump(dict1, out_file, indent = 4)
            return None
                
        elif task == 'set-timetable':
            # set water time value and update JSON config data
            #print('hit watertime')
            #print(title)
            #print(payload)
            # load config
            with open(configfile) as json_file:
                dict1 = json.load(json_file)
            #format payload
            payload = payload.replace(" ", "")
            regex = r"([a-zA-Z0-9]+):([0-9|,]+|master)"
            rematch = re.match(regex, test_str)
            if rematch is not None:
                name = rematch[1]
                timeslots = rematch[2].replace(",", " ").strip().split()
                # sanity check
                if title not in list(dict1['switches'].keys()):
                    logging.warning(f'Warning: Incorrect title sent!')
                    return None
                dict1['group'][name]['timetable'] = int(payload)
                os.remove(configfile)
                with open(configfile, "w") as out_file:
                    json.dump(dict1, out_file, indent = 4)
                return None
            else:
                logging.warning('Warning: wrong message format.')
                #TODO: not return None in case of warning
                return None

        elif task == 'config-status':
            # answer config status request from esp32
            # open JSON config file read and send all data
            #print('hit config status')
            # load config
            with open('bewae_config.json') as json_file:
                data = json.load(json_file)
            # message command format (letter) + (32 digits)
            # watering groups
            for entry in list(data['group'].keys()):
                mqtt_msg = 'W'
                mqtt_msg += f"{int(entry):16d}"
                mqtt_msg += f"{int(data['group'][entry]['water-time']):16d}"
                #print(mqtt_msg)
                client.publish('home/bewae/comms', mqtt_msg.replace(" ", "0")) 
            # switches
            for entry in list(data['switches'].keys()):
                mqtt_msg = 'S'
                #TODO: come up with more universal solution instead of picking 3rd char as id number
                mqtt_msg += f"{int(entry[2]):16d}"
                mqtt_msg += f"{int(data['switches'][entry]['value']):16d}"
                #print(mqtt_msg)
                client.publish('home/bewae/comms', mqtt_msg.replace(" ", "0"))
            # timetable(s?)
            # hint arduino int 2 byte! python int 4 bytes!
            for entry in list(data['group'].keys()):
                mqtt_msg = 'T'
                mqtt_msg += f"{int(entry):8d}"
                mqtt_msg += f"{int(data['group'][entry]['water-time']):24d}"
                #print(mqtt_msg)
                client.publish('home/bewae/comms', mqtt_msg.replace(" ", "0")) 
            #print(f"{int(data['timetable'][0]):32d}")
            return None

    else:
        logging.warning("Warning: Wrong message format, Regex didn't find anything.")
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
    logging.debug(msg.topic + ' ' + str(msg.payload))
    config_data = _parse_mqtt_message(msg.topic, msg.payload.decode('utf-8'), client)
    if config_data is not None:
        _send_config_data(config_data)
        
# on_publish handler
def on_publish(client, userdata, result):
    #print('data published \n' + str(userdata) + '\n' + str(result) + '\n')
    pass
        
########################################################################################################################
    # main
########################################################################################################################
def main():
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
    logging.info(f'Start MQTT to Config bridge started as {args.sname}')
    main()
