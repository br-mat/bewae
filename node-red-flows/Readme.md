# Node-RED Flows

## Deployment files

### `bewaeConfigPageFlow.json`
The production flow. Import this into Node-RED on the Pi.

The HTML config page is embedded directly as a template node — no separate file deployment needed.

**Endpoints:**

| Endpoint | Method | Description |
|---|---|---|
| `/bewae-working` | GET | Serves the config web page (HTML embedded in flow) |
| `/bewae/get-backendconfig-full` | GET | Returns the full `full-config.json` |
| `/bewae/save-backendconfig-full` | POST | Saves the full config, preserving Pi-computed `wm` values |
| `/bewae/get-config` | GET | Returns config filtered by `?deviceName=` and/or `?fileType=` (used by ESP32) |
| `/bewae/update-wm` | POST | Updates weather multiplier values per plant group (called by Pi scripts) |

**Config file path (Docker):** `/data/bewae/full-config.json` inside the container. Update the `file in` / `file` nodes if your volume is mounted differently.

---

### `bewae-config.html`
Source for the config web page. Edit this file, then re-run `build_flow.py` to embed the updated HTML into `bewaeConfigPageFlow.json`.

---

## Build tooling

### `build_flow.py`
Reads `bewae-config.html` and generates `bewaeConfigPageFlow.json` with the HTML embedded as a template node. Run from the repo root:

```bash
python node-red-flows/build_flow.py
```

---

## Security & network assumptions

This system is designed for use on a **trusted local network (LAN) only**.

- API endpoints have **no authentication** — any client on the network can read/write config
- Communication uses **HTTP** (no TLS/HTTPS)
- No CSRF protection on POST endpoints
- Server-side input validation is minimal (client-side validation enforced in the web UI)

These are intentional design choices for a home automation system. **Do not expose Node-RED or these endpoints to the public internet** without adding an authentication layer (e.g., Node-RED `httpNodeAuth`, reverse proxy with auth, or VPN-only access).
