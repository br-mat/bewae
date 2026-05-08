# weather_utils.py
# Shared utilities for bewae weather-based irrigation scripts
# by br-mat (c) 2025

import json
import logging
import os
import tempfile
import time
import requests
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

try:
    import fcntl
except ImportError:
    fcntl = None

# Default paths
DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
REQUEST_TIMEOUT = 10  # seconds


def setup_logger(name, log_file=None, level=logging.INFO):
    """Set up a logger with console and optional local file output."""
    logger = logging.getLogger(name)
    logger.setLevel(level)

    formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")

    # Console handler
    ch = logging.StreamHandler()
    ch.setFormatter(formatter)
    logger.addHandler(ch)

    # File handler (optional)
    if log_file:
        fh = logging.FileHandler(log_file, mode="a")
        fh.setFormatter(formatter)
        logger.addHandler(fh)

    return logger


def load_config(path=None):
    """Load config.json."""
    path = path or DEFAULT_CONFIG_PATH
    with open(path, "r") as f:
        return json.load(f)


def fetch_forecast(lat, lon, api_key):
    """
    Fetch 5-day/3-hour forecast from OpenWeatherMap free API.
    Returns list of forecast entries (up to 40, covering 5 days).
    Each entry covers a 3-hour window.
    """
    url = "https://api.openweathermap.org/data/2.5/forecast"
    params = {
        "lat": lat,
        "lon": lon,
        "appid": api_key,
        "units": "metric",  # temp in °C, wind in m/s
    }
    response = requests.get(url, params=params, timeout=REQUEST_TIMEOUT)
    response.raise_for_status()
    data = response.json()
    return data.get("list", [])


def fetch_current_weather(lat, lon, api_key):
    """
    Fetch current weather from OpenWeatherMap free API.
    Returns dict with current conditions.
    """
    url = "https://api.openweathermap.org/data/2.5/weather"
    params = {
        "lat": lat,
        "lon": lon,
        "appid": api_key,
        "units": "metric",
    }
    response = requests.get(url, params=params, timeout=REQUEST_TIMEOUT)
    response.raise_for_status()
    return response.json()


def _flux_string(value):
    """Return a safely quoted Flux string literal."""
    return json.dumps(str(value))


def _num(value, default):
    try:
        if value is None:
            return default
        return float(value)
    except (TypeError, ValueError):
        return default


def fetch_forecast_blocks_from_influx(config, limit=8):
    """
    Read OpenWeatherMap forecast points from InfluxDB.

    Returns normalized 3-hour forecast blocks with keys expected by
    calculate_weather_multiplier.py. Forecast points are timestamped at the
    target time, so the Flux range deliberately looks into the future.
    """
    required_keys = ["db_org", "server", "port", "bucket_env_forecast", "db_token_env_forecast"]
    for key in required_keys:
        if key not in config:
            raise KeyError(f"Missing required config key: '{key}'")

    bucket = config["bucket_env_forecast"]
    location = config.get("forecast_location") or config.get("location") or "Graz"
    source = config.get("forecast_source", "openweathermap")
    max_age_hours = float(config.get("forecast_max_age_hours", 4))

    fields = [
        "temp",
        "humidity",
        "pressure",
        "wind_speed",
        "rain_3h",
        "pop",
        "clouds_all",
        "issued_at_unix",
    ]
    field_filter = " or ".join([f'r["_field"] == "{field}"' for field in fields])
    source_filter = f'\n  |> filter(fn: (r) => r["source"] == {_flux_string(source)})' if source else ""
    query = f'''
from(bucket: {_flux_string(bucket)})
  |> range(start: -1h, stop: 5d)
  |> filter(fn: (r) => r["_measurement"] == "weather_forecast")
  |> filter(fn: (r) => r["location"] == {_flux_string(location)}){source_filter}
  |> filter(fn: (r) => {field_filter})
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> sort(columns: ["_time"])
'''

    url = config["server"] + config["port"]
    client = InfluxDBClient(url=url, token=config["db_token_env_forecast"], org=config["db_org"])
    blocks = []
    now_unix = time.time()
    try:
        tables = client.query_api().query(query)
        for table in tables:
            for record in table.records:
                values = record.values
                target_time = values.get("_time")
                if target_time is None:
                    continue
                issued_at_unix = _num(values.get("issued_at_unix"), 0.0)
                if issued_at_unix and now_unix - issued_at_unix > max_age_hours * 3600:
                    continue
                blocks.append({
                    "time": target_time,
                    "temp": _num(values.get("temp"), 20.0),
                    "humidity": _num(values.get("humidity"), 50.0),
                    "pressure": _num(values.get("pressure"), 1013.0),
                    "wind_speed": _num(values.get("wind_speed"), 2.0),
                    "cloud_cover": _num(values.get("clouds_all"), 50.0),
                    "rain_3h": max(0.0, _num(values.get("rain_3h"), 0.0)),
                    "pop": max(0.0, min(_num(values.get("pop"), 1.0), 1.0)),
                    "issued_at_unix": issued_at_unix,
                })
    finally:
        client.close()

    blocks.sort(key=lambda block: block["time"])
    return blocks[:limit]


def load_irrigation_config(config_path):
    """Read the full bewae config JSON (full-config.json)."""
    with open(config_path, "r") as f:
        return json.load(f)


def load_irrigation_config_http(base_url):
    """Read the full bewae config via Node-RED HTTP API."""
    url = base_url.rstrip("/") + "/bewae/get-backendconfig-full"
    response = requests.get(url, timeout=REQUEST_TIMEOUT)
    response.raise_for_status()
    return response.json()


def save_wm_http(base_url, wm_updates):
    """
    POST wm updates to Node-RED.

    Args:
        base_url: e.g. "http://localhost:1880"
        wm_updates: dict of {device_name: {group_key: wm_value, ...}, ...}
    """
    url = base_url.rstrip("/") + "/bewae/update-wm"
    response = requests.post(url, json=wm_updates, timeout=REQUEST_TIMEOUT)
    response.raise_for_status()
    return response.json()


def save_irrigation_config(config_path, data):
    """
    Atomically write the irrigation config JSON with file locking.
    Writes to a temp file first, then renames to avoid corruption
    if Node-RED reads mid-write.
    """
    dir_name = os.path.dirname(config_path)
    # Write to temp file in the same directory (ensures same filesystem for rename)
    fd, tmp_path = tempfile.mkstemp(dir=dir_name, suffix=".json.tmp")
    try:
        with os.fdopen(fd, "w") as tmp_file:
            if fcntl:
                fcntl.flock(tmp_file, fcntl.LOCK_EX)
            json.dump(data, tmp_file, indent=2)
            if fcntl:
                fcntl.flock(tmp_file, fcntl.LOCK_UN)
        os.replace(tmp_path, config_path)  # atomic on POSIX
    except Exception:
        # Clean up temp file on failure
        if os.path.exists(tmp_path):
            os.remove(tmp_path)
        raise


def publish_to_influxdb(measurement, tags, fields, config):
    """
    Publish a single data point to InfluxDB 2.x.

    Args:
        measurement: InfluxDB measurement name (e.g., "weather_multiplier")
        tags: dict of tag key-value pairs (e.g., {"location": "Vienna"})
        fields: dict of field key-value pairs (e.g., {"base_factor": 1.2, "rain_mm": 3.5})
        config: monitoring config dict with db_org, db_token, server, port, bucket
    """
    url = config["server"] + config["port"]
    client = InfluxDBClient(url=url, token=config["db_token"], org=config["db_org"])
    try:
        write_api = client.write_api(write_options=SYNCHRONOUS)
        point = Point(measurement)
        for k, v in tags.items():
            point.tag(k, v)
        for k, v in fields.items():
            if isinstance(v, (int, float)):
                point.field(k, float(v))
        write_api.write(bucket=config["bucket"], record=point)
    finally:
        client.close()
