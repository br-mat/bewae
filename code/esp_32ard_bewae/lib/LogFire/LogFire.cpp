#include "LogFire.h"

LogFireClass LogFire;

static const char* LEVEL_NAMES[] = { "", "INFO", "WARN", "ERROR", "CRITICAL" };

void LogFireClass::begin(const char* deviceName, const char* host, uint16_t port) {
    _deviceName = deviceName;
    _host = host;
    _port = port;
}

void LogFireClass::mirrorSerial(bool enable) {
    _mirrorSerial = enable;
}

void LogFireClass::localOnly(bool enable) {
    _localOnly = enable;
    if (enable) _disconnect();
}

String LogFireClass::_buildUrl() {
    return "http://" + _host + ":" + String(_port) + "/log";
}

void LogFireClass::_ensureConnected() {
    if (!_connected) {
        String url = _buildUrl();
        #ifdef ESP32
          _http.begin(url);
        #else
          _http.begin(_wifiClient, url);
        #endif
        _http.setTimeout(1000);
        _http.addHeader("Content-Type", "text/plain");
        _http.setReuse(true);
        _connected = true;
    }
}

void LogFireClass::_disconnect() {
    if (_connected) {
        _http.end();
        _connected = false;
    }
}

void LogFireClass::log(const char* message, uint8_t level) {
    if (_mirrorSerial) {
        if (level > 0 && level <= 4) {
            Serial.print("[");
            Serial.print(LEVEL_NAMES[level]);
            Serial.print("] ");
        }
        Serial.println(message);
    }

    if (_localOnly || WiFi.status() != WL_CONNECTED) {
        _disconnect();
        return;
    }

    _ensureConnected();

    String body;
    if (level > 0) {
        body = _deviceName + "(-" + String(level) + "): " + message;
    } else {
        body = _deviceName + ": " + message;
    }

    int code = _http.POST(body);
    if (code < 0) {
        _disconnect();
    }
}

void LogFireClass::log(const String& message, uint8_t level) {
    log(message.c_str(), level);
}
