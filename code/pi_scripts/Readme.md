# Pi Scripts

Python scripts that run on the Raspberry Pi host to compute and push watering
multipliers. They communicate with Node-RED by HTTP, so they work cleanly when
Node-RED stores `full-config.json` inside Docker.

---

## Scripts

### `calculate_weather_multiplier.py`

Reads stored forecast data from InfluxDB and computes a weather multiplier
(`wm`) per irrigation group. The result is POSTed to Node-RED at
`/bewae/update-wm`.

Run once daily via cron at 06:00. The script uses the next 24 hours of stored
forecast blocks, so the same `wm` applies consistently to the day's watering
windows.

### `store_weather_forecast.py`

Fetches OpenWeatherMap 5-day / 3-hour forecast data and writes it to the
forecast bucket. This gives Bewae its own lightweight forecast collector for
installs that do not run an external forecast system.

Run every 3 hours via cron.

### `check_soil_moisture.py`

Queries InfluxDB for recent soil moisture readings. For any group whose
moisture sensor reports above the `wet_threshold`, sets `wm = 0` to skip
watering.

Run 5 minutes after `calculate_weather_multiplier.py` so it can override the
weather-based value when the soil is already wet.

### `weather_utils.py`

Shared library used by the scripts above. Not run directly. Provides:

- Forecast query helper for InfluxDB
- HTTP config read/write helpers (`/bewae/get-backendconfig-full`, `/bewae/update-wm`)
- InfluxDB publish helpers
- Local file/console logging helpers

### `influx_utils.py`

Small generic helper used by `store_weather_forecast.py` for timestamped writes
to the forecast bucket.

---

## Configuration - `config.json`

Start from the template, then fill in your local tokens, URLs, and location:

```bash
cp config.template.json config.json
```

The intended InfluxDB split is:

- `environment_forecast` stores future OpenWeatherMap forecast rows.
- `bewae` stores Bewae control/diagnostic points such as `weather_multiplier`
  and `soil_moisture_check`.

`bucket_env_forecast` and `db_token_env_forecast` are required for both the
forecast collector and multiplier script. `bucket` / `db_token` should point to
the Bewae bucket used for control diagnostics and soil-moisture checks.

```json
{
  "db_org": "your-influxdb-org",
  "db_token": "your-bewae-bucket-token==",
  "server": "http://192.168.1.x",
  "port": ":8086",
  "bucket": "bewae",
  "weatherAPI": "your-openweathermap-api-key",
  "lat": "47.07",
  "lon": "15.44",
  "location": "Graz",
  "bucket_env_forecast": "environment_forecast",
  "db_token_env_forecast": "your-forecast-rw-token==",
  "forecast_location": "Graz",
  "forecast_source": "openweathermap",
  "forecast_max_age_hours": 4,
  "nodered_url": "http://localhost:1880",
  "config_path": "/home/homepi/bewae/full-config.json",
  "baseline_temp": 25,
  "baseline_humidity": 50,
  "baseline_wind": 5,
  "baseline_cloud_cover": 30,
  "rain_threshold_mm": 5,
  "rain_hard_cutoff_mm": 15,
  "multiplier_max": 2.0,
  "wet_threshold": 75,
  "sensor_measurement": "sensor_data"
}
```

| Field | Description |
|---|---|
| `db_org` / `server` / `port` | InfluxDB 2.x connection |
| `db_token` / `bucket` | Token and bucket used for Bewae control diagnostics and soil-moisture reads |
| `weatherAPI` / `lat` / `lon` | OpenWeatherMap forecast collector inputs |
| `bucket_env_forecast` | Forecast bucket, usually `environment_forecast` |
| `db_token_env_forecast` | Token with write access for the collector and read access for the multiplier |
| `forecast_location` | Forecast location tag, e.g. `Graz` |
| `forecast_source` | Forecast source tag, usually `openweathermap` |
| `forecast_max_age_hours` | Reject forecast rows older than this by `issued_at_unix` |
| `nodered_url` | Base URL for Node-RED config reads and `wm` writes |
| `baseline_temp` / `baseline_humidity` / `baseline_wind` / `baseline_cloud_cover` | Reference weather for `wm = 1.0` |
| `rain_threshold_mm` | Effective forecast rain above this reduces `wm` to 0 |
| `rain_hard_cutoff_mm` | Effective forecast rain above this immediately sets `wm = 0` |
| `multiplier_max` | Maximum weather multiplier |
| `wet_threshold` | Soil moisture percent above which `wm` is forced to 0 |
| `sensor_measurement` | InfluxDB measurement name for sensor data |

---

## Forecast Query Shape

`store_weather_forecast.py` stores forecast points at the forecast target time,
often in the future. The multiplier reads the next 24 hours by querying with a
future-looking range and pivoting forecast fields by `_time`.

```flux
from(bucket: "environment_forecast")
  |> range(start: -1h, stop: 5d)
  |> filter(fn: (r) => r["_measurement"] == "weather_forecast")
  |> filter(fn: (r) => r["location"] == "Graz")
  |> filter(fn: (r) => r["source"] == "openweathermap")
  |> filter(fn: (r) =>
    r["_field"] == "temp" or
    r["_field"] == "humidity" or
    r["_field"] == "pressure" or
    r["_field"] == "wind_speed" or
    r["_field"] == "rain_3h" or
    r["_field"] == "pop" or
    r["_field"] == "clouds_all" or
    r["_field"] == "issued_at_unix"
  )
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
  |> sort(columns: ["_time"])
```

The Python script then uses the first 8 valid 3-hour blocks.

---

## Multiplier Formula V3

The multiplier uses Penman-Monteith reference ET0 from
`estimate_water_loss.reference_et0()`. It computes ET0 for the actual forecast
blocks and for the configured baseline weather over the same timestamps:

```text
drying_ratio = sum(ET0(actual forecast)) / sum(ET0(baseline weather))
```

Rain is summed for the same 24-hour window and weighted by probability of
precipitation:

```text
effective_rain_mm = sum(rain_3h * (0.5 + 0.5 * pop))
rain_factor = 1 - effective_rain_mm / rain_threshold_mm
```

Per plant group:

```text
plant_factor = pls / 100

normal:     wm = clamp(drying_ratio * rain_factor * plant_factor, 0.0, multiplier_max)
under roof: wm = clamp(drying_ratio * plant_factor, 0.0, multiplier_max)
```

`kc` is no longer read by the Pi multiplier. Existing stored `kc` fields can
remain in the config until the Node-RED UI and firmware cleanup pass removes
dead fields.

If effective forecast rain reaches `rain_hard_cutoff_mm`, `rain_factor` becomes
0 and all normal, rain-exposed groups get `wm = 0`. Groups with
`ignore_rain = true` skip the rain factor and still use
`drying_ratio * plant_factor`.

---

## Cron Setup

Start from the cron template:

```bash
cat bewae.cron.template
```

Paste the contents into the Pi user's crontab via `crontab -e`. The default
template contains:

```cron
10 */3 * * *  cd /home/homepi/bewae && /home/homepi/heimdall/venv/bin/python3 store_weather_forecast.py       >> /home/homepi/bewae/cron.log 2>&1
0  6  * * *  cd /home/homepi/bewae && /home/homepi/heimdall/venv/bin/python3 calculate_weather_multiplier.py >> /home/homepi/bewae/cron.log 2>&1
5  6  * * *  cd /home/homepi/bewae && /home/homepi/heimdall/venv/bin/python3 check_soil_moisture.py          >> /home/homepi/bewae/cron.log 2>&1
```

The Python scripts do not contain Heimdall or LogFire integration. The current
Pi install reuses Heimdall's existing virtualenv only as a Python runtime.

---

## Legacy / Unused Scripts

| Script | Status |
|---|---|
| `store_weather_data.py` | Old current-weather collector |
| `estimate_water_loss.py` | V3 ET0 helper plus older standalone estimator entrypoint |
| `hourlisttoBIN.py` | Utility, converts hour list to 24-bit timetable integer |
| `old-config-to-JSON.py` | Migration tool for legacy configs |
| `test_weather.py` | Dev/test script for direct OpenWeatherMap integration |
