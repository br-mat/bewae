#!/usr/bin/env python3
# test_weather.py
# Standalone test for OpenWeather API fetch and ET/multiplier calculation.
# Bypasses fcntl and influxdb_client so it runs on Windows too.

import math
import requests

API_KEY = "your_openweathermap_api_key"
LAT = 0.0000   # your latitude
LON = 0.0000   # your longitude
LOCATION = "your_city"

# --- ET constants (from calculate_weather_multiplier.py) ---
CP = 1.013
GAMMA = 0.066
CLEAR_SKY_RADIATION = 0.8

# --- Config defaults matching config.json ---
CONFIG = {
    "baseline_temp": 25,
    "baseline_humidity": 50,
    "baseline_wind": 5,
    "rain_threshold_mm": 5,
    "rain_hard_cutoff_mm": 15,
    "multiplier_max": 2.0,
}


def fetch_forecast():
    url = "https://api.openweathermap.org/data/2.5/forecast"
    params = {"lat": LAT, "lon": LON, "appid": API_KEY, "units": "metric"}
    resp = requests.get(url, params=params, timeout=10)
    resp.raise_for_status()
    data = resp.json()
    return data.get("list", [])


def extract_forecast_24h(forecast_list):
    blocks = forecast_list[:8]
    result = []
    for block in blocks:
        main = block.get("main", {})
        wind = block.get("wind", {})
        clouds = block.get("clouds", {})
        rain = block.get("rain", {})
        result.append({
            "dt_txt": block.get("dt_txt", ""),
            "temp": main.get("temp", 20.0),
            "humidity": main.get("humidity", 50.0),
            "pressure": main.get("pressure", 1013) / 10.0,
            "wind_speed": wind.get("speed", 2.0),
            "cloud_cover": clouds.get("all", 50.0),
            "rain_3h": rain.get("3h", 0.0),
        })
    return result


def estimate_radiation(cloud_cover_pct):
    return CLEAR_SKY_RADIATION * (1.0 - (cloud_cover_pct / 100.0) * 0.75)


def soil_evaporation_rate(wind_ms, temp_c, humidity_pct, pressure_kpa, cloud_cover_pct, area_m2=0.03):
    wind_ms = max(wind_ms, 0.1)
    temp_c = max(temp_c, -40)
    e_s_term = math.exp(17.27 * temp_c / (temp_c + 237.3))
    delta = 4098 * (0.6108 * e_s_term) / ((temp_c + 237.3) ** 2)
    Rn = estimate_radiation(cloud_cover_pct)
    rho_a = pressure_kpa / (0.287 * (temp_c + 273.15))
    es = 0.6108 * e_s_term
    ea = humidity_pct * es / 100.0
    ra = 208.0 / wind_ms
    ET = (delta * Rn + rho_a * CP * (es - ea) / ra) / (delta + GAMMA)
    return max(ET * area_m2, 0.0)


def calculate_baseline_et():
    et_per_block = soil_evaporation_rate(
        wind_ms=CONFIG["baseline_wind"],
        temp_c=CONFIG["baseline_temp"],
        humidity_pct=CONFIG["baseline_humidity"],
        pressure_kpa=101.3,
        cloud_cover_pct=30,
    )
    return et_per_block * 8


def calculate_rain_factor(total_rain_mm):
    threshold = CONFIG["rain_threshold_mm"]
    hard_cutoff = CONFIG["rain_hard_cutoff_mm"]
    if total_rain_mm >= hard_cutoff:
        return 0.0
    if total_rain_mm <= 0:
        return 1.0
    return max(0.0, 1.0 - (total_rain_mm / threshold))


# --- estimate_water_loss.py V1 functions (isolated, no API call) ---
# NOTE: original script uses onecall API (paid tier) and converts wind km/h->m/s,
# but forecast API already returns m/s. We apply the same conversion here to match
# the original script's behaviour exactly — and flag the discrepancy in output.
CP_OLD = 1.013
GAMMA_OLD = 0.066

def soil_evap_v1(wind_kmh, temp, humidity, pressure, uvi, area, scaling_f=2250):
    """Verbatim from estimate_water_loss.py — wind expected in km/h."""
    wind = wind_kmh * 1000 / 3600  # km/h -> m/s (bug: forecast API is already m/s)
    e_s_term = math.exp(17.27 * temp / (temp + 237.3))
    delta = 4098 * (0.6108 * e_s_term) / ((temp + 237.3) ** 2)
    Rn = 0.77 * (0.082 * uvi)
    rho_a = pressure / (0.287 * (temp + 273.15))
    es = 0.6108 * e_s_term
    ea = humidity * es / 100
    ra = 208 / wind
    ET = (delta * Rn + rho_a * CP_OLD * (es - ea) / ra) / (delta + GAMMA_OLD)
    return max(ET * area * scaling_f, 0.0)

def transpiration_v1(wind_kmh, temp, humidity, pressure, uvi, soil_area, plant_area, scaling_f=1/125):
    Kc = 0.7
    ETc = Kc * soil_evap_v1(wind_kmh, temp, humidity, pressure, uvi, soil_area)
    bare_area = max(soil_area - plant_area, 0.001)  # guard against negative
    E = soil_evap_v1(wind_kmh, temp, humidity, pressure, uvi, bare_area)
    T = (ETc - E) * plant_area
    return T * scaling_f


def main():
    print(f"\n=== bewae Weather Multiplier Test | {LOCATION} ===\n")

    print("Fetching forecast from OpenWeatherMap...")
    forecast_list = fetch_forecast()
    print(f"  OK — received {len(forecast_list)} forecast blocks (5-day at 3h intervals)\n")

    blocks = extract_forecast_24h(forecast_list)
    print(f"Next 24h forecast ({len(blocks)} blocks):")
    print(f"  {'Time':<20} {'Temp':>6} {'Humidity':>9} {'Wind':>7} {'Clouds':>7} {'Rain':>7}")
    print(f"  {'-'*20} {'-'*6} {'-'*9} {'-'*7} {'-'*7} {'-'*7}")
    for b in blocks:
        print(f"  {b['dt_txt']:<20} {b['temp']:>5.1f}C {b['humidity']:>8.0f}% {b['wind_speed']:>5.1f}m/s {b['cloud_cover']:>6.0f}% {b['rain_3h']:>6.1f}mm")

    # --- Section 1: calculate_weather_multiplier.py ---
    print(f"\n--- calculate_weather_multiplier.py ---")

    TOMATO_KC = 1.05        # mid-season tomato crop coefficient
    BASE_DURATION_SEC = 120
    # Note: soil_area cancels in the ET ratio (actual/baseline both use same default 0.03),
    # so the multiplier is area-independent — only weather conditions matter.

    actual_et = sum(
        soil_evaporation_rate(b["wind_speed"], b["temp"], b["humidity"], b["pressure"], b["cloud_cover"])
        for b in blocks
    )
    baseline_et = calculate_baseline_et()
    total_rain = sum(b["rain_3h"] for b in blocks)
    et_ratio = actual_et / baseline_et if baseline_et > 0 else 1.0
    rain_factor = calculate_rain_factor(total_rain)
    base_factor = et_ratio * rain_factor
    wm = round(max(0.0, min(base_factor * TOMATO_KC, CONFIG["multiplier_max"])), 2)
    effective_sec = round(BASE_DURATION_SEC * wm)

    print(f"  Tomato kc:            {TOMATO_KC}")
    print(f"  Actual ET (24h):      {actual_et:.4f}")
    print(f"  Baseline ET (24h):    {baseline_et:.4f}")
    print(f"  ET ratio:             {et_ratio:.3f}")
    print(f"  Total rain:           {total_rain:.1f} mm")
    print(f"  Rain factor:          {rain_factor:.3f}")
    print(f"  Base factor:          {base_factor:.3f}")
    print(f"  wm (base*kc clamped): {wm}  [valid range: 0.0 – {CONFIG['multiplier_max']}]")
    in_range = 0.0 <= wm <= CONFIG["multiplier_max"]
    print(f"  Range check:          {'PASS' if in_range else 'FAIL'}")
    print(f"  Effective pump time:  {BASE_DURATION_SEC}s × {wm} = {effective_sec}s  (~{effective_sec/60:.1f} min)")

    # --- Section 2: estimate_water_loss.py V2 ---
    print(f"\n--- estimate_water_loss.py V2 ---")

    # Use a representative mid-day block (block index 0, roughly now)
    b = blocks[0]
    V2_SOIL_AREA = 0.0314   # hardcoded in estimate_water_loss.py
    V2_PLANT_AREA = 3       # hardcoded scale value 1-6

    # Replicate V2 soil_evaporation_rate inline (wind m/s, cloud cover radiation)
    def _evap_v2(wind_ms, temp, humidity, pressure, cloud_cover, area, scaling_f=2250):
        wind_ms = max(wind_ms, 0.1)
        e_s_term = math.exp(17.27 * temp / (temp + 237.3))
        delta = 4098 * (0.6108 * e_s_term) / ((temp + 237.3) ** 2)
        Rn = CLEAR_SKY_RADIATION * (1.0 - (cloud_cover / 100.0) * 0.75)
        rho_a = pressure / (0.287 * (temp + 273.15))
        es = 0.6108 * e_s_term
        ea = humidity * es / 100
        ra = 208 / wind_ms
        ET = (delta * Rn + rho_a * CP * (es - ea) / ra) / (CP + GAMMA)
        return max(ET * area * scaling_f, 0.0)

    evap_val = _evap_v2(b["wind_speed"], b["temp"], b["humidity"], b["pressure"], b["cloud_cover"], V2_SOIL_AREA)
    # transpiration: bare fraction (same formula as V2)
    bare_area = V2_SOIL_AREA * (1 - V2_PLANT_AREA / V2_SOIL_AREA)
    E_bare = _evap_v2(b["wind_speed"], b["temp"], b["humidity"], b["pressure"], b["cloud_cover"], bare_area)
    transp_val = ((0.7 * evap_val) - E_bare) * V2_PLANT_AREA * (1/125)
    total_loss = evap_val + transp_val

    print(f"  Representative block: {b['dt_txt']}  ({b['temp']:.1f}°C, {b['humidity']:.0f}% RH, wind {b['wind_speed']:.1f} m/s, clouds {b['cloud_cover']:.0f}%)")
    print(f"  soil_evap output:     {evap_val:.4f}  (arbitrary scaled units)")
    print(f"  transpiration output: {transp_val:.4f}  (arbitrary scaled units)")
    print(f"  Combined loss:        {total_loss:.4f}")
    evap_ok = evap_val >= 0.0
    transp_ok = transp_val >= 0.0
    print(f"  Range check evap:     {'PASS (>=0)' if evap_ok else 'FAIL'}")
    print(f"  Range check transp:   {'PASS (>=0)' if transp_ok else 'FAIL'}")
    print()


if __name__ == "__main__":
    main()
