# bewae — Configuration Reference

This document covers the manual configuration of the bewae system. Under normal use, all settings are managed through the [web config page](README.md#web-configuration). This reference is for direct file editing, debugging, or initial setup.

---

## Config Files on the ESP32 (SPIFFS)

The ESP32 stores its configuration in the internal flash filesystem (SPIFFS). Files are located in `code/esp_32ard_bewae/data/` and uploaded via PlatformIO → **Upload Filesystem Image**.

| File | Purpose |
|---|---|
| `deviceConfig.Json` | System switches and device name |
| `plantConfig.Json` | Irrigation groups and schedules |
| `sensorConfig.Json` | Sensor definitions |
| `running.Json` | Runtime state (managed automatically) |

The ESP32 polls the Raspberry Pi for updated versions of these files on each wake cycle and overwrites the local copies if the server version has changed.

---

## deviceConfig.Json

Controls the device identity and system switches.

```json
{
  "dn": "MyDevice",
  "main": 1,
  "irig": 1,
  "mssr": 1,
  "dmmy": 0
}
```

| Field | Type | Description |
|---|---|---|
| `dn` | string | Device name — must match the name configured in Node-RED |
| `main` | bool | Master switch — enables/disables the entire system |
| `irig` | bool | Irrigation switch — enables/disables watering |
| `mssr` | bool | Datalogging switch — enables/disables sensor measurements |
| `dmmy` | bool | Placeholder switch (unused) |

---

## plantConfig.Json

Defines irrigation groups. Each key is a numeric group index.

```json
{
  "0": {
    "pn": "Tomatoes",
    "ps": 1,
    "pp": [0, 5],
    "pw": 20,
    "wt": 262208,
    "pls": 2,
    "pts": 1.5,
    "kc": 1.05,
    "ignore_rain": false,
    "moisture_sensor": "moisture",
    "wm": 1.0
  }
}
```

| Field | Type | Description |
|---|---|---|
| `pn` | string | Group name |
| `ps` | bool | Active state — 1 to include in watering cycles |
| `pp` | array | Virtual pin numbers mapped to valve/pump outputs |
| `pw` | int | Watering duration in seconds per cycle |
| `wt` | int | Timetable — 24-bit integer, each bit is one hour (bit 0 = midnight) |
| `pls` | int | Plant size (informational, used for ET scaling) |
| `pts` | float | Pot size (informational, used for ET scaling) |
| `kc` | float | Crop coefficient (FAO-56) — scales ET per plant type (tomatoes ~1.05, herbs ~0.7) |
| `ignore_rain` | bool | If true, rain forecast does not reduce `wm` for this group |
| `moisture_sensor` | string | InfluxDB field name of the moisture sensor for this group (`"none"` to disable) |
| `wm` | float | Weather multiplier — Pi-computed, range 0.0–2.0. Actual watering time = `pw × wm` |

### Timetable examples

```
Hour:       23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
Bit:        23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0

Water at 6:00 and 18:00  →  0b000000001000000001000000  =  262208
Water at 8:00 only       →  0b000000000000000100000000  =  256
```

The script `code/pi_scripts/hourlisttoBIN.py` can convert a list of hours to the correct integer value.

---

## sensorConfig.Json

Defines sensor measurement points. Each key is a numeric sensor index.

```json
{
  "0": { "sn": "bme280",    "sf": "temperature", "sm": "bmetemp",  "sp": 0,  "hl": 0, "ll": 0, "ss": 1 },
  "1": { "sn": "bme280",    "sf": "humidity",    "sm": "bmehum",   "sp": 0,  "hl": 0, "ll": 0, "ss": 1 },
  "2": { "sn": "bme280",    "sf": "pressure",    "sm": "bmepress", "sp": 0,  "hl": 0, "ll": 0, "ss": 1 },
  "3": { "sn": "SoilTemp",  "sf": "soiltemp",    "sm": "soiltemp", "sp": 0,  "hl": 0, "ll": 0, "ss": 1 },
  "4": { "sn": "Soil",      "sf": "moisture",    "sm": "vanalog",  "sp": 15, "hl": 600, "ll": 250, "ss": 1 }
}
```

| Field | Type | Description |
|---|---|---|
| `sn` | string | Sensor name |
| `sf` | string | InfluxDB field name — what gets stored |
| `sm` | string | Sensor mode — how the firmware reads the hardware (see table below) |
| `sp` | int | Pin number (GPIO pin for `analog`, MUX channel for `vanalog`) |
| `hl` | int | Raw value mapped to 100% (for percentage output) |
| `ll` | int | Raw value mapped to 0% (for percentage output) |
| `ss` | bool | Sensor active — 0 disables this measurement point |

When both `hl` and `ll` are non-zero, the output is a percentage between 0–100.

### Sensor modes

| Mode | Description |
|---|---|
| `bmetemp` | BME280 temperature |
| `bmehum` | BME280 humidity |
| `bmepress` | BME280 pressure |
| `soiltemp` | DS18B20 soil temperature (1-Wire) |
| `analog` | Direct GPIO analog pin read (`sp` = GPIO number) |
| `vanalog` | MUX channel via shift register (`sp` = channel number) |

---

## Virtual Pins (vpins)

Virtual pins are output channels mapped through the shift register (74HC595). The mapping depends on the board version:

- **Board 5**: up to 8 outputs per shift register chain, expandable
- Pin 0 is the first output, pin N−1 is the last

Multiple groups can share the same vpin (e.g. a pump shared by several valve groups). The firmware ensures only one group is active at a time.

---

## connection.h — Network & Service Settings

Edit `src/connection.h` to set credentials. All values can be overridden via `platformio.ini` build flags without touching the file directly.

```cpp
#define ssid           "your-wifi-ssid"
#define wifi_password  "your-wifi-password"
#define SERVER         "192.168.1.x"     // Raspberry Pi IP
#define NODERED_PORT   1880              // Node-RED port
#define WEB_PREFIX     "/bewae/get-config"

#define NTP_Server     "at.pool.ntp.org"

#define INFLUXDB_URL   "http://192.168.1.x:8086"
#define INFLUXDB_DB_NAME "your-bucket"
#define INFLUXDB_TOKEN "your-token"
#define INFLUXDB_ORG   "your-org"

#define DEVICE_NAME    "MyDevice"        // must match dn in deviceConfig.Json
```

---

## config.h — Firmware Constants

Key constants in `src/config.h` that affect system behaviour:

| Constant | Default | Description |
|---|---|---|
| `HW_BOARD` | `Helper_config1_Board5v5` | Hardware class — select your PCB variant |
| `max_groups` | `32` | Maximum number of irrigation groups |
| `max_active_time_sec` | `50` | Hard limit for solenoid on-time (seconds) |
| `measure_intervall` | `600000` | Sensor measurement interval (ms, default 10 min) |
| `SOLENOID_COOLDOWN` | `30000` | Minimum time between valve activations (ms) |
| `INVERT_SHIFTOUT` | `true` | Set true for negative-logic (optocoupler) relay boards |
| `PUBDATA` | `true` | Enable InfluxDB publishing |
| `DEBUG` | `1` | Enable serial debug output |

---

## Pi Scripts — monitoring_config.JSON

See [`code/pi_scripts/Readme.md`](code/pi_scripts/Readme.md) for the full field reference and cron setup.
