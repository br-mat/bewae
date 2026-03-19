#!/usr/bin/env python3
# calculate_weather_multiplier.py
# Calculates per-group weather-based irrigation multipliers using OpenWeather forecast data.
# Writes 'wm' field into each plant group in the bewae config JSON.
#
# Designed to run via crontab (e.g., 2x daily at 05:00 and 17:00).
#
# by br-mat (c) 2025

import math
import sys
import os

from weather_utils import (
    setup_logger,
    load_config,
    fetch_forecast,
    load_irrigation_config_http,
    save_wm_http,
    publish_to_influxdb,
)

# Physical constants for Penman-Monteith ET calculation
CP = 1.013       # specific heat of air [MJ/(kg·°C)]
GAMMA = 0.066    # psychrometric constant [kPa/°C]

# Clear-sky solar radiation estimate [MJ/m²/hour] for mid-latitudes summer
# This is a simplified constant; could be refined with latitude/day-of-year
CLEAR_SKY_RADIATION = 0.8  # approximate average daytime value

logger = setup_logger("weather_multiplier", "weather_multiplier.log")


def extract_forecast_24h(forecast_list):
    """
    Extract the next 24 hours of forecast data from the API response.
    The free API returns 3-hour blocks, so 8 blocks = 24 hours.

    Returns list of dicts with standardized keys.
    """
    blocks = forecast_list[:8]  # first 8 entries = next 24h
    result = []
    for block in blocks:
        main = block.get("main", {})
        wind = block.get("wind", {})
        clouds = block.get("clouds", {})
        rain = block.get("rain", {})

        result.append({
            "temp": main.get("temp", 20.0),           # °C (API returns metric)
            "humidity": main.get("humidity", 50.0),    # %
            "pressure": main.get("pressure", 1013) / 10.0,  # hPa -> kPa
            "wind_speed": wind.get("speed", 2.0),      # m/s (already in m/s!)
            "cloud_cover": clouds.get("all", 50.0),    # %
            "rain_3h": rain.get("3h", 0.0),            # mm in 3h window
        })

    return result


def estimate_radiation(cloud_cover_pct):
    """
    Estimate net radiation using cloud cover as a proxy for UV index.
    Rn = clear_sky_radiation * (1 - cloud_cover * 0.75)

    Returns radiation in MJ/m²/hour (simplified).
    """
    cloud_fraction = cloud_cover_pct / 100.0
    return CLEAR_SKY_RADIATION * (1.0 - cloud_fraction * 0.75)


def soil_evaporation_rate(wind_ms, temp_c, humidity_pct, pressure_kpa, cloud_cover_pct, area_m2):
    """
    Calculate soil evaporation rate using adapted Penman-Monteith equation.
    Wind speed is already in m/s (no conversion needed).

    Args:
        wind_ms: wind speed in m/s
        temp_c: temperature in °C
        humidity_pct: relative humidity in %
        pressure_kpa: atmospheric pressure in kPa
        cloud_cover_pct: cloud cover in %
        area_m2: soil area in m²

    Returns:
        Evaporation rate (arbitrary units, consistent for ratio comparison)
    """
    # Guard against division by zero
    if wind_ms < 0.1:
        wind_ms = 0.1
    if temp_c < -40:
        temp_c = -40

    # Saturation vapor pressure terms
    e_s_term = math.exp(17.27 * temp_c / (temp_c + 237.3))
    delta = 4098 * (0.6108 * e_s_term) / ((temp_c + 237.3) ** 2)

    # Net radiation from cloud cover proxy
    Rn = estimate_radiation(cloud_cover_pct)

    # Air density via ideal gas law
    rho_a = pressure_kpa / (0.287 * (temp_c + 273.15))

    # Vapor pressures
    es = 0.6108 * e_s_term
    ea = humidity_pct * es / 100.0

    # Aerodynamic resistance (simplified)
    ra = 208.0 / wind_ms

    # Penman-Monteith ET
    ET = (delta * Rn + rho_a * CP * (es - ea) / ra) / (delta + GAMMA)
    ET = ET * area_m2

    return max(ET, 0.0)  # ET should never be negative


def calculate_et_for_block(block, soil_area=0.03):
    """
    Calculate evapotranspiration for a single 3-hour forecast block.
    Uses soil evaporation as the primary ET indicator.
    """
    return soil_evaporation_rate(
        wind_ms=block["wind_speed"],
        temp_c=block["temp"],
        humidity_pct=block["humidity"],
        pressure_kpa=block["pressure"],
        cloud_cover_pct=block["cloud_cover"],
        area_m2=soil_area,
    )


def calculate_baseline_et(config, soil_area=0.03):
    """
    Calculate the baseline ET for a 'normal' reference day using config values.
    This represents a typical summer day that the user's base watering duration is calibrated for.
    """
    baseline_temp = config.get("baseline_temp", 25)
    baseline_humidity = config.get("baseline_humidity", 50)
    baseline_wind = config.get("baseline_wind", 5)
    baseline_pressure = 101.3  # standard atmospheric pressure in kPa
    baseline_cloud_cover = 30  # partly cloudy

    # 8 identical blocks (24h at 3h intervals) for baseline
    et_per_block = soil_evaporation_rate(
        wind_ms=baseline_wind,
        temp_c=baseline_temp,
        humidity_pct=baseline_humidity,
        pressure_kpa=baseline_pressure,
        cloud_cover_pct=baseline_cloud_cover,
        area_m2=soil_area,
    )
    return et_per_block * 8  # 24h total


def calculate_rain_factor(total_rain_mm, threshold_mm, hard_cutoff_mm):
    """
    Combined rain logic:
    - Gradual reduction for light rain
    - Hard cutoff above heavy rain threshold

    Returns factor between 0.0 and 1.0.
    """
    if total_rain_mm >= hard_cutoff_mm:
        return 0.0

    if total_rain_mm <= 0:
        return 1.0

    # Gradual reduction: linear from 1.0 at 0mm to 0.0 at threshold
    factor = max(0.0, 1.0 - (total_rain_mm / threshold_mm))
    return factor


def calculate_base_weather_factor(forecast_blocks, config):
    """
    Calculate the base weather factor from forecast data.
    This is the ET ratio (actual/baseline) modified by rain.

    Returns (base_factor, total_rain_mm) tuple.
    """
    # Sum ET across all 24h forecast blocks
    actual_et = sum(calculate_et_for_block(block) for block in forecast_blocks)

    # Calculate baseline ET
    baseline_et = calculate_baseline_et(config)

    # Avoid division by zero
    if baseline_et <= 0:
        logger.warning("Baseline ET is zero or negative, using factor 1.0")
        et_ratio = 1.0
    else:
        et_ratio = actual_et / baseline_et

    # Sum rain across 24h
    total_rain_mm = sum(block["rain_3h"] for block in forecast_blocks)

    # Apply combined rain logic
    rain_threshold = config.get("rain_threshold_mm", 5)
    rain_hard_cutoff = config.get("rain_hard_cutoff_mm", 15)
    rain_factor = calculate_rain_factor(total_rain_mm, rain_threshold, rain_hard_cutoff)

    base_factor = et_ratio * rain_factor

    logger.info(
        f"ET actual={actual_et:.3f}, baseline={baseline_et:.3f}, ratio={et_ratio:.2f}, "
        f"rain={total_rain_mm:.1f}mm, rain_factor={rain_factor:.2f}, base_factor={base_factor:.2f}"
    )

    return base_factor, et_ratio, rain_factor, total_rain_mm


def apply_multipliers_to_config(irrig_config, base_factor, et_ratio, multiplier_max):
    """
    Calculate weather multiplier for each plant group.
    For each group: wm = clamp(factor * kc, 0.0, multiplier_max)

    If a group has ignore_rain=True, the rain factor is skipped and only
    ET ratio is used (plant is under a roof, rain doesn't reach it).

    Args:
        irrig_config: the full config dict (device -> {plantConfig, ...})
        base_factor: the base weather factor (et_ratio * rain_factor)
        et_ratio: ET ratio without rain reduction
        multiplier_max: maximum allowed multiplier

    Returns:
        tuple of (wm_results, wm_updates)
        - wm_results: {group_name: wm_value} for logging/InfluxDB
        - wm_updates: {device_name: {group_key: wm_value}} for API POST
    """
    wm_results = {}
    wm_updates = {}

    for device_name, device_config in irrig_config.items():
        if not isinstance(device_config, dict):
            continue
        plant_config = device_config.get("plantConfig", {})
        if not isinstance(plant_config, dict):
            continue

        for group_key, group_data in plant_config.items():
            if not isinstance(group_data, dict):
                continue
            # Skip non-group entries (e.g., "checksum")
            if "pn" not in group_data:
                continue

            kc = group_data.get("kc", 1.0)
            ignore_rain = group_data.get("ignore_rain", False)

            if ignore_rain:
                # Plant under roof: only ET matters, rain doesn't reach it
                wm = et_ratio * kc
            else:
                # Normal plant: full factor including rain reduction
                wm = base_factor * kc

            wm = max(0.0, min(wm, multiplier_max))
            wm = round(wm, 2)

            group_name = group_data.get("pn", group_key)
            wm_results[group_name] = wm

            if device_name not in wm_updates:
                wm_updates[device_name] = {}
            wm_updates[device_name][group_key] = wm

            rain_note = " (ignore_rain)" if ignore_rain else ""
            logger.info(f"  Group '{group_name}': kc={kc}, wm={wm}{rain_note}")

    return wm_results, wm_updates


def build_fallback_updates(irrig_config):
    """Build wm=1.0 updates for all groups (safe fallback on error)."""
    wm_updates = {}
    for device_name, device_config in irrig_config.items():
        if not isinstance(device_config, dict):
            continue
        plant_config = device_config.get("plantConfig", {})
        if not isinstance(plant_config, dict):
            continue
        for group_key, group_data in plant_config.items():
            if isinstance(group_data, dict) and "pn" in group_data:
                if device_name not in wm_updates:
                    wm_updates[device_name] = {}
                wm_updates[device_name][group_key] = 1.0
    return wm_updates


def main():
    nodered_url = None
    irrig_config = None

    try:
        # Load configs
        config = load_config()
        nodered_url = config.get("nodered_url", "http://localhost:1880")
        multiplier_max = config.get("multiplier_max", 2.0)

        logger.info("Fetching weather forecast...")
        forecast_list = fetch_forecast(config["lat"], config["lon"], config["weatherAPI"])

        if not forecast_list:
            raise ValueError("Empty forecast response from API")

        # Extract and process 24h forecast
        forecast_blocks = extract_forecast_24h(forecast_list)
        logger.info(f"Got {len(forecast_blocks)} forecast blocks")

        # Calculate base weather factor
        base_factor, et_ratio, rain_factor, total_rain_mm = calculate_base_weather_factor(forecast_blocks, config)

        # Load irrigation config via HTTP and calculate multipliers
        irrig_config = load_irrigation_config_http(nodered_url)
        wm_results, wm_updates = apply_multipliers_to_config(irrig_config, base_factor, et_ratio, multiplier_max)

        # POST wm updates to Node-RED
        save_wm_http(nodered_url, wm_updates)
        logger.info("WM updates posted to Node-RED successfully")

        # Publish to InfluxDB (optional, best-effort)
        try:
            fields = {"base_factor": base_factor, "et_ratio": et_ratio, "rain_factor": rain_factor, "rain_mm": total_rain_mm}
            fields.update({f"wm_{name}": wm for name, wm in wm_results.items()})
            publish_to_influxdb(
                measurement="weather_multiplier",
                tags={"location": config.get("location", "unknown")},
                fields=fields,
                config=config,
            )
            logger.info("Published to InfluxDB")
        except Exception as e:
            logger.warning(f"InfluxDB publish failed (non-critical): {e}")

    except Exception as e:
        logger.error(f"Error calculating weather multiplier: {e}", exc_info=True)

        # Safe fallback: try to POST wm=1.0 for all groups
        if nodered_url and irrig_config:
            try:
                fallback_updates = build_fallback_updates(irrig_config)
                if fallback_updates:
                    save_wm_http(nodered_url, fallback_updates)
                    logger.info("Fallback applied: wm=1.0 for all groups via HTTP")
            except Exception as fallback_err:
                logger.error(f"Failed to apply fallback: {fallback_err}")

        sys.exit(1)


if __name__ == "__main__":
    main()
