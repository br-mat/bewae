# weather_utils.py
# Shared utilities for bewae weather-based irrigation scripts
# by br-mat (c) 2025

import json
import logging
import os
import tempfile
import fcntl
import requests
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

# Default paths
DEFAULT_CONFIG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "monitoring_config.JSON")
REQUEST_TIMEOUT = 10  # seconds


class LogfireHandler(logging.Handler):
    """POST log records to Logfire /log endpoint. Fire-and-forget."""
    LEVEL_MAP = {logging.INFO: 1, logging.WARNING: 2, logging.ERROR: 3, logging.CRITICAL: 4}

    def __init__(self, url, device_name, timeout=2):
        super().__init__()
        self._url = url.rstrip("/") + "/log"
        self._device = device_name
        self._timeout = timeout

    def emit(self, record):
        try:
            level = self.LEVEL_MAP.get(record.levelno, 0)
            tag = f"(-{level})" if level > 0 else ""
            body = f"{self._device}{tag}: {record.getMessage()}"
            requests.post(self._url, data=body,
                          headers={"Content-Type": "text/plain"}, timeout=self._timeout)
        except Exception:
            pass


def setup_logger(name, log_file=None, level=logging.INFO,
                 logfire_url=None, device_name=None):
    """Set up a logger with file, console, and optional Logfire remote output."""
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

    # Logfire remote handler (optional)
    if logfire_url and device_name:
        lf = LogfireHandler(logfire_url, device_name)
        lf.setLevel(logging.INFO)
        logger.addHandler(lf)

    return logger


def load_config(path=None):
    """Load monitoring_config.JSON with basic validation."""
    path = path or DEFAULT_CONFIG_PATH
    with open(path, "r") as f:
        config = json.load(f)

    required_keys = ["weatherAPI", "lat", "lon"]
    for key in required_keys:
        if key not in config:
            raise KeyError(f"Missing required config key: '{key}'")

    return config


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
            fcntl.flock(tmp_file, fcntl.LOCK_EX)
            json.dump(data, tmp_file, indent=2)
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
