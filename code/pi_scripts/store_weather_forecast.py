########################################################################################################################
# store_weather_forecast.py
# Fetches OpenWeatherMap 5 day / 3 hour forecast data and writes it to InfluxDB.
#
# Measurement: weather_forecast
#
# Timestamps are the forecast target times, not the collection time. This makes Grafana
# future-looking panels straightforward. Repeated runs update the same forecast-time points.
#
# Flags:  --dry-run   Print fetched forecast points without writing to InfluxDB
# Cron:   10 */3 * * *   cd /home/pi/bewae && python3 store_weather_forecast.py
########################################################################################################################

import logging
import sys
from datetime import datetime, timezone

import requests

from weather_utils import load_config
from influx_utils import get_client, get_write_api, write_point

logging.basicConfig(
    filename="weather_forecast.log",
    filemode="a",
    format="%(asctime)s %(levelname)s %(message)s",
    level=logging.INFO,
)
logger = logging.getLogger(__name__)


def fetch_forecast(api_key: str, lat: str, lon: str) -> dict:
    url = "https://api.openweathermap.org/data/2.5/forecast"
    params = {
        "lat": lat,
        "lon": lon,
        "appid": api_key,
        "units": "metric",
    }
    response = requests.get(url, params=params, timeout=15)
    response.raise_for_status()
    return response.json()


def _as_float(value, default=None):
    if isinstance(value, (int, float)) and value == value:
        return float(value)
    return default


def transform_forecast(raw: dict, issued_at: datetime) -> list:
    points = []
    issued_at_unix = issued_at.timestamp()

    for item in raw.get("list", []):
        forecast_unix = item.get("dt")
        if not isinstance(forecast_unix, (int, float)):
            continue

        forecast_time = datetime.fromtimestamp(forecast_unix, tz=timezone.utc).replace(tzinfo=None)
        horizon_hours = (float(forecast_unix) - issued_at_unix) / 3600

        main = item.get("main", {})
        wind = item.get("wind", {})
        clouds = item.get("clouds", {})
        rain = item.get("rain", {})
        snow = item.get("snow", {})

        fields = {
            "issued_at_unix": float(issued_at_unix),
            "horizon_hours": float(horizon_hours),
        }

        mappings = [
            (main, "temp", "temp"),
            (main, "feels_like", "feels_like"),
            (main, "temp_min", "temp_min"),
            (main, "temp_max", "temp_max"),
            (main, "humidity", "humidity"),
            (main, "pressure", "pressure"),
            (wind, "speed", "wind_speed"),
            (wind, "deg", "wind_deg"),
            (wind, "gust", "wind_gust"),
            (clouds, "all", "clouds_all"),
        ]

        for source, source_key, field_key in mappings:
            if isinstance(source, dict):
                value = _as_float(source.get(source_key))
                if value is not None:
                    fields[field_key] = value

        fields["rain_3h"] = _as_float(rain.get("3h"), 0.0) if isinstance(rain, dict) else 0.0
        fields["snow_3h"] = _as_float(snow.get("3h"), 0.0) if isinstance(snow, dict) else 0.0
        fields["pop"] = _as_float(item.get("pop"), 0.0)

        points.append(
            {
                "timestamp": forecast_time,
                "fields": fields,
            }
        )

    if not points:
        raise ValueError("OpenWeatherMap forecast response contained no usable forecast points.")

    return points


def print_preview(location: str, points: list) -> None:
    print("=== Weather Forecast - dry run (no InfluxDB write) ===\n")
    print(f"  Location : {location}")
    print(f"  Points   : {len(points)}")
    print("")

    for point in points[:8]:
        fields = point["fields"]
        print(
            f"  {point['timestamp'].strftime('%Y-%m-%d %H:%M')} UTC  "
            f"{fields.get('temp', 0):>6.1f} C  "
            f"rain {fields.get('rain_3h', 0):>5.1f} mm/3h  "
            f"pop {fields.get('pop', 0) * 100:>5.1f}%  "
            f"wind {fields.get('wind_speed', 0):>4.1f} m/s"
        )

    if len(points) > 8:
        print(f"\n  ... {len(points) - 8} more forecast points")
    print("\nFetch successful.")


def main(config: dict, dry_run: bool) -> None:
    issued_at = datetime.now(timezone.utc)
    raw = fetch_forecast(config["weatherAPI"], config["lat"], config["lon"])
    points = transform_forecast(raw, issued_at)
    location = config.get("forecast_location") or config["location"]

    if dry_run:
        print_preview(location, points)
        return

    bucket = config.get("bucket_env_forecast")
    if not bucket:
        raise ValueError("Missing required config key: bucket_env_forecast")
    if not config.get("db_token_env_forecast"):
        raise ValueError("Missing required config key: db_token_env_forecast")

    client = get_client(config, token_key="db_token_env_forecast")
    write_api = get_write_api(client)
    try:
        for point in points:
            write_point(
                write_api,
                bucket,
                "weather_forecast",
                tags={"location": location, "source": "openweathermap"},
                data=point["fields"],
                timestamp=point["timestamp"],
            )
        logger.info(f"Weather forecast written: {len(points)} points to bucket {bucket}")
    finally:
        client.close()


if __name__ == "__main__":
    dry_run = "--dry-run" in sys.argv
    try:
        config = load_config()
        main(config, dry_run)
    except Exception as e:
        logger.error(f"Fatal error: {e}", exc_info=True)
        if dry_run:
            print(f"FATAL: {e}")
        sys.exit(1)
