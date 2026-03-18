# estimate_water_loss.py V2
# by br-mat (c) 2024/2025, no waranty

# Get forecast data from openweatherAPI (free tier) and calculate estimated water loss
# Uses /data/2.5/forecast (5-day/3h, free) instead of onecall (paid).

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
CLEAR_SKY_RADIATION = 0.8  # MJ/m²/hour simplified constant (proxy for solar radiation)

DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "monitoring_config.JSON")

def setup_logger():
    logging.basicConfig(filename='water_estimator.log', filemode='a',
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                        level=logging.INFO)
    logger = logging.getLogger(__name__)
    return logger

# API Request — free tier /data/2.5/forecast (5-day, 3h intervals)
def request_data(lat, lon, key):
    URL = "https://api.openweathermap.org/data/2.5/forecast"
    params = {"lat": lat, "lon": lon, "appid": key, "units": "metric"}
    response = requests.get(URL, params=params)
    if response.status_code == 200:
        data = response.json()
        return data
    else:
        print("Error in the HTTP request")
        return None

# Take requested data and create a dataframe
def prepare_data(response):
    forecast_list = response.get('list', [])
    # Convert to a pandas DataFrame and transform some data
    df = pd.json_normalize(forecast_list)
    # Rename nested columns to flat names
    rename_map = {
        'main.temp': 'temp',
        'main.humidity': 'humidity',
        'main.pressure': 'pressure',
        'wind.speed': 'wind_speed',
        'wind.gust': 'wind_gust',
        'clouds.all': 'cloud_cover',
    }
    df = df.rename(columns={k: v for k, v in rename_map.items() if k in df.columns})
    if 'wind_gust' not in df.columns:
        df['wind_gust'] = df['wind_speed']  # fall back to speed if gust not reported
    df["pressure"] = df["pressure"] / 10  # hPa -> kPa
    # temp already in °C (units=metric), no Kelvin conversion needed
    return df

# Estimate net radiation from cloud cover (replaces UVI — free tier has no UVI)
def estimate_radiation(cloud_cover_pct):
    return CLEAR_SKY_RADIATION * (1.0 - (cloud_cover_pct / 100.0) * 0.75)

# Define function to calculate transpiration rate
def transpiration_rate(wind_ms, temp, humidity, pressure, cloud_cover, soil_area, plant_area, scaling_f = 1/125):
    Kc = 0.7 # assumed value for herbs
    # Calculate crop evapotranspiration
    ETc = Kc * soil_evaporation_rate(wind_ms, temp, humidity, pressure, cloud_cover, soil_area)
    # Calculate soil evaporation rate on bare fraction
    E = soil_evaporation_rate(wind_ms, temp, humidity, pressure, cloud_cover, soil_area * (1 - plant_area / soil_area))
    T = ETc - E # Calculate transpiration rate
    T = T * plant_area # Convert from mm/day to ml/hour
    return T * scaling_f # scaling factor

# Define function to calculate soil evaporation rate
def soil_evaporation_rate(wind_ms, temp, humidity, pressure, cloud_cover, area, scaling_f = 2250):
    # wind already in m/s from forecast API (units=metric)
    wind_ms = max(wind_ms, 0.1)  # guard against division by zero
    e_s_term = math.exp(17.27 * temp / (temp + 237.3)) # Pre-compute the term for saturation vapor pressure
    delta = 4098 * (0.6108 * e_s_term) / ((temp + 237.3) ** 2) # Calculate slope of saturation vapor pressure curve
    Rn = estimate_radiation(cloud_cover) # radiation from cloud cover proxy (replaces UVI)
    rho_a = pressure / (0.287 * (temp + 273.15)) # ideal gas law for air density
    es = 0.6108 * e_s_term # Calculate saturation vapor pressure
    ea = humidity * es / 100 # Calculate actual vapor pressure
    ra = 208 / wind_ms # Calculate aerodynamic resistance
    ET = (delta * Rn + rho_a * cp * (es - ea) / ra) / (delta + gamma) # Calculate evapotranspiration rate
    ET = ET * area
    # Return soil evaporation rate
    return ET * scaling_f # scaling factor

def main():
    logger = setup_logger()
    try:
        with open(DEFAULT_CONFIG_PATH) as f:
            config = json.load(f)
        openweather_data = request_data(config["lat"], config["lon"], config["weatherAPI"])
        weather_df = prepare_data(openweather_data)
        weather_df['soil_evap'] = weather_df.apply(lambda row: soil_evaporation_rate(row['wind_gust'],
                                                                                     row['temp'],
                                                                                     row['humidity'],
                                                                                     row['pressure'],
                                                                                     row['cloud_cover'],
                                                                                     soil_area),
                                                   axis=1)
        weather_df['transp'] = weather_df.apply(lambda row: transpiration_rate(row['wind_gust'],
                                                                               row['temp'],
                                                                               row['humidity'],
                                                                               row['pressure'],
                                                                               row['cloud_cover'],
                                                                               soil_area,
                                                                               plant_area),
                                                axis=1)
        # estimate total evaporation over 24h (8 blocks of 3h each)
        total = weather_df['soil_evap'][:8].sum() + weather_df['transp'][:8].sum()
        #print("Total evaporation:")
        #print(total)

    except Exception as e:
        logger.error("An error occurred:", exc_info=True)
        print("An error occurred:", str(e))

if __name__ == "__main__":
    main()