#!/usr/bin/env python3
"""Calculate V3 weather-based irrigation multipliers from stored forecast data.

V3 uses a Penman-Monteith reference ET0 ratio over the next 24 hours:

    wm = clamp(drying_ratio * rain_factor * (pls / 100), 0, multiplier_max)

The Node-RED endpoints and ESP-facing wm wire format are unchanged.
Designed to run via crontab once daily at 06:00.
"""

from __future__ import annotations

import argparse
import json
import sys
import time

from estimate_water_loss import reference_et0
from weather_utils import (
    setup_logger,
    load_config,
    fetch_forecast_blocks_from_influx,
    load_irrigation_config_http,
    save_wm_http,
    publish_to_influxdb,
)


logger = setup_logger("weather_multiplier", "weather_multiplier.log")


def clamp(value, minimum, maximum):
    return max(minimum, min(value, maximum))


def drying_score_v3(block, lat, lon):
    """Reference ET0 for one forecast block."""
    return reference_et0(
        wind_ms=block["wind_speed"],
        temp=block["temp"],
        humidity=block["humidity"],
        pressure=block["pressure"] / 10.0,
        cloud_cover=block["cloud_cover"],
        when_utc=block["time"],
        lat_deg=lat,
        lon_deg=lon,
    )


def baseline_drying_sum_v3(blocks, config, lat, lon):
    """Baseline ET0 over the same forecast timestamps as the actual weather."""
    baseline_temp = float(config.get("baseline_temp", 25.0))
    baseline_humidity = float(config.get("baseline_humidity", 50.0))
    baseline_wind = float(config.get("baseline_wind", 5.0))
    baseline_cloud_cover = float(config.get("baseline_cloud_cover", 30.0))

    return sum(
        reference_et0(
            wind_ms=baseline_wind,
            temp=baseline_temp,
            humidity=baseline_humidity,
            pressure=block["pressure"] / 10.0,
            cloud_cover=baseline_cloud_cover,
            when_utc=block["time"],
            lat_deg=lat,
            lon_deg=lon,
        )
        for block in blocks
    )


def calculate_rain_factor(rain_mm, threshold_mm, hard_cutoff_mm):
    """
    Combined rain logic:
    - no reduction when no rain is forecast
    - linear reduction for light rain
    - hard cutoff above the heavy-rain threshold
    """
    if rain_mm <= 0:
        return 1.0

    if hard_cutoff_mm > 0 and rain_mm >= hard_cutoff_mm:
        return 0.0

    if threshold_mm <= 0:
        return 0.0

    return max(0.0, 1.0 - rain_mm / threshold_mm)


def compute_factors(blocks, config, lat, lon):
    """
    Calculate aggregate V3 weather factors for the next forecast blocks.

    Returns a dict with actual/baseline drying sums, drying_ratio, rain values,
    rain_factor, and base_factor.
    """
    if not blocks:
        raise ValueError("No forecast blocks available")

    actual_sum = sum(drying_score_v3(block, lat, lon) for block in blocks)
    baseline_sum = baseline_drying_sum_v3(blocks, config, lat, lon)

    if baseline_sum <= 0:
        logger.warning("Baseline drying sum is zero or negative; using ratio 1.0")
        drying_ratio = 1.0
    else:
        drying_ratio = actual_sum / baseline_sum

    total_rain_mm = sum(block["rain_3h"] for block in blocks)
    effective_rain_mm = sum(
        block["rain_3h"] * (0.5 + 0.5 * block.get("pop", 1.0))
        for block in blocks
    )

    rain_threshold = float(config.get("rain_threshold_mm", 5.0))
    rain_hard_cutoff = float(config.get("rain_hard_cutoff_mm", 15.0))
    rain_factor = calculate_rain_factor(
        effective_rain_mm, rain_threshold, rain_hard_cutoff
    )
    base_factor = drying_ratio * rain_factor

    logger.info(
        f"V3 actual_sum={actual_sum:.4f}, baseline_sum={baseline_sum:.4f}, "
        f"drying_ratio={drying_ratio:.3f}, rain={total_rain_mm:.1f}mm, "
        f"effective_rain={effective_rain_mm:.1f}mm, "
        f"rain_factor={rain_factor:.2f}, base_factor={base_factor:.2f}"
    )

    return {
        "actual_sum": actual_sum,
        "baseline_sum": baseline_sum,
        "drying_ratio": drying_ratio,
        "rain_mm": total_rain_mm,
        "effective_rain_mm": effective_rain_mm,
        "rain_factor": rain_factor,
        "base_factor": base_factor,
    }


def _group_pls(group_data):
    try:
        pls = float(group_data.get("pls", 100.0))
    except (TypeError, ValueError):
        pls = 100.0
    return clamp(pls, 0.0, 100.0)


def apply_multipliers_to_config(irrig_config, base_factor, drying_ratio, multiplier_max):
    """
    Calculate per-group wm values.

    V3 no longer reads kc. The only plant-coupling input is pls, interpreted as
    a 0-100 percent plant-size/demand slider. Missing or invalid pls defaults
    to 100, while pls=0 intentionally skips watering for that group.
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
            if not isinstance(group_data, dict) or "pn" not in group_data:
                continue

            pls = _group_pls(group_data)
            plant_factor = pls / 100.0
            ignore_rain = bool(group_data.get("ignore_rain", False))
            weather_factor = drying_ratio if ignore_rain else base_factor
            wm = round(clamp(weather_factor * plant_factor, 0.0, multiplier_max), 2)

            group_name = group_data.get("pn", group_key)
            wm_results[group_name] = wm
            wm_updates.setdefault(device_name, {})[group_key] = wm

            rain_note = " (ignore_rain)" if ignore_rain else ""
            logger.info(
                f"  {device_name}.{group_key} '{group_name}': "
                f"pls={pls:.0f}%, plant_factor={plant_factor:.2f}, wm={wm}{rain_note}"
            )

    return wm_results, wm_updates


def build_fallback_updates(irrig_config):
    """Build wm=1.0 updates for all groups as a safe fallback on live errors."""
    wm_updates = {}
    for device_name, device_config in irrig_config.items():
        if not isinstance(device_config, dict):
            continue
        plant_config = device_config.get("plantConfig", {})
        if not isinstance(plant_config, dict):
            continue
        for group_key, group_data in plant_config.items():
            if isinstance(group_data, dict) and "pn" in group_data:
                wm_updates.setdefault(device_name, {})[group_key] = 1.0
    return wm_updates


def print_dry_run_report(blocks, factors, wm_results, wm_updates):
    print("\n" + "=" * 72)
    print("  DRY RUN - no Node-RED POST, no InfluxDB write")
    print("=" * 72)
    print(f"\n  Forecast window  : {blocks[0]['time']}  ->  {blocks[-1]['time']}")
    print(f"  Blocks used      : {len(blocks)} (next {len(blocks) * 3}h)")
    issued_at = blocks[-1].get("issued_at_unix", 0.0)
    if issued_at:
        print(
            "  Forecast issued  : "
            f"{time.strftime('%Y-%m-%d %H:%M UTC', time.gmtime(issued_at))}"
        )

    print("\n  --- Aggregate factors ---")
    print(f"  drying_ratio     : {factors['drying_ratio']:.3f}")
    print(f"  rain_mm          : {factors['rain_mm']:.2f}")
    print(f"  effective_rain   : {factors['effective_rain_mm']:.2f}")
    print(f"  rain_factor      : {factors['rain_factor']:.2f}")
    print(f"  base_factor      : {factors['base_factor']:.2f}")

    print("\n  --- Per-group wm (would be POSTed) ---")
    if not wm_results:
        print("    (no groups found in irrigation config)")
    else:
        for name, wm in wm_results.items():
            print(f"    {name:<24} -> {wm:.2f}")

    print("\n  --- Wire-format payload (POST body) ---")
    print(json.dumps(wm_updates, indent=2))
    print()


def main(dry_run=False):
    nodered_url = None
    irrig_config = None

    try:
        config = load_config()

        nodered_url = config.get("nodered_url", "http://localhost:1880")
        multiplier_max = float(config.get("multiplier_max", 2.0))
        lat = float(config["lat"])
        lon = float(config["lon"])

        logger.info("Fetching forecast from InfluxDB...")
        blocks = fetch_forecast_blocks_from_influx(config, limit=8)
        if not blocks:
            raise ValueError("Empty or stale forecast response from InfluxDB")
        logger.info(f"Got {len(blocks)} forecast blocks")

        factors = compute_factors(blocks, config, lat, lon)

        logger.info(f"Loading irrigation config from {nodered_url}...")
        irrig_config = load_irrigation_config_http(nodered_url)
        wm_results, wm_updates = apply_multipliers_to_config(
            irrig_config,
            factors["base_factor"],
            factors["drying_ratio"],
            multiplier_max,
        )

        if dry_run:
            print_dry_run_report(blocks, factors, wm_results, wm_updates)
            logger.info("Dry-run complete; no side effects.")
            return

        save_wm_http(nodered_url, wm_updates)
        logger.info("WM updates posted to Node-RED successfully")

        try:
            fields = {
                "base_factor": factors["base_factor"],
                "drying_ratio": factors["drying_ratio"],
                "rain_factor": factors["rain_factor"],
                "rain_mm": factors["rain_mm"],
                "effective_rain_mm": factors["effective_rain_mm"],
                "actual_sum": factors["actual_sum"],
                "baseline_sum": factors["baseline_sum"],
            }
            fields.update({f"wm_{name}": wm for name, wm in wm_results.items()})
            publish_to_influxdb(
                measurement="weather_multiplier",
                tags={
                    "location": config.get(
                        "forecast_location", config.get("location", "unknown")
                    ),
                    "model": "v3",
                },
                fields=fields,
                config=config,
            )
            logger.info("Published to InfluxDB")
        except Exception as e:
            logger.warning(f"InfluxDB publish failed (non-critical): {e}")

    except Exception as e:
        logger.error(f"Error calculating weather multiplier: {e}", exc_info=True)

        if not dry_run and nodered_url and irrig_config:
            try:
                fallback_updates = build_fallback_updates(irrig_config)
                if fallback_updates:
                    save_wm_http(nodered_url, fallback_updates)
                    logger.info("Fallback applied: wm=1.0 for all groups via HTTP")
            except Exception as fallback_err:
                logger.error(f"Failed to apply fallback: {fallback_err}")

        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="V3 weather multiplier using forecast ET0 ratio"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Compute and print only. Do not POST to Node-RED or write InfluxDB.",
    )
    args = parser.parse_args()
    main(dry_run=args.dry_run)
