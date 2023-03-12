////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// br-mat (c) 2022
// email: matthiasbraun@gmx.at
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

    // MQTT
    #ifndef mqtt_server
    #define mqtt_server "************"  // IP of the MQTT broker
    #endif

    #ifndef mqtt_username
    #define mqtt_username "************" // MQTT username
    #endif

    #ifndef mqtt_password
    #define mqtt_password "************" // MQTT password
    #endif

    // HTTP
    #ifndef SERVER
    #define SERVER "************" // IP of http server
    #endif

    #ifndef SERVER_PATH
    #define SERVER_PATH "/get-conf" // server path
    #endif

    #ifndef SERVER_PORT
    #define SERVER_PORT 1234
    #endif

#endif