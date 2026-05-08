# estimate_water_loss.py V3
# by br-mat (c) 2024/2026, no waranty
#
# V3 changes vs V2:
#   - Day/night radiation gate via solar elevation. At night Rn becomes a
#     small negative value, allowing the model to depict dew formation and
#     collapsing the spurious nighttime evap floor.
#   - Plant size: 1-6 integer mapped to a canopy/pot area ratio
#     (canopy_ratio_from_size). The previous dual-source split is dropped
#     because it produced negative bare-soil areas when plant_area > soil_area.
#   - transpiration_rate now uses single-coefficient FAO-56:
#         T = kc * canopy_ratio * ET0 * soil_area
#   - reference_et0() exposed for callers that want the bare Penman-Monteith
#     value without area or scaling.

import logging
import math
import os
import json
import datetime
from datetime import timezone

# --- physical constants ---
cp = 1.013                      # specific heat of air, MJ/kg/°C
gamma = 0.066                   # psychrometric constant, kPa/°C
CLEAR_SKY_RADIATION = 0.8       # MJ/m²/hour, peak clear-sky solar proxy
LONGWAVE_NIGHT_LOSS = 0.08      # MJ/m²/hour, net radiative cooling toward sky at night

# --- defaults (override per call / from config) ---
soil_area = 0.0314              # m², ~20 cm pot footprint
DEFAULT_SIZE = 3                # 1-6 plant size index (see canopy_ratio_from_size)
DEFAULT_KC = 0.5                # FAO crop coefficient — initial-stage / per unit canopy

DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")


def canopy_ratio_from_size(size):
    """Map the 1-6 size index to a canopy-to-pot area ratio.
        1 -> 0.5  (plant smaller than pot, e.g. seedling)
        3 -> 1.9  (slightly overhangs pot - default)
        6 -> 4.0  (overgrown, e.g. mid-summer tomato)
    """
    s = max(1, min(6, int(size)))
    return 0.5 + (s - 1) * 0.7


def solar_elevation_deg(lat_deg, lon_deg, when_utc):
    """Approximate solar elevation angle in degrees for a UTC datetime.

    Cooper's declination + simplified equation of time. Sub-degree accuracy
    near the horizon, which is all the day/night gate cares about.
    """
    if when_utc.tzinfo is not None:
        when_utc = when_utc.astimezone(timezone.utc).replace(tzinfo=None)

    n = when_utc.timetuple().tm_yday
    decl = 23.45 * math.sin(math.radians(360.0 * (284 + n) / 365.0))
    B = math.radians(360.0 * (n - 81) / 365.0)
    eot_min = 9.87 * math.sin(2 * B) - 7.53 * math.cos(B) - 1.5 * math.sin(B)
    solar_hour = when_utc.hour + when_utc.minute / 60.0 + lon_deg / 15.0 + eot_min / 60.0
    H = math.radians(15.0 * (solar_hour - 12.0))
    lat = math.radians(lat_deg)
    d = math.radians(decl)
    sin_elev = math.sin(lat) * math.sin(d) + math.cos(lat) * math.cos(d) * math.cos(H)
    return math.degrees(math.asin(max(-1.0, min(1.0, sin_elev))))


def estimate_radiation(cloud_cover_pct, when_utc, lat_deg, lon_deg):
    """Net radiation proxy in MJ/m²/hour.

    Day:   solar input scaled by sin(elevation) and reduced by clouds.
    Night: small negative Rn (longwave cooling), suppressed by clouds.
    """
    elev = solar_elevation_deg(lat_deg, lon_deg, when_utc)
    cloud = cloud_cover_pct / 100.0
    if elev <= 0:
        return -LONGWAVE_NIGHT_LOSS * (1.0 - 0.7 * cloud)
    return CLEAR_SKY_RADIATION * math.sin(math.radians(elev)) * (1.0 - 0.75 * cloud)


def reference_et0(wind_ms, temp, humidity, pressure, cloud_cover, when_utc, lat_deg, lon_deg):
    """Penman-Monteith reference ET in raw model units per m² per hour.

    pressure: kPa
    """
    wind_ms = max(wind_ms, 0.1)
    e_s_term = math.exp(17.27 * temp / (temp + 237.3))
    delta = 4098 * (0.6108 * e_s_term) / ((temp + 237.3) ** 2)
    Rn = estimate_radiation(cloud_cover, when_utc, lat_deg, lon_deg)
    rho_a = pressure / (0.287 * (temp + 273.15))
    es = 0.6108 * e_s_term
    ea = humidity * es / 100.0
    ra = 208 / wind_ms
    return (delta * Rn + rho_a * cp * (es - ea) / ra) / (delta + gamma)


def soil_evaporation_rate(wind_ms, temp, humidity, pressure, cloud_cover, area,
                          when_utc, lat_deg, lon_deg, scaling_f=2250):
    """Bare-soil evaporation over `area` m² — kept for backwards compatibility.

    For the realistic per-pot loss with a plant present, use total_water_loss().
    """
    et0 = reference_et0(wind_ms, temp, humidity, pressure, cloud_cover,
                        when_utc, lat_deg, lon_deg)
    return et0 * area * scaling_f


def transpiration_rate(wind_ms, temp, humidity, pressure, cloud_cover, soil_area,
                       size, when_utc, lat_deg, lon_deg, kc=DEFAULT_KC, scaling_f=2250):
    """Plant transpiration only.

    Single-coefficient FAO-56:  T = kc * canopy_ratio * ET0 * soil_area
    canopy_ratio comes from the 1-6 size index. Drops the broken dual-source
    split that produced negative bare-soil areas when plant_area > soil_area.
    """
    canopy_ratio = canopy_ratio_from_size(size)
    et0 = reference_et0(wind_ms, temp, humidity, pressure, cloud_cover,
                        when_utc, lat_deg, lon_deg)
    return kc * canopy_ratio * et0 * soil_area * scaling_f


def total_water_loss(wind_ms, temp, humidity, pressure, cloud_cover, soil_area,
                     size, when_utc, lat_deg, lon_deg, kc=DEFAULT_KC, scaling_f=2250):
    """Full pot water loss = bare-soil evap on the uncovered fraction + plant transpiration.

    Once the canopy covers the pot (canopy_ratio >= 1) the bare fraction is 0
    and the result is pure transpiration. This is the irrigation-relevant
    quantity — how much water the pot lost over the period.
    """
    canopy_ratio = canopy_ratio_from_size(size)
    bare_fraction = max(0.0, 1.0 - min(canopy_ratio, 1.0))
    et0 = reference_et0(wind_ms, temp, humidity, pressure, cloud_cover,
                        when_utc, lat_deg, lon_deg)
    soil_evap = et0 * soil_area * bare_fraction
    transp = kc * canopy_ratio * et0 * soil_area
    return (soil_evap + transp) * scaling_f


# ---------------------------------------------------------------------------
# Below: forecast wiring kept compatible with the V2 main(), now passing
# timestamp + lat/lon through to the new radiation gate.

def setup_logger():
    logging.basicConfig(filename='water_estimator.log', filemode='a',
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
                        level=logging.INFO)
    return logging.getLogger(__name__)


def request_data(lat, lon, key):
    import requests

    URL = "https://api.openweathermap.org/data/2.5/forecast"
    params = {"lat": lat, "lon": lon, "appid": key, "units": "metric"}
    response = requests.get(URL, params=params)
    if response.status_code == 200:
        return response.json()
    print("Error in the HTTP request")
    return None


def prepare_data(response):
    import pandas as pd

    forecast_list = response.get('list', [])
    df = pd.json_normalize(forecast_list)
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
        df['wind_gust'] = df['wind_speed']
    df["pressure"] = df["pressure"] / 10  # hPa -> kPa
    df["dt_utc"] = pd.to_datetime(df["dt"], unit="s", utc=True).dt.tz_convert(None)
    return df


def main():
    logger = setup_logger()
    try:
        with open(DEFAULT_CONFIG_PATH) as f:
            config = json.load(f)
        lat = float(config["lat"])
        lon = float(config["lon"])
        size = int(config.get("size", DEFAULT_SIZE))
        kc = float(config.get("kc", DEFAULT_KC))

        openweather_data = request_data(config["lat"], config["lon"], config["weatherAPI"])
        weather_df = prepare_data(openweather_data)

        weather_df['total_loss'] = weather_df.apply(
            lambda row: total_water_loss(row['wind_gust'], row['temp'], row['humidity'],
                                         row['pressure'], row['cloud_cover'], soil_area,
                                         size, row['dt_utc'].to_pydatetime(), lat, lon, kc=kc),
            axis=1)
        # 24h horizon = 8 forecast blocks of 3h each
        total = weather_df['total_loss'][:8].sum()
        logger.info(f"Estimated 24h water loss: {total:.2f} model units")

    except Exception as e:
        logger.error("An error occurred:", exc_info=True)
        print("An error occurred:", str(e))


if __name__ == "__main__":
    main()
