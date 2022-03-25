# openweather api test
####################################################################################################
# Autor: br-mat                       br-mat (c) 2022
# contact: matthiasbraun@gmx.at
# Openweather API request data and store within influxdb
# set crontab to execute this hourly
####################################################################################################

####################################################################################################
# Imports
####################################################################################################
from dataclasses import dataclass
import requests, json
from influxdb import InfluxDBClient

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


@dataclass
class Data():
    latitude: float
    longitude: float
    city: str
    measurement_key: str
    request: dict

def to_influxdb(sensor_data):
    json_body = [
        {
            'measurement_key': sensor_data.measurement_key,
            'tags': {
                'location': sensor_data.city
            },
            'fields': sensor_data.request
        }
    ]
    influxdb_client.write_points(json_body)

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, 8086, INFLUXDB_USER, INFLUXDB_PASSWORD, None)

def main():
    # request
    LAT=99.99 # latitude
    LON=99.99 # longitude
    EXCL='minutely,hourly,daily' #exclude 'currently,minutley,hourly,daily'
    API_KEY='b05760615139c231301ec828d011e7b1' #"Your API Key"
    URL = f"https://api.openweathermap.org/data/2.5/onecall?lat={LAT}&lon={LON}&exclude={EXCL}&appid={API_KEY}"

    # HTTP request
    response = requests.get(URL)
    # checking the status code of the request
    if response.status_code == 200:
        # getting data in the json format
        data = response.json()
        current_set=Data(LAT, LON, 'City', 'current', data)
        to_influxdb(current_set)
        print(current_set.request)
        #print(data)
    else:
        # showing the error message
        print("Error: HTTP request not responding")


if __name__ == '__main__':
    print('OpenweatherAPI to InfluxDB')
    main()
