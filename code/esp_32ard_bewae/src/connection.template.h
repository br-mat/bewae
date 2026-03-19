////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// see gitHub for author info
//
// Connection related definitions — TEMPLATE FILE
// Copy this to connection.h and fill in your real values.
// connection.h is gitignored and excluded from Claude.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CONNECTION_H__
#define __CONNECTION_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wifi Constants
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // WiFi
    #ifndef ssid
    #define ssid "YOUR_WIFI_SSID"         // Your personal network SSID
    #endif

    #ifndef wifi_password
    #define wifi_password "YOUR_WIFI_PASSWORD" // Your personal network password
    #endif

  // Server (RaspberryPi)
    #ifndef SERVER
    #define SERVER "192.168.x.x" // IP of http server
    #endif

    #ifndef SERVER_PATH
    #define SERVER_PATH "/get-conf" // server path
    #endif

    #ifndef NODERED_PORT
    #define NODERED_PORT 1880
    #endif

  // NTP
    #ifndef NTP_Server
    #define NTP_Server "at.pool.ntp.org"
    #endif

  // INFLUXDB
    #ifndef INFLUXDB_URL
    #define INFLUXDB_URL "http://192.168.x.x:8086"
    #endif

    #ifndef INFLUXDB_DB_NAME
    #define INFLUXDB_DB_NAME "your-db-name"
    #endif

    #ifndef INFLUXDB_TOKEN
    #define INFLUXDB_TOKEN "your-influxdb-token"
    #endif

    #ifndef INFLUXDB_ORG
    #define INFLUXDB_ORG "your-org"
    #endif

  // BEWAE DEVICE
    #ifndef DEVICE_NAME
    #define DEVICE_NAME "Default" // name must match WebConfig name
    #endif

#endif
