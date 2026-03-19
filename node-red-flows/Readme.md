# Node-RED Flows

## Deployment files

### `bewaeConfigPageFlow.json`
The production flow. Import this into Node-RED on the Pi.

**Tabs / endpoints:**

| Endpoint | Method | Description |
|---|---|---|
| `/bewae-working` | GET | Serves the config web page (reads `bewae-config.html` from disk) |
| `/bewae/get-backendconfig-full` | GET | Returns the full `full-config.json` |
| `/bewae/save-backendconfig-full` | POST | Saves the full config, preserving Pi-computed `wm` values |
| `/bewae/get-config` | GET | Returns config filtered by `?deviceName=` and/or `?fileType=` (used by ESP32) |
| `/bewae/update-wm` | POST | Updates weather multiplier values per plant group (called by Pi scripts) |

**File paths to update before deploying:**
- All `file in` / `file` nodes reference `/home/homepi/bewae/full-config.json`
- The config page node references `/home/homepi/bewae/www/bewae-config.html`

Update these to match your Pi's actual paths.

---

### `bewae-config.html`
The config web page served by the flow above. Deploy to the path configured in the `file in` node (default: `/home/homepi/bewae/www/bewae-config.html`).

---

## Build tooling (development only)

### `build_reworkflow.py`
Generates the reworkbuild test flow JSON with the HTML embedded as a template node (useful for testing without file access on the Pi). Run from the repo root:

```bash
python node-red-flows/build_reworkflow.py
```

Output files (`bewaeConfigPageFlow_reworkbuild.json`, `bewae-config_reworkbuild.html`) are gitignored — they are local build artifacts.

---

## Security & network assumptions

This system is designed for use on a **trusted local network (LAN) only**.

- API endpoints have **no authentication** — any client on the network can read/write config
- Communication uses **HTTP** (no TLS/HTTPS)
- No CSRF protection on POST endpoints
- Server-side input validation is minimal (client-side validation enforced in the web UI)

These are intentional design choices for a home automation system. **Do not expose Node-RED or these endpoints to the public internet** without adding an authentication layer (e.g., Node-RED `httpNodeAuth`, reverse proxy with auth, or VPN-only access).
