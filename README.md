# Bewae - Bewässerungsprojekt v3.3

v3.3 WIP versions now are not stable

- added openweather scripts (saving current conditions & air condition to influxDB)
- added corresponding .sh launcher files
- added ESP32 build
- added 2nd circuit handling pumps & solenoids

todo:
- add openwather forecast script
- add dynamic watering controlled by Pi (using ML later)
- updating main circuit, correct bugs & problems on ESP32 build
- update english translation of new version
- update documentation

open problems:
- sensor rail needs 5V not 3V! (hooked onto logic switch for now)
- backup SD card module malfunctions
- reverse voltage protection on 12V IN get really hot (bridged for now)

### About

Automatisiert Sensorgesteuerte Bewässerung mit Raspberry Pi & Arduino <br>
**Ziele:**
+ Sensorgesteuerte Automatisierte Bewässerung der Balkonpflanzen
+ Speichern & Darstellung von Sensordaten
+ Überwachung & Steuerung von unterwegs
+ (Zukünftig) Abfrage von Wetterdaten & Bewässerung mit Machine Learning

## Inhalt:
- [Introduction (EN)](#introduction-(de))
- [Einleitung (DE)](#einleitung-(de))
  * [Aktueller Aufbau](#aktuelleraufbau)
  * [Beschreibung](#beschreibung)
  * [Systemdiagramm](#systemdiagramm)
- [Steuerung](#steuerung-der-bewässerung)
- [Details](#details)
  * [Platinen](#platinen)
  * [Code Arduino Nano](#code-arduino-nano)
  * [Code ESP8266-01](#code-esp8266-01)
  * [RaspberryPi](#raspberrypi)
- [Bilder](#bilder)

## Introduction (EN)
This version is more of a test version. It has been reworked to fit a ESP32 board. <br>
This is **not** a step-by-step guide - a little basic knowledge in dealing with Linux and microcontrollers is required. It should only be given an insight into the project and make it easier for me to rebuild in spring or at new locations. That's why I decided to use German, but I would like to add an English version later. <br>

## Einleitung (DE)
Diese Version ist eher als Testversion zu sehen. <br>
Bei der bisherigen Aufarbeitung handelt es sich **nicht** um eine Step-by-step Anleitung es ist ein wenig Grundwissen im Umgang mit Linux und Mikrokontrollern vorausgesetzt. Es soll lediglich ein Einblick in das Projekt gegeben werden und mir den Wiederaufbau im Frühjahr oder an neuen Standorten erleichtern. Deshalb habe ich mich auch für Deutsch entschieden, möchte jedoch noch eine Englische Version ergänzen. <br>

### aktueller Aufbau
- ESP32
- RaspberryPi 4 B (*4GB*)
- *6* Ventile *12V*
- *2* Pumpen (aktuell nur eine *12V* im Betrieb)
- *16* Analoge/Digitale Pins für Sensoren & andere Messungen (Bodenfeuchte, Photoresistor, etc.)
- BME280 Temperatur/Luftfeuchtigkeit/Druck Sensor
- RTC DS3231 Real Time Clock
- micro SD module & micro SD Karte
- Bewässerung Kit Schläuche, Sprinkler, etc.
- *10W 12V* Solar Panel
- *12 Ah* *12V* Bleiakku
- Solarladeregler

### Beschreibung
Übersicht des Schaltungsaufbaus im beigefügten [*Systemdiagramm.png*](#systemdiagramm) als Blockschaltbild. Zur Steuerung wird ein [*ESP-32*](https://de.wikipedia.org/wiki/ESP32) verwendet. Sensoren und Module zeichnen jede Menge Daten auf, dazu zählen Bodenfeuchtigkeits Sensoren, Temperatur-, Luftfeuchte- und Luftdruckdaten sowie Sonnenscheindauer. Die Daten werden via [*MQTT*](#mqtt) an den [RaspberryPi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) gesendet, dort gespeichert und dank Grafana als schöne Diagramme dargestellt. <br> 
 Die bewässerung folgt einem Zeitplan. Der entweder mit einprogrammierten Werten arbeitet oder über das Netzwerk gesteuert werden kann. Mit einem [*MQTT*](#mqtt) messaging client ist möglich in die Bewässerung einzugreifen. Mit einem eingerichtetem [*VPN*](https://www.pivpn.io/) kann man auch von unterwegs seine Pflanzen im Auge behalten und bewässern.
In den weiteren Ordnern befinden sich der Code für beide Controller sowie das json export für das Grafana Dashboard und die Python scripts zur Verarbeitung der [*MQTT*](#mqtt) messages. Die verwendete Datenbank ist [*InfluxDB*](#influxdb) in der alle gesendeten Daten gespeichert werden. Alle relevanten Programme und Scripte starten automatisiert dank crontab bei jedem bootvorgang. <br>

<br>

**Entstehung:** <br>
Das Projekt selbst entstand aus einer mehrwöchigen Abwesenheit in der die Balkonpflanzen ohne Versorgung gewesen wären. Das grün sollte mit einem Bewässerungsset, Schläuche und ein paar Düsen und einer Pumpe am Leben gehalten werden. Eine Reguläre Bewässerung wie bei einer Zeitschaltuhr kam mir jedoch zu langweilig vor und mein Interesse an einem kleinen Bastelprojekt war geweckt. Kapazitive Bodenfeuchte Sensoren und 4 kleine *12V* Ventile waren schnell bestellt, Arduinos hatte ich genug zu Hause, so kam es innerhalb einer Woche zu [Version 1](#v1) und das Überleben der Pflanzen war gesichert. Es entstand aus der Not ein Projekt das mich eine Weile Beschäftigt hat, kontinuierlich erweitert und verbessert erfüllt es nach momentanem Stand weit mehr als zuerst geplant. <br>
<br>

**Stand jetzt:** <br>

**Technik:** Für die einfachere Handhabung wurde mit [fritzing](https://fritzing.org/) ein Plan für ein [PCB](#platinen) erstellt und gedruckt. Sensordaten von diversen Sensoren (Bodenfeuchte, Temperatur etc.) werden an einen [RaspberryPi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) gesendet und dort in eine Datenbank gespeichert. Die Steuerung der Bewässerung funktioniert entweder automatisiert Sensorgesteuert oder über eine [MQTT-messaging App](#mqttdash-app-optional) über das Smartphone. Dank [VPN](https://www.pivpn.io/) sind auch von unterwegs Steuerung und Beobachtung möglich. <br>

**Versorgung:** Mittlerweile wurde daraus ein autonom funktionierendes System, ausgestattet mit einem kleinen PV Modul einem Blei Akku und einem kleinen *100l* Wassertank. Je nach Temperatur muss nun nur noch einmal pro Woche daran gedacht werden den Tank zu füllen.
<br>

## Systemdiagramm
### V3.0:
![System](/docs/pictures/Systemdiagramm.png "Systemdiagramm")
### V3.3 preview:
![System](/docs/pictures/Systemdiagramm3_3.png "Systemdiagramm 3.3")


## Steuerung der Bewässerung

- (Neu) getestet wird eine Steuerung über eine command funktion

Grundsätzlich erlaubt es der **Zeitplan** das einmal pro stunde bewässert wird. Die **Wassermenge** in den einzelnen Bewässerungsgruppen wird über die **Zeit** gesteuert die Pumpe und das jeweilige Ventil arbeiten. Zu jeder eingetragenen Stunde im Zeitplan wird einmal die eingestellte Menge entlassen. <br>

### Steuerung mit Programmierung (Default):

**ZEITPLAN:** bestehend aus einer *long* Zahl wobei jedes bit von rechts beginnend bei *0* für eine Stunde auf der Uhr steht. Im Beispiel wird somit um *10* und *20* Uhr bewässert. <br>
```
//                                           2523211917151311 9 7 5 3 1
//                                            | | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                             | | | | | | | | | | | | |
//                                            2422201816141210 8 6 4 2 0
```
<br>

**MENGE:**
Diese eintragung ist im code vorzunehmen siehe code Beispiel unten.
Hierbei muss die Gruppe auf *true* gesetzt werden und der richtige pin und pumpe eingetragen werden. Nach der Bezeichnung (nicht notwendig) muss noch die Zeit in Sekunden eingetragen werden. Die restlichen Werte müssen auf *0* gesetzt werden da diese zur Laufzeit relevant werden.
```
//is_set, pin, pump_pin, name, watering default, watering base, watering time, last act,
solenoid group[max_groups] =
{ 
  {true, 0, pump1, "Tom1", 100, 0, 0 , 0}, //group1
  {true, 1, pump1, "Tom2", 80, 0, 0, 0}, //group2
  {true, 2, pump1, "Gewa", 30, 0, 0, 0}, //group3
  {true, 3, pump1, "Chil", 40, 0, 0, 0}, //group4
  {true, 6, pump1, "Krtr", 20, 0, 0, 0}, //group5
  {true, 7, pump1, "Erdb", 50, 0, 0, 0}, //group6
};
```
<br>

### Steuerung über WLAN (APP, MQTT):
Im *config.h* file muss *define RasPi 1* gesetzt sein und MQTT sowie WLAN einstellungen müssen richtig konfiguriert sein. Um mit dem System zu kommunizieren wird eine App aus dem Play-store [MQTT Dash](https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=en&gl=US) genutzt.

**ZEITPLAN:**
Über MQTT muss mit dem topic *home/nano/timetable* der Wert der *long* *int* Zahl als dezimal gesendet werden. Da die Binäre representation als string unnötig lange wäre.
<br>

Diese Methode ist nicht benutzerfreundlich deshalb habe ich eine weiter Möglichkeit implementiert. Unter dem Topic: *home/nano/planed* können die geplanten vollen stunden als liste (Beispiel: *6,10,20*) gesendet werden. <br>

**MENGE:**
Es muss lediglich die anzahl an sekunden gesendet werden die die jeweilige Gruppe bewässert werden soll. Die Topics müssen dem unten angeführten schema entsprechen und in der App eingestellt werden. <br>
Topics:
```
home/grp1/water_time
home/grp2/water_time
.
.
home/grp6/water_time
```
<br>

![config](/docs/pictures/mqtt-app)
![config](/docs/pictures/watering-config)

### Steuerung über WLAN (commands):
platzhalter

# Details

## Platinen
Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen. Ab einer gewissen größe des Projekts ist fritzing allerdings nicht mehr ideal. <br>

### **bewae3_3_board1v3_7.fzz** Hauptplatine (PCB)
Hat den Zweck das System zu Steuern und alle Daten zu Sammeln. Es werden alle relevanten Sensoren und Module sowie Steuerungen auf diese Platine zusammengeführt und von einem [*ESP-32*](https://de.wikipedia.org/wiki/ESP32) verarbeitet.
![Main PCB](/docs/pictures/bewae_v3_1.png "Main board")

<br>

### **bewae3_3_board2v6.fzz** als Erweiterung (PCB)
Sie ist als Erweiterung gedacht. Die Platine bietet 13 Steckplätze für sensoren sowie 2 Pumpen (*12V*) und 6 Ventile (*12V*). Die Spannung der Bleibatterie (Akku) soll über den Spannungsteiler abgegriffen werden können. 
<br>


## Code ESP32
[Arduino-Nano Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>

### Allgemein
Der Code wird mit Visual Studio Code und der Platformio extension aufgespielt, VS code bietet eine umfangreiche IDE und eignet sich für größere Projekte ausgezeichnet. <br>

Damit das Programm ausgeführt werden kann muss das RTC Module richtig angeschlossen sein und funktionieren. Die SD Karte, den BME280 sowie Netzwerksteuerung können über dei config deaktiviert werden. Dazu werden einfach die dafür vorgesehenen definitionen im *config.h* file auskommentiert. <br>

Damit die Steuerung und Kommunikation über das Netzwerk funktioniert müssen die WLAN Einstellungen richtig konfiguriert sein. Hier sind lediglich die Daten des jeweiligen Netzwerks auszufüllen. <br>

Für Debugging zwecke empfielt es sich RX und TX pins am Mikrocontroller abzugreifen und die *DEBUG* definition zu setzten. <br>

## Raspberrypi
### Allgemein
Persönlich verwende ich gerade einen [**RaspberryPi 4B**](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) mit *4GB* Ram auf dem auch ein [*VPN*](https://www.pivpn.io/) (**wireguard**) und [*Pihole*](https://pi-hole.net/) installiert sind. Durch den [*VPN*](https://www.pivpn.io/) kann ich auch von unterwegs auf die Daten zugreifen oder gegebenenfalls in die Bewässerung eingreifen. Auf die Installation gehe ich nicht weiter ein, sie ist aber relativ einfach und in zahlreichen Tutorials gut beschrieben. <br>Für den Betrieb genügt auch die Verfügbarkeit im lokalen Netzwerk, hierbei notwendig sind:
- SSid (Netzwerkname): BeispielNetzwerkXY
- Passwort:            BeispielpasswortXY
- IP oder hostname des RaspberryPi (IP statisch vergeben)
Für die erleichterte Konfiguration entweder Bildschirm, ssh, teamviewer, vnc viewer ... am Raspberrypi auch auf diese Punkte gehe ich nicht weiter ein, da sie anderswo gut zu finden sind. <br>

### InfluxDB
Falls influx noch nicht installiert wurde, gibt es nach kurzer Suche jede Menge brauchbare Anleitungen z.B: [(pimylifeup)](https://pimylifeup.com/raspberry-pi-influxdb/).
<br>

Für Einrichtung der Datenbank beginnen wir erstmal mit dem Erstellen eines Users. Dafür wird InfluxDB gestartet indem man in das Terminal folgende Zeile eingibt.
```
influx
```
Influx sollte nun gestartet haben, nun weiter in das Interface eingeben:
```
CREATE USER "username" WITH PASSWORD "password"
CREATE DATABASE "database"
GRANT ALL ON "username" TO "database"
```
Diesen ***username*** sowie ***password*** und die ***database*** müssen im Code der Mikrocontroller ausgefüllt werden.
<br>
Um aus diesem "interface" wieder herauszukommen muss man einfach den Befehl **exit** eingeben und man befindet sich wieder im Terminal.
```
exit
```

### Grafana
![grafana dashboard example](/docs/pictures/grafanarainyday.png "Grafana rainy day") <br>

Auch hier gibt es sehr gute [Tutorials](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) auf die man zurückgreifen kann, daher gehe ich nicht genauer auf den Installationsprozess ein. Grafana kann genutzt werden um die gesammelten Daten in schönen plots darzustellen. Somit ist eine Überwachung der Feuchtigkeitswerte der Pflanzen sehr leicht möglich. <br>

Sobald Grafana installiert ist kann das [json](/grafana_dashboard/bewaeMonitor.json) export über das Webinterface importiert werden. <br>

Unter 'Configuration' muss man nun die 'Datasources' eintragen. Hierbei muss man nun darauf achten die gleichen Datenbanken zu verwenden die man erstellt und in der die Daten abgespeichert werden. In meinem Fall wäre das z.B ***main*** für alle Daten der Bewässerung. Sowie ***pidb*** für die CPU Temperatur am RaspberryPi die im Import mit dabei ist, da dies rein optional und nichts mit der Bewässerung zu tun hat werde ich nicht weiter darauf eingehen. Die Panels wenn sie nicht genutzt werden kann man einfach entfernen. <br>

![Datasource configuration](/docs/pictures/datasources.png "Datasource configuration example") <br>

### MQTT&Python

#### MQTT
**MQTT** (Message Queuing Telemetry Transport) Ist ein offenes Netzwerkprotokoll für Machine-to-Machine-Kommunikation (M2M), das die Übertragung von Telemetriedaten in Form von Nachrichten zwischen Geräten ermöglicht. Wie es funktioniert [(link)](http://www.steves-internet-guide.com/mqtt-works/). <br>
Die versendeten Messages setzen sich aus topic und payload zusammen. Topics dienen der einfachen Zuordnung und haben in meinem Fall eine fixe Struktur:
```
#topic:
exampletopic=home/location/measurement
```
Zusätzlich zu diesem topic werden im Payload die Daten angehängt, der Inhalt als String. Auflistung relevanter topics:
```
#example topics to publish data on
  humidity_topic_sens2 = "home/sens2/humidity";
  #bme280 sensor read

  temperature_topic_sens2 = "home/sens2/temperature";
  #bme280 sensor read

  pressure_topic_sens2 = "home/sens2/pressure";
  #bme280 sensor read

  config_status = "home/bewae/config_status";
  #signaling RaspberryPi to repost relevant data so ESP can receive it

#subsciption topics to receive data
  watering_topic = "home/bewae/config";
  #bewae config recive watering instructions (as csv)

  bewae_sw = "home/nano/bewae_sw";
  #switching watering no/off

  watering_sw = "home/nano/watering_sw";
  #switching value override on/off (off - using default values on nano)

```

#### Installation
Auch hier gibt es einen [link](https://pimylifeup.com/raspberry-pi-mosquitto-mqtt-server/).
Benutzername und Passwort müssen im Code wieder an allen Stellen angepasst werden.

#### Python
Um an die über [*MQTT*](#mqtt) gesendeten Daten nun in die Datenbank schreiben oder lesen zu können wird das Python script [MQTTInfluxDBBridge3.py](/code/pi_scripts/MQTTInfluxDBBridge3.py) verwendet. Das script selbst stammt aus einem [Tutorial](https://diyi0t.com/visualize-mqtt-data-with-influxdb-and-grafana/) und wurde adaptiert um es an die Anforderungen in meinem Projekt anzupassen. Der Python code kann mit dem shell script [launcher1.sh](/code/pi_scripts/launcher1.sh) automatisiert mit crontab bei jedem Bootvorgang mitgestartet werden. Da der Pi beim Hochfahren eine gewisse Zeit benötigt um alles fehlerfrei zu starten, verzögere ich den Start des scripts um *20* Sekunden. <br>
Um Fehler zu vermeiden sollten über [*MQTT*](#mqtt) nur **int** Werte verschickt werden (*2* **byte**), der Datentyp **int** ist am [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) *2* **byte** groß. <br>
```
sudo crontab -e
```
Öffnet cron mit einem beliebigen Editor um nun die gewünschten Programme einzutragen. Als Beispiel:
```
@reboot /path/file.sh
```
Zusätzliche [Informationen](https://pimylifeup.com/cron-jobs-and-crontab/) zu cron.

#### mqttdash app (optional)
Auf mein **Android** Smartphone habe ich die App [mqttdash](https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=de_AT&gl=US) geladen. Diese ist sehr einfach und intuitiv zu verwenden man muss Adresse Nutzer und Passwort die oben angelegt wurden eintragen und kann dann die Topics konfigurieren. Wichtig ist das nur **int** werte gesendet werden können. Bei mehr als *6* Gruppen muss man die Variable max_groups ebenfalls wieder an jeder Stelle in allen Programmen anpassen. Alle topics die über [*MQTT*](#mqtt) gesendet werden und als *'measurement'* *'water_time'* eingetragen haben werden an den [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) weitergeleitet und in die Datenbank eingetragen. Über den eintrag *'location'* können sie unterschieden werden deshalb empfiehlt es sich gleichen Namen und eine Nummerierung zu verwenden da die *'locations'* sortiert und in aufsteigender Reihenfolge vom [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) an den [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) gesendet werden.

Ein Beispiel Screenshot aus der App, die Zahlen stehen für die Zeit (s) in der das jeweilige Ventil geöffnet ist und Wasser gepumpt wird (ca. 0.7l/min):
![mqttdash app](/docs/pictures/mqttdash.jpg) <br>

## Bilder
Kleine Sammlung von Fotos über mehrere Versionen des Projekts, die über die Zeit entstanden sind.

### V3

![Bild](/docs/pictures/bewaeV3(Sommer).jpg) <br>
![Bild](/docs/pictures/bewaeV3(Herbst).jpg) <br>
![Bild](/docs/pictures/bewaeV3(Box).jpg) <br>
![Bild](/docs/pictures/MainPCB.jpg) <br>

### V2

![Bild](/docs/pictures/bewaeV2.jpg) <br>


### V1

![Bild](/docs/pictures/bewaeV1(2).jpg) <br>
![Bild](/docs/pictures/bewaeV1.jpg) <br>