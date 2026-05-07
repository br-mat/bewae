#!/usr/bin/env python3
"""Build the production flow JSON with HTML embedded as a template node."""
import argparse
import json

parser = argparse.ArgumentParser(description='Build bewae Node-RED flow JSON.')
parser.add_argument('--output', default='node-red-flows/bewaeConfigPageFlow.json',
                    help='Output flow JSON path (default: production file)')
parser.add_argument('--timezone', default='Europe/Vienna',
                    help='IANA timezone for _ts values (default: Europe/Vienna)')
args = parser.parse_args()

with open('node-red-flows/bewae-config.html', 'r', encoding='utf-8') as f:
    html = f.read()

# Shared JS helpers reused across function nodes that stamp _ts.
ts_helpers_js = """function getBewaeTimezone() {
    var tz = 'Europe/Vienna';
    try {
        if (typeof env !== 'undefined' && env.get) {
            tz = env.get('BEWAE_TIMEZONE') || tz;
        }
    } catch (e) {}
    return tz;
}
function offsetString(offsetMinutes) {
    var sign = offsetMinutes >= 0 ? '+' : '-';
    var abs = Math.abs(offsetMinutes);
    var pad = function(n) { return n < 10 ? '0' + n : '' + n; };
    return sign + pad(Math.floor(abs / 60)) + ':' + pad(abs % 60);
}
function zonedTimestamp(date, timezone) {
    var pad = function(n) { return n < 10 ? '0' + n : '' + n; };
    var parts;
    try {
        parts = new Intl.DateTimeFormat('en-GB', {
            timeZone: timezone,
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hourCycle: 'h23'
        }).formatToParts(date);
    } catch (e) {
        timezone = 'Europe/Vienna';
        parts = new Intl.DateTimeFormat('en-GB', {
            timeZone: timezone,
            year: 'numeric',
            month: '2-digit',
            day: '2-digit',
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit',
            hourCycle: 'h23'
        }).formatToParts(date);
    }
    var p = {};
    parts.forEach(function(part) {
        if (part.type !== 'literal') p[part.type] = part.value;
    });
    var asUtc = Date.UTC(Number(p.year), Number(p.month) - 1, Number(p.day),
                         Number(p.hour), Number(p.minute), Number(p.second));
    var offsetMinutes = Math.round((asUtc - date.getTime()) / 60000);
    return p.year + '-' + p.month + '-' + p.day + 'T' + p.hour + ':' + p.minute + ':' + p.second
         + offsetString(offsetMinutes);
}
function makeTs(prevTs) {
    var base = zonedTimestamp(new Date(), getBewaeTimezone());
    if (typeof prevTs === 'string') {
        var m = prevTs.match(/^(.+?)(?:~(\\d+))?$/);
        if (m && m[1] === base) {
            return base + '~' + (m[2] ? parseInt(m[2], 10) + 1 : 1);
        }
    }
    return base;
}
function deviceContent(dev) {
    if (!dev) return '';
    var copy = Object.assign({}, dev);
    delete copy._ts;
    return JSON.stringify(copy);
}"""

merge_wm_func = ts_helpers_js + """

const configPath = '/data/bewae/full-config.json';
let currentConfig = {};
try {
    currentConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));
} catch(e) {
    node.warn('Could not read existing config: ' + e.message);
}

const incoming = msg.payload;

for (const [deviceName, device] of Object.entries(incoming)) {
    if (!device || !device.plantConfig) continue;
    const currentDevice = currentConfig[deviceName];

    if (currentDevice && currentDevice.plantConfig) {
        for (const [groupKey, group] of Object.entries(device.plantConfig)) {
            const currentGroup = currentDevice.plantConfig[groupKey];
            if (currentGroup && currentGroup.wm !== undefined) {
                group.wm = currentGroup.wm;
            }
        }
    }

    // Bump _ts only if device content actually changed; otherwise keep prior _ts.
    const prevTs = currentDevice ? currentDevice._ts : undefined;
    if (currentDevice && deviceContent(device) === deviceContent(currentDevice)) {
        if (prevTs) device._ts = prevTs;
    } else {
        device._ts = makeTs(prevTs);
    }
}

msg.payload = incoming;
return msg;"""

filter_func = """var deviceName = msg.req.query.deviceName;
var fileType = msg.req.query.fileType;

try {
    var config = JSON.parse(msg.payload);

    function returnEmpty() {
        msg.payload = {};
        return msg;
    }

    if (!deviceName && !fileType) {
        msg.payload = config;
        return msg;
    }

    if (deviceName && !fileType) {
        if (config[deviceName]) {
            msg.payload = config[deviceName];
            return msg;
        } else {
            return returnEmpty();
        }
    }

    if (deviceName && fileType) {
        if (config[deviceName] && config[deviceName][fileType]) {
            msg.payload = config[deviceName][fileType];
            return msg;
        } else {
            return returnEmpty();
        }
    }

    return returnEmpty();
} catch (error) {
    return returnEmpty();
}"""

update_wm_func = ts_helpers_js + """

var configPath = '/data/bewae/full-config.json';
var currentConfig = {};
try {
    currentConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));
} catch(e) {
    node.error('Could not read config: ' + e.message);
    msg.payload = { error: 'Failed to read config', detail: e.message };
    msg.statusCode = 500;
    return msg;
}

var wmUpdates = msg.payload;
var updated = 0;
var changedDevices = {};
var deviceNames = Object.keys(wmUpdates);

for (var i = 0; i < deviceNames.length; i++) {
    var deviceName = deviceNames[i];
    var groups = wmUpdates[deviceName];
    if (!currentConfig[deviceName] || !currentConfig[deviceName].plantConfig) {
        node.warn('Device not found in config: ' + deviceName);
        continue;
    }
    var groupKeys = Object.keys(groups);
    for (var j = 0; j < groupKeys.length; j++) {
        var groupKey = groupKeys[j];
        if (currentConfig[deviceName].plantConfig[groupKey]) {
            var oldVal = currentConfig[deviceName].plantConfig[groupKey].wm;
            var newVal = groups[groupKey];
            if (oldVal !== newVal) {
                currentConfig[deviceName].plantConfig[groupKey].wm = newVal;
                updated++;
                changedDevices[deviceName] = true;
            }
        } else {
            node.warn('Group not found: ' + deviceName + '/' + groupKey);
        }
    }
}

Object.keys(changedDevices).forEach(function(d) {
    currentConfig[d]._ts = makeTs(currentConfig[d]._ts);
});

msg.payload = currentConfig;
msg.wmUpdated = updated;
return msg;"""

clear_override_func = ts_helpers_js + """

var configPath = '/data/bewae/full-config.json';
var currentConfig = {};
try {
    currentConfig = JSON.parse(fs.readFileSync(configPath, 'utf8'));
} catch(e) {
    node.error('Could not read config: ' + e.message);
    msg.payload = { error: 'Failed to read config', detail: e.message };
    msg.statusCode = 500;
    return msg;
}

var clearRequests = msg.payload;
var cleared = 0;
var changedDevices = {};
var deviceNames = Object.keys(clearRequests);

for (var i = 0; i < deviceNames.length; i++) {
    var deviceName = deviceNames[i];
    var groupKeys = clearRequests[deviceName];
    if (!currentConfig[deviceName] || !currentConfig[deviceName].overrideConfig) {
        continue;
    }
    if (!Array.isArray(groupKeys)) groupKeys = [groupKeys];
    for (var j = 0; j < groupKeys.length; j++) {
        var gk = String(groupKeys[j]);
        if (currentConfig[deviceName].overrideConfig[gk]) {
            delete currentConfig[deviceName].overrideConfig[gk];
            cleared++;
            changedDevices[deviceName] = true;
        }
    }
}

Object.keys(changedDevices).forEach(function(d) {
    currentConfig[d]._ts = makeTs(currentConfig[d]._ts);
});

msg.payload = currentConfig;
msg.overrideCleared = cleared;
return msg;"""

get_ts_func = """var deviceName = msg.req.query.deviceName;
try {
    var config = JSON.parse(msg.payload);
    if (!deviceName || !config[deviceName]) {
        msg.payload = { _ts: "" };
        return msg;
    }
    msg.payload = { _ts: config[deviceName]._ts || "" };
    return msg;
} catch (e) {
    msg.payload = { _ts: "" };
    return msg;
}"""

error_func = 'msg.payload = {\n    error: "Config file operation failed",\n    detail: msg.error ? msg.error.message : "Unknown error"\n};\nmsg.statusCode = 500;\nreturn msg;'

test_data = json.dumps({
    "default": {
        "plantConfig": {
            "0": {"pn": "Big Tomato", "pw": 20, "pls": 3, "pts": 30, "wt": 25, "pp": [1], "ps": 1, "kc": 1.05, "ignore_rain": False, "moisture_sensor": "moisture"},
            "1": {"pn": "Red Rose", "pw": 15, "pls": 2, "pts": 20, "wt": 128, "pp": [2, 3], "ps": 1, "kc": 0.8, "ignore_rain": True, "moisture_sensor": ""}
        },
        "sensorConfig": {
            "0": {"sn": "bme280", "sm": "bmetemp", "sf": "temperature", "sp": 0, "hl": 50, "ll": 0, "ss": 1},
            "1": {"sn": "Soil", "sm": "vanalog", "sf": "moisture", "sp": 15, "hl": 600, "ll": 250, "ss": 1}
        },
        "deviceConfig": {"dn": "Test", "mssr": 1, "main": 1, "irig": 1}
    }
})

TAB = "rw_tab"

flow = [
    {"id": TAB, "type": "tab", "label": "bewae-config", "disabled": False, "info": "", "env": [
        {"name": "BEWAE_TIMEZONE", "type": "str", "value": args.timezone}
    ]},

    # --- Config Page ---
    {"id": "rw_c1", "type": "comment", "z": TAB, "name": "Config Page (template-embedded)", "info": "", "x": 300, "y": 60, "wires": []},
    {"id": "rw_http_page", "type": "http in", "z": TAB, "name": "config page", "url": "/bewae-working", "method": "get", "upload": False, "swaggerDoc": "", "x": 200, "y": 100, "wires": [["rw_tpl"]]},
    {"id": "rw_tpl", "type": "template", "z": TAB, "name": "bewae-config.html", "field": "payload", "fieldType": "msg", "format": "html", "syntax": "plain", "template": html, "output": "str", "x": 460, "y": 100, "wires": [["rw_resp_page"]]},
    {"id": "rw_resp_page", "type": "http response", "z": TAB, "name": "serve page", "statusCode": "200", "headers": {"content-type": "text/html"}, "x": 700, "y": 100, "wires": []},

    # --- Backend Config API ---
    {"id": "rw_c2", "type": "comment", "z": TAB, "name": "Backend Config API (full-config.json)", "info": "", "x": 280, "y": 160, "wires": []},
    {"id": "rw_get", "type": "http in", "z": TAB, "name": "[get] full config", "url": "/bewae/get-backendconfig-full", "method": "get", "upload": False, "swaggerDoc": "", "x": 210, "y": 200, "wires": [["rw_read1"]]},
    {"id": "rw_read1", "type": "file in", "z": TAB, "name": "read config", "filename": "/data/bewae/full-config.json", "filenameType": "str", "format": "utf8", "chunk": False, "sendError": True, "encoding": "none", "allProps": False, "x": 440, "y": 200, "wires": [["rw_json1"]]},
    {"id": "rw_json1", "type": "json", "z": TAB, "name": "", "property": "payload", "action": "", "pretty": False, "x": 610, "y": 200, "wires": [["rw_resp_get", "rw_dbg_get"]]},
    {"id": "rw_resp_get", "type": "http response", "z": TAB, "name": "return config", "statusCode": "", "headers": {}, "x": 830, "y": 200, "wires": []},
    {"id": "rw_dbg_get", "type": "debug", "z": TAB, "name": "debug GET", "active": False, "tosidebar": True, "console": False, "tostatus": False, "complete": "payload", "targetType": "msg", "statusVal": "", "statusType": "auto", "x": 830, "y": 160, "wires": []},

    # --- POST save ---
    {"id": "rw_post", "type": "http in", "z": TAB, "name": "[post] save config", "url": "/bewae/save-backendconfig-full", "method": "post", "upload": False, "swaggerDoc": "", "x": 220, "y": 280, "wires": [["rw_merge"]]},
    {"id": "rw_merge", "type": "function", "z": TAB, "name": "merge wm", "func": merge_wm_func, "outputs": 1, "timeout": "", "noerr": 0, "initialize": "", "finalize": "", "libs": [{"var": "fs", "module": "fs"}], "x": 440, "y": 280, "wires": [["rw_json2"]]},
    {"id": "rw_json2", "type": "json", "z": TAB, "name": "stringify", "property": "payload", "action": "str", "pretty": True, "x": 620, "y": 300, "wires": [["rw_write"]]},
    {"id": "rw_write", "type": "file", "z": TAB, "name": "write config", "filename": "/data/bewae/full-config.json", "filenameType": "str", "appendNewline": True, "createDir": False, "overwriteFile": "true", "encoding": "utf8", "x": 790, "y": 300, "wires": [["rw_resp_post", "rw_dbg_post"]]},
    {"id": "rw_resp_post", "type": "http response", "z": TAB, "name": "200 OK", "statusCode": "200", "headers": {}, "x": 960, "y": 280, "wires": []},
    {"id": "rw_dbg_post", "type": "debug", "z": TAB, "name": "debug POST", "active": False, "tosidebar": True, "console": False, "tostatus": False, "complete": "false", "statusVal": "", "statusType": "auto", "x": 970, "y": 320, "wires": []},

    # --- Test inject ---
    {"id": "rw_inject", "type": "inject", "z": TAB, "name": "test data", "props": [{"p": "payload"}], "repeat": "", "crontab": "", "once": False, "onceDelay": 0.1, "topic": "", "payload": test_data, "payloadType": "json", "x": 420, "y": 340, "wires": [["rw_json2"]]},

    # --- ESP32 Config API ---
    {"id": "rw_c3", "type": "comment", "z": TAB, "name": "ESP32 Config API (filtered by device/fileType)", "info": "", "x": 300, "y": 380, "wires": []},
    {"id": "rw_get_esp", "type": "http in", "z": TAB, "name": "[get] device config", "url": "/bewae/get-config", "method": "get", "upload": False, "swaggerDoc": "", "x": 210, "y": 420, "wires": [["rw_read2"]]},
    {"id": "rw_read2", "type": "file in", "z": TAB, "name": "read config", "filename": "/data/bewae/full-config.json", "filenameType": "str", "format": "utf8", "chunk": False, "sendError": True, "encoding": "none", "allProps": False, "x": 440, "y": 420, "wires": [["rw_filt"]]},
    {"id": "rw_filt", "type": "function", "z": TAB, "name": "filter config", "func": filter_func, "outputs": 1, "timeout": "", "noerr": 0, "initialize": "", "finalize": "", "libs": [], "x": 640, "y": 420, "wires": [["rw_resp_esp"]]},
    {"id": "rw_resp_esp", "type": "http response", "z": TAB, "name": "return filtered", "statusCode": "", "headers": {}, "x": 840, "y": 420, "wires": []},

    # --- Update WM API (Pi scripts POST wm values) ---
    {"id": "rw_c5", "type": "comment", "z": TAB, "name": "Update WM API (Pi scripts POST wm values)", "info": "", "x": 300, "y": 480, "wires": []},
    {"id": "rw_post_wm", "type": "http in", "z": TAB, "name": "[post] update wm", "url": "/bewae/update-wm", "method": "post", "upload": False, "swaggerDoc": "", "x": 210, "y": 520, "wires": [["rw_wm_merge"]]},
    {"id": "rw_wm_merge", "type": "function", "z": TAB, "name": "merge wm updates", "func": update_wm_func, "outputs": 1, "timeout": "", "noerr": 0, "initialize": "", "finalize": "", "libs": [{"var": "fs", "module": "fs"}], "x": 440, "y": 520, "wires": [["rw_wm_json"]]},
    {"id": "rw_wm_json", "type": "json", "z": TAB, "name": "stringify", "property": "payload", "action": "str", "pretty": True, "x": 620, "y": 520, "wires": [["rw_wm_write"]]},
    {"id": "rw_wm_write", "type": "file", "z": TAB, "name": "write config", "filename": "/data/bewae/full-config.json", "filenameType": "str", "appendNewline": True, "createDir": False, "overwriteFile": "true", "encoding": "utf8", "x": 790, "y": 520, "wires": [["rw_wm_resp", "rw_wm_dbg"]]},
    {"id": "rw_wm_resp", "type": "http response", "z": TAB, "name": "200 OK", "statusCode": "200", "headers": {}, "x": 960, "y": 500, "wires": []},
    {"id": "rw_wm_dbg", "type": "debug", "z": TAB, "name": "debug WM", "active": False, "tosidebar": True, "console": False, "tostatus": False, "complete": "true", "statusVal": "", "statusType": "auto", "x": 960, "y": 540, "wires": []},

    # --- Clear Override API (ESP32 clears consumed overrides) ---
    {"id": "rw_c6", "type": "comment", "z": TAB, "name": "Clear Override API (ESP32 clears consumed overrides)", "info": "", "x": 320, "y": 580, "wires": []},
    {"id": "rw_post_clr", "type": "http in", "z": TAB, "name": "[post] clear override", "url": "/bewae/clear-override", "method": "post", "upload": False, "swaggerDoc": "", "x": 220, "y": 620, "wires": [["rw_clr_fn"]]},
    {"id": "rw_clr_fn", "type": "function", "z": TAB, "name": "clear overrides", "func": clear_override_func, "outputs": 1, "timeout": "", "noerr": 0, "initialize": "", "finalize": "", "libs": [{"var": "fs", "module": "fs"}], "x": 440, "y": 620, "wires": [["rw_clr_json"]]},
    {"id": "rw_clr_json", "type": "json", "z": TAB, "name": "stringify", "property": "payload", "action": "str", "pretty": True, "x": 620, "y": 620, "wires": [["rw_clr_write"]]},
    {"id": "rw_clr_write", "type": "file", "z": TAB, "name": "write config", "filename": "/data/bewae/full-config.json", "filenameType": "str", "appendNewline": True, "createDir": False, "overwriteFile": "true", "encoding": "utf8", "x": 790, "y": 620, "wires": [["rw_clr_resp", "rw_clr_dbg"]]},
    {"id": "rw_clr_resp", "type": "http response", "z": TAB, "name": "200 OK", "statusCode": "200", "headers": {}, "x": 960, "y": 600, "wires": []},
    {"id": "rw_clr_dbg", "type": "debug", "z": TAB, "name": "debug CLR", "active": False, "tosidebar": True, "console": False, "tostatus": False, "complete": "true", "statusVal": "", "statusType": "auto", "x": 960, "y": 640, "wires": []},

    # --- Get TS API (ESP32 lightweight skip-download check) ---
    {"id": "rw_c7", "type": "comment", "z": TAB, "name": "Get TS API (ESP32 lightweight skip-download check)", "info": "", "x": 310, "y": 680, "wires": []},
    {"id": "rw_get_ts", "type": "http in", "z": TAB, "name": "[get] device ts", "url": "/bewae/get-ts", "method": "get", "upload": False, "swaggerDoc": "", "x": 200, "y": 720, "wires": [["rw_read_ts"]]},
    {"id": "rw_read_ts", "type": "file in", "z": TAB, "name": "read config", "filename": "/data/bewae/full-config.json", "filenameType": "str", "format": "utf8", "chunk": False, "sendError": True, "encoding": "none", "allProps": False, "x": 440, "y": 720, "wires": [["rw_extract_ts"]]},
    {"id": "rw_extract_ts", "type": "function", "z": TAB, "name": "extract ts", "func": get_ts_func, "outputs": 1, "timeout": "", "noerr": 0, "initialize": "", "finalize": "", "libs": [], "x": 640, "y": 720, "wires": [["rw_resp_ts"]]},
    {"id": "rw_resp_ts", "type": "http response", "z": TAB, "name": "return ts", "statusCode": "", "headers": {}, "x": 840, "y": 720, "wires": []},

    # --- Error handling ---
    {"id": "rw_c4", "type": "comment", "z": TAB, "name": "Error Handling", "info": "Catches file read/write errors and returns 500.", "x": 210, "y": 800, "wires": []},
    {"id": "rw_catch", "type": "catch", "z": TAB, "name": "catch file errors", "scope": ["rw_read1", "rw_merge", "rw_write", "rw_read2", "rw_filt", "rw_wm_merge", "rw_wm_write", "rw_clr_fn", "rw_clr_write", "rw_read_ts", "rw_extract_ts"], "uncaught": False, "x": 230, "y": 840, "wires": [["rw_fmterr"]]},
    {"id": "rw_fmterr", "type": "function", "z": TAB, "name": "format error", "func": error_func, "outputs": 1, "timeout": "", "noerr": 0, "initialize": "", "finalize": "", "libs": [], "x": 470, "y": 840, "wires": [["rw_resp_err"]]},
    {"id": "rw_resp_err", "type": "http response", "z": TAB, "name": "error 500", "statusCode": "500", "headers": {}, "x": 670, "y": 840, "wires": []},
]

with open(args.output, 'w', encoding='utf-8') as f:
    json.dump(flow, f, indent=4, ensure_ascii=False)

print(f"Done. {args.output}: {len(json.dumps(flow)):,} bytes, {len(flow)} nodes")
