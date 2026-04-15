# Pi Scripts

Python scripts that run on the Raspberry Pi host to compute and push weather-based watering multipliers. They communicate with Node-RED via HTTP — no direct file access needed (Docker compatible).

---

## Scripts

### `calculate_weather_multiplier.py`

Fetches a 5-day forecast from OpenWeatherMap and computes a weather multiplier (`wm`) per irrigation group using Penman-Monteith evapotranspiration. The result is POSTed to Node-RED at `/bewae/update-wm`.

Run twice daily via cron (e.g. 06:00 and 18:00).

### `check_soil_moisture.py`

Queries InfluxDB for recent soil moisture readings. For any group whose moisture sensor reports above the `wet_threshold`, sets `wm = 0` (skip watering) via POST to `/bewae/update-wm`.

Run 5 minutes after `calculate_weather_multiplier.py` so it can override the weather-based value when the soil is already wet.

### `weather_utils.py`

Shared library used by both scripts above. Not run directly. Provides:
- OpenWeatherMap API fetch + Penman-Monteith ET calculation
- HTTP config read/write (`/bewae/get-backendconfig-full`, `/bewae/update-wm`)
- InfluxDB publish helpers
- `LogfireHandler` for remote logging

---

## Configuration — `monitoring_config.JSON`

Fill in before first run. All fields required unless marked optional.

```json
{
  "db_org":              "your-influxdb-org",
  "db_token":            "your-influxdb-token==",
  "server":              "192.168.1.x",
  "port":                ":8086",
  "bucket":              "your-bucket",
  "weatherAPI":          "your-openweathermap-api-key",
  "lat":                 "48.20",
  "lon":                 "16.37",
  "location":            "Vienna",
  "nodered_url":         "http://localhost:1880",
  "logfire_url":         "http://localhost:1880",
  "config_path":         "/home/homepi/bewae/full-config.json",
  "baseline_temp":       25,
  "baseline_humidity":   50,
  "baseline_wind":       5,
  "rain_threshold_mm":   5,
  "rain_hard_cutoff_mm": 15,
  "multiplier_max":      2.0,
  "wet_threshold":       75,
  "sensor_measurement":  "sensor_data"
}
```

| Field | Description |
|---|---|
| `db_org` / `db_token` / `bucket` | InfluxDB 2.0 credentials |
| `server` / `port` | InfluxDB host and port |
| `weatherAPI` | OpenWeatherMap API key (free tier works) |
| `lat` / `lon` / `location` | Location for weather forecast |
| `nodered_url` | Base URL for Node-RED (used for config read/write) |
| `logfire_url` | LogFire hub URL for remote logging |
| `config_path` | Path to `full-config.json` on the Pi (legacy fallback, normally unused) |
| `baseline_temp` / `baseline_humidity` / `baseline_wind` | Reference climate values for ET calculation |
| `rain_threshold_mm` | Forecasted rain above this reduces `wm` |
| `rain_hard_cutoff_mm` | Forecasted rain above this sets `wm = 0` |
| `multiplier_max` | Maximum `wm` value (caps ET-based scaling) |
| `wet_threshold` | Soil moisture % above which `wm` is forced to 0 |
| `sensor_measurement` | InfluxDB measurement name for sensor data |

---

## Cron setup

```cron
0  6  * * *  python3 /home/homepi/bewae/calculate_weather_multiplier.py
0 18  * * *  python3 /home/homepi/bewae/calculate_weather_multiplier.py
5  6  * * *  python3 /home/homepi/bewae/check_soil_moisture.py
5 18  * * *  python3 /home/homepi/bewae/check_soil_moisture.py
```

---

## Legacy / unused scripts

The following scripts remain in the folder but are no longer part of the active system:

| Script | Status |
|---|---|
| `store_weather_data.py` | Old — stored raw OWM data to InfluxDB, superseded |
| `estimate_water_loss.py` | Old — early ET prototype, superseded by `calculate_weather_multiplier.py` |
| `hourlisttoBIN.py` | Utility — converts hour list to 24-bit timetable integer |
| `old-config-to-JSON.py` | Migration tool — converts legacy config format, one-off use |
| `test_weather.py` | Dev/test script for OWM API integration |
