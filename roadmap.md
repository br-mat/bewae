# Roadmap

## Near-term

- **Cron setup on Pi** — write crontab entries for `calculate_weather_multiplier.py` (2x daily) and `check_soil_moisture.py` (5 min after weather)
- **`_ts` timestamp** — Node-RED save endpoint stamps config with ISO timestamp + counter suffix; ESP32 checks a lightweight endpoint and skips the full config download when unchanged (reduces WiFi uptime per cycle)
- **Push devBranch + Pi+WiFi end-to-end test** — override flow and crash recovery on full hardware

## Future features

- **Main loop unblocking** — calculate safe watering window to avoid sensor interval conflicts, allowing sensor reads between watering pulses
- **Docker Compose deployment docs** — document the full Pi deployment (Node-RED in Docker, InfluxDB, Grafana, cron jobs)
- **Schema docs for full-config.json** — document the server-side config structure

## Refactoring / code quality

- Pass `DynamicJsonDocument` by reference in `writeConfigFile` — currently copies 8 KB on every call
- Consolidate the three BME280 sensor handlers into one parameterised function
- Replace magic delay values with named constants
- WiFi reconnect — consider exponential backoff or configurable timeout

## Ideas (no timeline)

- Display + interface menu on device
- App for configuration (sensors and groups)
- Bluetooth config upload
- Machine learning for watering decisions based on collected data
- NVS instead of SPIFFS for `running.Json` runtime state (better wear levelling)
