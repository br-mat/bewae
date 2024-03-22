This document provides a brief overview of how to use the following scripts. They are designed to run on a Raspberry Pi or a similar device using crontab.

1. store_weather_data.py:
   - This script collects data from OpenWeatherMap and stores it in a local InfluxDB instance (version 2.x).
   - Note: It is currently not required for functionality.

2. estimate_water_loss.py:
   - The purpose of this script is to retrieve forecast data from OpenWeatherMap.
   - It calculates an estimated total evaporation loss in milliliters per hour.
   - This function is intended to be executed daily.
   - The results can be used to automate plant irrigation if configured correctly, considering plant size and soil area.

3. hourlisttoBIN.py (to be removed soon):
   - This script converts a list of hours into a single number, which is used in the timetable.

4. old-config-to-JSON.py (to be removed soon):
   - Use this script to convert an old configuration format to the new one.

Additional files:
5. irrigation_example.JSON: This file provides an example configuration for the irrigation system.

6. monitoring_config.JSON: In this file, you can fill in details about the connection to InfluxDB, API keys, and other relevant settings.