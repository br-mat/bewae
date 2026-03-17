# bewae — Automated Irrigation System v3.3

> Sensor-controlled balcony irrigation using ESP32 & Raspberry Pi. *(v3.3 — work in progress)*

---

## Overview

bewae automates the watering of balcony plants based on configurable schedules. An ESP32 reads soil moisture, temperature, humidity, pressure, and light data, stores it in InfluxDB on a Raspberry Pi, and controls irrigation valves and pumps. The system can run standalone or be managed remotely via VPN.

**Key features:**

- Schedule-based irrigation with per-group timing control
- Sensor data storage and visualization (InfluxDB 2.0 + Grafana)
- Web-based configuration via Node-RED
- Solar-powered operation (optional)
- Remote monitoring via VPN (e.g. PiVPN)

## System Diagram

| System Setup                                                                    | Solar Setup                                                                       |
| :-----------------------------------------------------------------------------: | :-------------------------------------------------------------------------------: |
| ![System diagram](docs/pictures/SystemdiagrammV3_3.png)                         | ![Solar diagram](docs/pictures/systemdiagramSolar.png)                            |

---

## Repository Structure

```text
bewae/
├── code/esp_32ard_bewae/         # ESP32 firmware (PlatformIO)
│   ├── src/                      # Main source files
│   └── data/                     # Filesystem configs (SPIFFS)
├── node-red-flows/               # Node-RED flows for Raspberry Pi
│   ├── bewaeConfigPageFlow.json  # Web config page (active)
│   └── flows.json                # Legacy config file server
├── fzz-layout/                   # PCB designs (Fritzing + Gerber)
└── docs/                         # Documentation & images
```

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 (WROOM) |
| Single-board computer | Raspberry Pi 4B (64-bit OS) |
| Sensors | BME280 (temp/humidity/pressure), DS18B20 (soil temp), capacitive soil moisture, LDR |
| Actuators | Up to 10× 12V valves, 1–2× 12V pumps via shift register (74HC595) |
| Power | 20W 12V solar panel, 12Ah lead-acid battery, solar charge controller |

Three PCB layouts are available under [`fzz-layout/`](fzz-layout/):

- **Board 5** (`bewae3_3_board5v5_final.fzz`) — **Current supported setup.** Compact main board, onboard I2C + 1-Wire, 2× 5V / 2× 3V sensor slots, LDR, up to 8 valve/pump outputs (expandable via shift register).
- **Board 1** (`bewae3_3_board1v3_838.fzz`) — Legacy. Large main board with up to 16 analog sensors, requires Board 3 for valve expansion.
- **Board 3** (`bewae3_3_board3v22.fzz`) — Legacy extension board: 16 sensor slots, 10 valves, 2 pumps (12V).

---

## Setup

### Prerequisites

- Visual Studio Code with the [PlatformIO IDE](https://platformio.org/) extension
- Raspberry Pi running a 64-bit OS with [Node-RED](https://nodered.org/docs/getting-started/raspberrypi) and [InfluxDB 2.0](https://docs.influxdata.com/influxdb/v2/install/?t=Raspberry+Pi)

### ESP32 Firmware

1. Open `code/esp_32ard_bewae/` in VS Code with PlatformIO.
2. Edit `src/connection.h` — set your Wi-Fi credentials, Raspberry Pi IP, InfluxDB token, and NTP server.
3. In `src/config.h`, select the hardware class matching your PCB board:

   ```cpp
   #define HW_BOARD Helper_config1_Board5v5  // or your board variant
   ```

   Supported values: `Helper_config1_Board5v5` (current), `Helper_config1_Board1v3838` (legacy).

4. Build and upload the firmware via the PlatformIO toolbar (arrow icon).
5. Upload the filesystem: PlatformIO → **Upload Filesystem Image** (uploads `data/` to SPIFFS).

> If no network is available, the firmware still runs using locally stored config. Time must be set manually via `Helper::set_time()`.

### Raspberry Pi — Node-RED

1. Access Node-RED at `http://<Pi-IP>:1880`.
2. Import [`node-red-flows/bewaeConfigPageFlow.json`](node-red-flows/bewaeConfigPageFlow.json) via the Node-RED menu → **Import**.
3. In the flow, update the **read-file** and **write-file** nodes to point to your `config.JSON` path on the Pi.
4. Deploy the flow.

The web config page is then available at:

```text
http://<Pi-IP>:1880/bewae-working
```

It allows you to view and edit irrigation groups, schedules, and sensor settings directly in the browser — no re-flashing required.

### Raspberry Pi — InfluxDB 2.0

1. Visit `http://localhost:8086` and complete initial setup (user, org, bucket).
2. Generate an API token: **Load Data → API Tokens → Generate API Token → Custom** (Write permission).
3. Enter the token in `src/connection.h`.

---

## Configuration

All system settings are stored in `config.JSON` (on the ESP32 SPIFFS or served via Node-RED).

### Irrigation Groups

Each group defines a set of valves/pins and a watering schedule:

```json
"group": {
  "Tomatoes": {
    "is_set": 1,
    "vpins": [0, 5],
    "water-time": 10,
    "timetable": 1049600
  }
}
```

- **`vpins`** — virtual pin numbers mapped to valves/pumps
- **`water-time`** — duration in seconds per watering cycle
- **`timetable`** — 24-bit integer; each bit represents one hour of the day

```cpp
// Bit layout (hours 0–23):
// 0b00000000000100000000010000000000  →  waters at hour 10 and 20
```

### Sensors

Sensors are identified by unique IDs and configured with a mode:

```json
"sensor": {
  "id00": { "name": "bme280", "field": "temp", "mode": "bmetemp" },
  "id08": { "name": "Soil", "field": "moisture", "mode": "vanalog", "vpin": 15, "hlim": 600, "llim": 250 }
}
```

| Mode        | Description                  | Requires         |
| ----------- | ---------------------------- | ---------------- |
| `analog`    | Reads analog pin             | `pin`            |
| `vanalog`   | Reads virtual analog pin     | `vpin`           |
| `bmetemp`   | BME280 temperature           | BME280 on I2C    |
| `bmehum`    | BME280 humidity              | BME280 on I2C    |
| `bmepress`  | BME280 pressure              | BME280 on I2C    |
| `soiltemp`  | DS18B20 soil temperature     | 1-Wire bus       |

Optional modifiers: `add` (offset), `fac` (factor), `hlim`/`llim` (range for % output).

---

## Images

| V3 (Summer)                                                         | V3 (Box)                                                        | Main PCB                                    |
| :-----------------------------------------------------------------: | :-------------------------------------------------------------: | :-----------------------------------------: |
| ![V3 Summer](docs/pictures/bewaeV3%28Sommer%29.jpg)                 | ![V3 Box](docs/pictures/bewaeV3%28Box%29.jpg)                   | ![Main PCB](docs/pictures/MainPCB.jpg)      |

---

## Roadmap

See [roadmap.md](roadmap.md)
