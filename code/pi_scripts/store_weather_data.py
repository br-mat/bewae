# store_weather_data.py V1
# by br-mat (c) 2023, no waranty

# Script to gather weather data and store in InfluxDB

import logging
import os
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
import requests
import json

def get_weather(api_key, lat, lon):
    base_url = "http://api.openweathermap.org/data/2.5/weather"
    params = {
        'lat': lat,
        'lon': lon,
        'appid': api_key,
        'units': "metric",
    }
    try:
        response = requests.get(base_url, params=params)
        response.raise_for_status()
        result = response.text
        data_dict = json.loads(result)
        return data_dict
    except requests.exceptions.RequestException as e:
        print("Error:", str(e))
        data_dict = {}
        return data_dict

def transform_data(data):
    # Initialize an empty dictionary to store the transformed data
    transformed_data = {}
    fields = ["main", "wind", "clouds"]
    # Iterate over the items in the data
    for key, value in data.items():
        # If the key is in the fields list and the value is a dictionary, unpack it
        if key in fields and isinstance(value, dict):
            for subkey, subvalue in value.items():
                # Add the unpacked values to the transformed_data dictionary
                transformed_data[f"{key}_{subkey}"] = subvalue
    if key == "rain":
        transformed_data["rain"] = value.get("1h")
    else:
        transformed_data["rain"] = 0
    transformed_data.pop("main_temp_min")
    transformed_data.pop("main_temp_max")
    transformed_data.pop("main_feels_like")
    return transformed_data

def setup_logger():
    logging.basicConfig(filename='weather_logger.log', filemode='a',
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                        level=logging.INFO)
    logger = logging.getLogger(__name__)
    return logger

def pubInfluxdb(client: InfluxDBClient, bucket: str, measurement: str, location: str, data: dict):
    # Instantiate the WriteAPI
    write_api = client.write_api(write_options=SYNCHRONOUS)
    # Create a new point
    point = Point(measurement).tag("location", location)
    # Add fields to the point
    for key, value in data.items():
        if isinstance(value, (int, float)):
            point.field(key, value)
        else:
            raise ValueError(f"Invalid value for {key}: {value}. Only int and float are allowed.")
    write_api.write(bucket=bucket, record=point)
    client.__del__()

def main():
    logger = setup_logger()
    try:
        # Load the configuration
        with open('/path/to/pi_scripts/monitoring_config.json') as f:
            config = json.load(f)
        # Initialize InfluxDB client with environment variables
        client = InfluxDBClient(url=config["server"]+config["port"], token=config["db_token"], org=config["db_org"])
        data = get_weather(config["weatherAPI"], config["lat"], config["lon"])
        transformed_data = transform_data(data)
        # Store the transformed data in InfluxDB
        pubInfluxdb(client, config["bucket"], "weather", config["location"], transformed_data)
        client.close()
    except Exception as e:
        logger.error("An error occurred:", exc_info=True)
        print("An error occurred:", str(e))

if __name__ == "__main__":
    main()