import re
from typing import NamedTuple

import paho.mqtt.client as mqtt
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

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, 8086, INFLUXDB_USER, INFLUXDB_PASSWORD, None)

class SensorData(NamedTuple):
    location: str
    measurement: str
    value: float

def on_connect(client, userdata, flags, rc):
    """ The callback for when the client receives a CONNACK response from the server."""
    print('Connected with result code ' + str(rc))
    client.subscribe(MQTT_TOPIC)

def on_publish(client, userdata, result):
    print('data published \n')
    pass

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
            hours = [int(x) for x in lst if int(x) in list(range(0,24))]
            plan_long = sum(hours)
            return SensorData(location, 'timetable', float(plan_long))
        elif measurement == 'timetable':
            try:
                float(payload)
            except ValueError:
                return None
            return SensorData(location, measurement, float(payload))
        elif measurement == 'config_status':
            # answer & repost config stats (bool switches & water_time configuration)
            # search for latest topics and repost them too update bewae on config
            query = influxdb_client.query("SELECT * from water_time LIMIT 1000") #limiting reduce time taken to gather data
            if query is None:
                return None
            #print(list(query.get_points(measurement='water_time')))
            #print(len(list(query.get_points(measurement='water_time'))))
            query_l=list(query.get_points(measurement='water_time'))
            unique_l=[]
            for element in range((lambda x: x if x < 1000 else 1000)(len(query_l))): #lambda function to limit max iterations; already limited tho
                if query_l[element]['location'] not in unique_l:
                    unique_l.append(query_l[element]['location'])
            water_time=[]
            for element in unique_l:
                query = influxdb_client.query("SELECT * FROM water_time WHERE location = '{}' ORDER BY time desc LIMIT 1".format(element))
                item=list(query.get_points(measurement='water_time'))[0]['value']
                water_time.append(item)
                republish_data=SensorData(element ,'water_time' ,float(item)) #republish at every request (better readability with grafana)
                if republish_data is not None:
                     _send_sensor_data_to_influxdb(republish_data)
            mqtt_msg=','.join(str(e) for e in water_time)
            mqtt_msg+=','
            client.publish('home/bewae/config', mqtt_msg)
            
            query = influxdb_client.query("SELECT * FROM bewae_sw GROUP BY * ORDER BY DESC LIMIT 1".format(element))
            if query:
                query_msg=str(list(query.get_points(measurement='bewae_sw'))[0]['value'])
                #print('test')
                #print(query_msg)
                client.publish('home/nano/bewae_sw', query_msg)
            
            query = influxdb_client.query("SELECT * FROM watering_sw GROUP BY * ORDER BY DESC LIMIT 1".format(element))
            if query:
                query_msg=str(list(query.get_points(measurement='watering_sw'))[0]['value'])
                #print('test')
                #print(query_msg)
                client.publish('home/nano/watering_sw', query_msg)
            
            query = influxdb_client.query("SELECT * FROM timetable_sw GROUP BY * ORDER BY DESC LIMIT 1".format(element))
            if query:
                query_msg=str(list(query.get_points(measurement='timetable_sw'))[0]['value'])
                #print('test')
                #print(query_msg)
                client.publish('home/nano/timetable_sw', query_msg)
            
            query = influxdb_client.query("SELECT * FROM timetable GROUP BY * ORDER BY DESC LIMIT 1".format(element))
            if query:
               query_msg=str(list(query.get_points(measurement='timetable'))[0]['value'])
               #print('test')
               #print(query_msg)
               client.publish('home/nano/timetable', query_msg)
            return None
        else: #real data
            return SensorData(location, measurement, float(payload))
    else:
        return None
    
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