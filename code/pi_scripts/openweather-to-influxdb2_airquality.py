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

LAT=99.99 # latitude
LON=99.99 # longitude
API_KEY='******************************' #"Your API Key"

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, 8086, INFLUXDB_USER, INFLUXDB_PASSWORD, None)

@dataclass
class Data():
    latitude: float
    longitude: float
    city: str
    measurement_key: str
    request: dict

def to_influxdb(sensor_data):
    data={}
    for key in sensor_data.request[sensor_data.measurement_key].keys():
        if isinstance(sensor_data.request[sensor_data.measurement_key][key], (int, float)):
            data.update({key:float(sensor_data.request[sensor_data.measurement_key][key])})
        elif isinstance(sensor_data.request[sensor_data.measurement_key][key], list):
            #data.update(sensor_data.request[sensor_data.measurement_key][key][0])
            pass #ignore weather condition related data for now

    json_body = [
        {
            'measurement': sensor_data.measurement_key,
            'tags': {
                'location': sensor_data.city
            },
            'fields': data
        }
    ]
    print(data)
    influxdb_client.write_points(json_body)
    
def _init_influxdb_database():
    databases = influxdb_client.get_list_database()
    if len(list(filter(lambda x: x['name'] == INFLUXDB_DATABASE, databases))) == 0:
        influxdb_client.create_database(INFLUXDB_DATABASE)
    influxdb_client.switch_database(INFLUXDB_DATABASE)
    
def get_city(lat, lon, api_key):
    URL = f"http://api.openweathermap.org/geo/1.0/reverse?lat={lat}&lon={lon}&limit={1}&appid={api_key}"
    response = requests.get(URL)
    name = None
    if response.status_code == 200:
        try:
            name = response.json()[0]['name'] #unpack list
        except IndexError:
            print("Sorry! IndexError while unpacking response.")
        except KeyError:
            print("Sorry! KeyError while unpacking response.")
        finally:
            print(f"Location set: {name}")
    else:
        print(f"Warning city request failed! Location: {name}")
    return name

def main():
    _init_influxdb_database()
    # request
    lat=LAT
    lon=LON
    excl='minutely,hourly,daily' #exclude 'currently,minutley,hourly,daily'
    key=API_KEY
    URL = f"http://api.openweathermap.org/data/2.5/air_pollution?lat={lat}&lon={lon}&appid={key}"

    # request location name
    city = get_city(lat, lon, key)    
    # request data
    response = requests.get(URL)
    # checking the status code of the request
    if response.status_code == 200:
        # getting data in the json format
        data = response.json()
        data = data['list'][0] # unpack data to fit old structure
        current_set=Data(lat, lon, city, 'components', data)
        #print(current_set)
        to_influxdb(current_set)
    else:
        # print warning message
        print("Error: HTTP request not responding")
        
    #terminate client
    influxdb_client.close()


if __name__ == '__main__':
    print('OpenweatherAPI to InfluxDB')
    main()