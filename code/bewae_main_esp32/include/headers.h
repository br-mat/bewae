//Standard
//#include <ArduinoSTL.h>
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

//external
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFi.h>
//#include "PubSubClient.h" // wont work with platformio use esp idf

//own
#include <lib/config.h>
#include <lib/Helper.h>

using namespace std;