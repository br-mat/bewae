////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2023
// see gitHub for author info
//
// Connection related definitions
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef __CONNECTION_H__
#define __CONNECTION_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Wifi Constants
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // WiFi
    #ifndef ssid
    #define ssid "************"         // Your personal network SSID
    #endif

    #ifndef wifi_password
    #define wifi_password "************" // Your personal network password
    #endif

  // Server (RaspberryPi)
    #ifndef SERVER
    #define SERVER "************" // IP of http server
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
    // Define the URL and database name of the InfluxDB server
    // TODO prettyfie influx url!
    #ifndef INFLUXDB_URL
    #define INFLUXDB_URL "http://************:8086"
    #endif

    #ifndef INFLUXDB_DB_NAME
    #define INFLUXDB_DB_NAME "example-home"
    #endif

    #ifndef INFLUXDB_TOKEN
    #define INFLUXDB_TOKEN "************"
    #endif

    #ifndef INFLUXDB_ORG
    #define INFLUXDB_ORG "example-org"
    #endif

  // BEWAE DEVICE
    #ifndef DEVICE_NAME
    #define DEVICE_NAME "Default" // name must match WebConfig name
    #endif

#endif