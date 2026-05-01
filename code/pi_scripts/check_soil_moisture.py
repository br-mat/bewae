#!/usr/bin/env python3
# check_soil_moisture.py
# Checks soil moisture sensor readings from InfluxDB and overrides wm=0
# for plant groups whose soil is wet enough (skip watering).
#
# Designed to run via crontab 5 minutes after calculate_weather_multiplier.py
# (at 06:05 and 18:05).
#
# by br-mat (c) 2025

import sys

from weather_utils import (
    setup_logger,
    load_config,
    load_irrigation_config_http,
    save_wm_http,
    publish_to_influxdb,
)
from influxdb_client import InfluxDBClient

# ── Constants ─────────────────────────────────────────────────────────
DEFAULT_WET_THRESHOLD = 75      # % above which soil is considered "wet"
DEFAULT_MEASUREMENT = "sensor_data"  # InfluxDB measurement name
MAX_READINGS = 3                # Number of recent readings to query
MIN_VALID_FOR_OVERRIDE = 2      # How many valid readings must be above threshold
MAX_SPIKE = 40                  # % change between consecutive readings = spike
QUERY_RANGE_HOURS = 24          # How far back to look for readings

logger = setup_logger("soil_moisture", "soil_moisture.log")


def query_moisture(client, bucket, org, measurement, field_name):
    """
    Query InfluxDB for the last MAX_READINGS values of a specific sensor field.
    Returns list of float values (newest first).
    """
    query = (
        f'from(bucket: "{bucket}")'
        f'  |> range(start: -{QUERY_RANGE_HOURS}h)'
        f'  |> filter(fn: (r) => r._measurement == "{measurement}")'
        f'  |> filter(fn: (r) => r._field == "{field_name}")'
        f'  |> sort(columns: ["_time"], desc: true)'
        f'  |> limit(n: {MAX_READINGS})'
    )

    tables = client.query_api().query(query, org=org)
    values = []
    for table in tables:
        for record in table.records:
            val = record.get_value()
            if isinstance(val, (int, float)):
                values.append(float(val))

    return values


def validate_readings(values):
    """
    Apply sanity checks to sensor readings:
    1. Drop readings outside 0-100% range
    2. Drop readings with impossible spikes (>MAX_SPIKE% change between consecutive)

    Returns filtered list.
    """
    # Step 1: drop out-of-range values
    valid = [v for v in values if 0 <= v <= 100]

    if len(valid) < 2:
        return valid

    # Step 2: drop spikes — walk through and keep readings that don't jump too much
    filtered = [valid[0]]
    for i in range(1, len(valid)):
        if abs(valid[i] - valid[i - 1]) <= MAX_SPIKE:
            filtered.append(valid[i])
        else:
            logger.warning(
                f"  Spike detected: {valid[i-1]:.1f} -> {valid[i]:.1f} "
                f"(change {abs(valid[i] - valid[i-1]):.1f}%), dropping {valid[i]:.1f}"
            )

    return filtered


def main():
    client = None
    try:
        # Load configs
        config = load_config()

        # Attach Logfire remote logging if configured
        logfire_url = config.get("logfire_url")
        if logfire_url:
            from weather_utils import LogfireHandler
            logger.addHandler(LogfireHandler(logfire_url, "bewae-moisture"))

        nodered_url = config.get("nodered_url", "http://localhost:1880")
        wet_threshold = config.get("wet_threshold", DEFAULT_WET_THRESHOLD)
        measurement = config.get("sensor_measurement", DEFAULT_MEASUREMENT)

        logger.info(f"Checking soil moisture (threshold={wet_threshold}%, measurement={measurement})")

        irrig_config = load_irrigation_config_http(nodered_url)

        # Connect to InfluxDB
        url = config["server"] + config["port"]
        client = InfluxDBClient(url=url, token=config["db_token"], org=config["db_org"])
        bucket = config["bucket"]
        org = config["db_org"]

        wm_updates = {}
        override_results = {}

        for device_name, device_data in irrig_config.items():
            if not isinstance(device_data, dict):
                continue
            plant_config = device_data.get("plantConfig", {})
            if not isinstance(plant_config, dict):
                continue

            for group_key, group_data in plant_config.items():
                if not isinstance(group_data, dict) or "pn" not in group_data:
                    continue

                sensor_field = group_data.get("moisture_sensor", "")
                if not sensor_field or not sensor_field.strip():
                    continue  # No sensor configured for this group, skip

                group_name = group_data.get("pn", group_key)
                logger.info(f"Group '{group_name}': sensor field = '{sensor_field}'")

                # Query InfluxDB
                readings = query_moisture(client, bucket, org, measurement, sensor_field)
                logger.info(f"  Raw readings: {readings}")

                if not readings:
                    logger.info("  No readings found, skipping")
                    continue

                # Validate
                valid = validate_readings(readings)
                logger.info(f"  Valid readings: {valid}")

                if len(valid) < MIN_VALID_FOR_OVERRIDE:
                    logger.info(f"  Not enough valid readings ({len(valid)}/{MIN_VALID_FOR_OVERRIDE}), skipping")
                    continue

                # Check if soil is wet
                above = sum(1 for v in valid if v > wet_threshold)
                if above >= MIN_VALID_FOR_OVERRIDE:
                    logger.info(
                        f"  {above} readings above {wet_threshold}% — soil wet, setting wm=0"
                    )
                    if device_name not in wm_updates:
                        wm_updates[device_name] = {}
                    wm_updates[device_name][group_key] = 0.0
                    override_results[group_name] = 0.0
                else:
                    logger.info(f"  Soil not wet enough ({above}/{MIN_VALID_FOR_OVERRIDE} above threshold), wm unchanged")

        # POST overrides if any
        if wm_updates:
            save_wm_http(nodered_url, wm_updates)
            logger.info("WM overrides posted to Node-RED successfully")
        else:
            logger.info("No moisture overrides needed")

        # Publish results to InfluxDB (best-effort)
        if override_results:
            try:
                fields = {f"moisture_override_{name}": wm for name, wm in override_results.items()}
                publish_to_influxdb(
                    measurement="soil_moisture_check",
                    tags={"location": config.get("location", "unknown")},
                    fields=fields,
                    config=config,
                )
                logger.info("Published to InfluxDB")
            except Exception as e:
                logger.warning(f"InfluxDB publish failed (non-critical): {e}")

    except Exception as e:
        logger.error(f"Error checking soil moisture: {e}", exc_info=True)
        sys.exit(1)
    finally:
        if client:
            client.close()


if __name__ == "__main__":
    main()
