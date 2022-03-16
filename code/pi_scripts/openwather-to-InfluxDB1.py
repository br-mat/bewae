# openweather api test
####################################################################################################
# Autor: br-mat                       br-mat (c) 2022
# contact: matthiasbraun@gmx.at
# request Openweather API data and store within influxdb
####################################################################################################

####################################################################################################
# Imports
####################################################################################################
from dataclasses import dataclass
import requests
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
    LAT=XX.XX # latitude
    LON=XX.XX # longitude
    EXCL='minutely,hourly,daily' #exclude 'currently,minutley,hourly,daily'
    API_KEY='b05760615139c231301ec828d011e7b1' #"Your API Key"
    URL = f"https://api.openweathermap.org/data/2.5/onecall?lat={LAT}&lon={LON}&exclude={EXCL}&appid={API_KEY}"

    # HTTP request
    response = requests.get(URL)
    # checking the status code of the request
    if response.status_code == 200:
        # getting data in the json format
        data = response.json()
        #print(data)
    else:
        # showing the error message
        print("Error in the HTTP request")

    current_set=Data(LAT, LON, '****', 'current', data)
    #to_influxdb(current_set)
    print(current_set.request)

if __name__ == '__main__':
    print('OpenweatherAPI to InfluxDB')
    main()