# estimate_water_loss.py V1
# by br-mat (c) 2024, no waranty

# Get forecast data from openweatherAPI and calculate estimated water loss

import logging
import os
import datetime
import requests, json
import pandas as pd
import math

cp = 1.013 # specific heat of the air in MJ/kg/°C
gamma = 0.066 # psychrometric constant in kPa/°C
soil_area = 0.0314 # example value for medium size in m²
plant_area = 3 # example value for medium size (no unit 1-6)

def setup_logger():
    logging.basicConfig(filename='water_estimator.log', filemode='a',
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                        level=logging.INFO)
    logger = logging.getLogger(__name__)
    return logger

# API Request
def request_data(lat, lon, key):
    URL = f"https://api.openweathermap.org/data/2.5/onecall?lat={lat}&lon={lon}&exclude=minutely&appid={key}"
    response = requests.get(URL)
    if response.status_code == 200:
        data = response.json()
        return data
    else:
        print("Error in the HTTP request")
        return None

# Take requested data and create a dataframe
def prepare_data(response):
    daily_data = response.get('hourly', [])
    # Convert to a pandas DataFrame and transform some data
    df = pd.json_normalize(daily_data)
    df["pressure"] = df["pressure"]/10 # to kPa
    df["temp"] = df["temp"] - 273.15
    return df

# Define function to calculate transpiration rate
def transpiration_rate(wind, temp, humidity, pressure, uvi, soil_area, plant_area, scaling_f = 1/125):
    Kc = 0.7 # assumed value for herbs
    # Calculate crop evapotranspiration
    ETc = Kc * soil_evaporation_rate(wind, temp, humidity, pressure, uvi, soil_area) # pass the additional parameters to the soil evaporation rate function
    # Calculate soil evaporation rate
    E = soil_evaporation_rate(wind, temp, humidity, pressure, uvi, soil_area * (1 - plant_area / soil_area)) # pass the additional parameters to the soil evaporation rate function and adjust the area ratio
    T = ETc - E # Calculate transpiration rate
    T = T * plant_area # Convert from mm/day to ml/hour
    return T * scaling_f # scaling factor

# Define function to calculate soil evaporation rate
def soil_evaporation_rate(wind, temp, humidity, pressure, uvi, area, scaling_f = 2250):
    wind = wind * 1000 / 3600 # Convert wind speed from km/h to m/s
    e_s_term = math.exp(17.27 * temp / (temp + 237.3)) # Pre-compute the term for saturation vapor pressure
    delta = 4098 * (0.6108 * e_s_term) / ((temp + 237.3) ** 2) # Calculate slope of saturation vapor pressure curve
    Rn = 0.77 * (0.082 * uvi) # Calculate radiation: simplified formula for solar radiation using uvi
    rho_a = pressure / (0.287 * (temp + 273.15)) # ideal gas law for air density
    es = 0.6108 * e_s_term # Calculate saturation vapor pressure
    ea = humidity * es / 100 # Calculate actual vapor pressure
    ra = 208 / wind # Calculate aerodynamic resistance: simplified formula for aerodynamic resistance
    ET = (delta * Rn + rho_a * cp * (es - ea) / ra) / (delta + gamma) # Calculate evapotranspiration rate
    ET = ET * area
    # Return soil evaporation rate
    return ET * scaling_f # scaling factor

def main():
    logger = setup_logger()
    try:
        with open('/path/to/pi_scripts/monitoring_config.json') as f:
            config = json.load(f)
        openweather_data = request_data(config["lat"], config["lon"], config["weatherAPI"])
        weather_df = prepare_data(openweather_data)
        weather_df['soil_evap'] = weather_df.apply(lambda row: soil_evaporation_rate(row['wind_gust'],
                                                                                     row['temp'],
                                                                                     row['humidity'],
                                                                                     row['pressure'],
                                                                                     row['uvi'],
                                                                                     soil_area),
                                                   axis=1)
        weather_df['transp'] = weather_df.apply(lambda row: transpiration_rate(row['wind_gust'],
                                                                               row['temp'],
                                                                               row['humidity'],
                                                                               row['pressure'],
                                                                               row['uvi'],
                                                                               soil_area,
                                                                               plant_area),
                                                axis=1)
        # estimate total evaporation
        total = weather_df['soil_evap'][:24].sum() + weather_df['transp'][:24].sum()
        #print("Total evaporation:")
        #print(total)
        
    except Exception as e:
        logger.error("An error occurred:", exc_info=True)
        print("An error occurred:", str(e))
        
if __name__ == "__main__":
    main()