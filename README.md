# Bewae - Bewässerungsprojekt v3.3

v3.3final versions are not stable (work in progress).

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
- [Bilder & Entstehung](#bilder--Entstehung)

## Introduction (EN)
This version is more of a test version. It has been reworked to fit an ESP32 board. <br>
This is **not** a step-by-step guide - a little basic knowledge in dealing with Linux and microcontrollers is required. It should only be given an insight into the project and make it easier for me to rebuild in spring or at new locations. That's why I decided to use German, but I would like to add an English version later. <br>

## Einleitung (DE)
Diese Version ist eher als Testversion zu sehen. <br>
Bei der bisherigen Aufarbeitung handelt es sich **nicht** um eine Step-by-step Anleitung es ist ein wenig Grundwissen im Umgang mit Linux und Mikrokontrollern vorausgesetzt. Es soll lediglich ein Einblick in das Projekt gegeben werden und mir den Wiederaufbau im Frühjahr oder an neuen Standorten erleichtern. Deshalb habe ich mich auch für Deutsch entschieden, möchte jedoch noch eine englische Version ergänzen. <br>

### Beschreibung
Übersicht des Schaltungsaufbaus im beigefügten [*Systemdiagramm.png*](#systemdiagramm) als Blockschaltbild. Zur Steuerung wird ein ESP-32 verwendet. Sensoren und Module zeichnen jede Menge Daten auf, dazu zählen Bodenfeuchtigkeit-, Temperatur-, Luftfeuchte- und Luftdruckdaten sowie Sonnenscheindauer. Werden mit einem InfluxDB Client an RaspberryPi gesendet und in InfluxDB 2.0. <br> 
Die Bewässrung folgt einem Zeitplan. Der entweder mit einprogrammierten Werten arbeitet oder über das Netzwerk gesteuert werden kann. Mit einem eingerichtetem *VPN* (z.B PiVPN) kann man auch von unterwegs seine Pflanzen im Auge behalten und bewässern. <br>

<br>

**Versorgung:** Das System ist ausgestattet mit einem kleinen PV Modul einem Blei Akku und einem kleinen *100l* Wassertank ausgestattet. Je nach Temperatur muss nun nur noch einmal pro Woche daran gedacht werden den Tank zu füllen.
<br>

## Systemdiagramm

### V3.3 Abbildung:

 System Setup:                  | Solar Setup:
:-------------------------:|:-------------------------:
![System](/docs/pictures/SystemdiagrammV3_3.png "Systemdiagramm 3.3") | ![Solar](/docs/pictures/systemdiagramSolar.png "Solar diagramm 3.3")
(zeigt grobe Skizzierung des Systems)

### aktueller Aufbau
- ESP32
- RaspberryPi 4 B (*4GB*)
- *6* Ventile *12V*
- *2* Pumpen (aktuell nur eine *12V* im Betrieb)
- *16* Analoge/Digitale Pins für Sensoren & andere Messungen (Bodenfeuchte, Photoresistor, etc.)
- BME280 Temperatur/Luftfeuchtigkeit/Druck Sensor
- RTC DS3231 Real Time Clock
- Bewässerungs Kit: Schläuche, Sprinkler, etc.
- *10W 12V* Solar Panel
- *12 Ah* *12V* Bleiakku
- Solarladeregler

## Steuerung der Bewässerung

Gesteuert wird mit den Einstellungen im config.JSON file.
Dabei werden 2 Variablen verwendet - timetable und water-time:

- timetable: Repreäsentirt durch die ersten 24 bit einer int Zahl wobei jedes bit für eine Stunde des tages steht, um platz zu sparen als long int abgespeichert
- watertime: Ist die Zeit (in Sekunden) die die jeweilige Gruppe bewässert wird. Hierbei können mehrere Ventile oder Ventil + Pumpe gleichzeitig gesetzt werden.

<br>

```
// example timetable                           23211917151311 9 7 5 3 1
//                                              | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                               | | | | | | | | | | | |
// decimal representation: 1049600              22201816141210 8 6 4 2 0
// 
```

Die Steuerung selbst kann auf folgenden Arten erfolgen:
- MQTT über W-LAN (Telefon oder Pi) (noch nicht wieder implementiert)
- Config-Datei im Flash (SPIFFS) wenn kein Netzwerk gefunden wurde

### config.JSON:

In dieser Datei befinden sich alle relevanten Einstellungen. Diese wird Dank Node-Red vom Raspberry Pi innerhalb des Netzwerks zur Verfügung gestellt und kann vom Mikrocontroller abgefragt werden. Hierfür muss die Adresse in der connection.h angepasst werden. <br>
Somit werden kann über Änderungen der beiden genannten variablen in die Bewässerung eingegriffen werden.

```
// JSON group template
{"group": {
    "Tomatoes": {         // name of grp
    "is_set":1,           // indicates status
    "vpins":[0,5],        // list of pins bound to group
    "lastup":[0,0],       // runtime variable - dont touch!
    "watering":0,         // runtime variable - dont touch!
    "water-time":10,      // water time in seconds
    "timetable":1049600   // timetable (first 24 bit)
    },
    .
    .
    .
  }
}
```

### Node-Red flow:

Der verwendete Node-Red flow befindet sich im Ordner node-red-flows. Gegebenenfalls Adresse bzw. Pfad zur Datei ändern.

## Konfiguration der Sensoren:

TODO: fill out section

<br>

# Details

## Platinen
Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen. Ab einer gewissen Größe des Projekts ist fritzing allerdings nicht mehr ideal. Gerber files sind vorhanden. Für die Fritzing files ist lediglich die Platinen Ansicht relevant. Alle Boards setzten auf den [*ESP-32*](https://de.wikipedia.org/wiki/ESP32) als Mikrocontroller. <br>

 Board 1:                  | Board 3:                  | Board 5:
:-------------------------:|:-------------------------:|:-------------------------:
![Board1](/docs/pictures/bewae3_3_board1v3_838_Leiterplatte.png) | ![Board3](/docs/pictures/bewae3_3_board3v22_Leiterplatte.png) | ![Board5](/docs/pictures/bewae3_3_board5v5_final_Leiterplatte.png)

### **bewae3_3_board1v3_838.fzz** Hauptplatine (PCB)
Große Hauptplatine. Platz für bis zu 16 analoge Sensoren sowie bme280 und rtc-Modul über i2c. 8 Pins für Ventile/Pumpen (Erweiterbar) diese können über Relais oder dem PCB Board 3 einfach verwendet werden.
<br>


### **bewae3_3_board3v22.fzz** als Erweiterung (PCB)
Als Erweiterung gedacht. Hat den Zweck Steckplätze für Sensoren und weitere Verbraucher zu liefern. Zur Erweiterung der 8 Pins der Hauptplatine kann das Schieberegister verwendet werden. Insgesamt 13 Steckplätze für Sensoren inklusive Versorgung sowie 2 Pumpen (*12V*) und 10 kleinere Ventile (*12V*).
<br> 

### **bewae3_3_board5v5_final.fzz** als Hauptplatine (PCB)
Kleinere Hauptplatine. Verzichtet auf große Anzahl an Sensor Anschlüsse. Platz für 2x 5V Sensoren, 2x 3V Sensoren, 1x LDR, i2c BUS, 1wireBus. Bme280 und rtc-Modul sind ebenfalls vorgesehen.
<br> 


## Code ESP32
[Arduino-Nano Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>

### How to deploy the code:
First, make sure you have PlatformIO installed in Visual Studio Code. Then, open the project folder in VS Code. Next, plug in your ESP-32 Controller and click on the arrow icon in the PlatformIO taskbar to upload your code. The device should be detected automatically. <br>

Before executing the program on the controller, it will check if some devices are responding, so be sure to connect them properly. <br>

Don’t forget to update your connection settings. Simply change the ‘connection.h’ file within the src folder to match your (network) configuration. <br>

### Change time of RTC:

Make sure to set the time of the RTC either setup a code example for this device or uncomment my function in the setup. If my approach is used don’t forget to comment it again otherwise time will be reset every reboot. <br>

## Raspberrypi
### How to:
Persönlich verwende ich gerade einen [**RaspberryPi 4B**](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) mit *4GB* Ram auf dem ein [*VPN*](https://www.pivpn.io/) (**wireguard**) installiert ist. Durch den *VPN* kann ich auch von unterwegs auf die Daten zugreifen oder gegebenenfalls in die Bewässerung eingreifen. Auf die Installation gehe ich nicht weiter ein, sie ist aber relativ einfach und in zahlreichen Tutorials gut beschrieben. <br>
Für den Betrieb genügt auch die Verfügbarkeit im lokalen Netzwerk, hierbei notwendig sind:
- SSid (Netzwerkname): BeispielNetzwerkXY
- Passwort:            BeispielpasswortXY
- IP oder hostname des RaspberryPi (IP statisch vergeben)
Für die erleichterte Konfiguration entweder Bildschirm, ssh, teamviewer, vnc viewer ... am Raspberrypi auch auf diese Punkte gehe ich nicht weiter ein, da sie anderswo gut zu finden sind. <br>

### Node-Red

TODO: update documentation

### InfluxDB 2.0

TODO: update documentation

### Grafana
![grafana dashboard example](/docs/pictures/grafanarainyday.png "Grafana rainy day") <br>

Auch hier gibt es sehr gute [Tutorials](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) auf die man zurückgreifen kann, daher gehe ich nicht genauer auf den Installationsprozess ein. Grafana kann genutzt werden um die gesammelten Daten in schönen plots darzustellen. Somit ist eine Überwachung der Feuchtigkeitswerte der Pflanzen sehr leicht möglich. <br>

Sobald Grafana installiert ist kann das [json](/grafana_dashboard/bewaeMonitor.json) export über das Webinterface importiert werden. <br>

Unter 'Configuration' muss man nun die 'Datasources' eintragen. Hierbei muss man nun darauf achten die gleichen Datenbanken zu verwenden die man erstellt und in der die Daten abgespeichert werden. In meinem Fall wäre das z.B ***main*** für alle Daten der Bewässerung. Sowie ***pidb*** für die CPU Temperatur am RaspberryPi die im Import mit dabei ist, da dies rein optional und nichts mit der Bewässerung zu tun hat werde ich nicht weiter darauf eingehen. Die Panels wenn sie nicht genutzt werden kann man einfach entfernen. <br>

![Datasource configuration](/docs/pictures/datasources.png "Datasource configuration example") <br>

### MQTT (OUTDATED!)
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
```

#### Installation
Auch hier gibt es einen [link](https://pimylifeup.com/raspberry-pi-mosquitto-mqtt-server/).
Benutzername und Passwort müssen im Code wieder an allen Stellen angepasst werden.

#### mqttdash app (OUTDATED!)

Auf mein **Android** Smartphone habe ich die App [mqttdash](https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=de_AT&gl=US) geladen. Diese ist sehr einfach und intuitiv zu verwenden man muss Adresse Nutzer und Passwort die oben angelegt wurden eintragen und kann dann die Topics konfigurieren. 

Ein Beispiel Screenshot aus der App, die Zahlen stehen für die Zeit (s) in der das jeweilige Ventil geöffnet ist und Wasser gepumpt wird (ca. 0.7l/min):
![mqttdash app](/docs/pictures/mqttdash.jpg) <br>

## Bilder & Entstehung

**Entstehung:** <br>
Das Projekt selbst entstand aus einer mehrwöchigen Abwesenheit in der die Balkonpflanzen ohne Versorgung gewesen wären. Das grün sollte mit einem Bewässerungsset, Schläuche und ein paar Düsen und einer Pumpe am Leben gehalten werden. Eine Reguläre Bewässerung wie bei einer Zeitschaltuhr kam mir jedoch zu langweilig vor und mein Interesse an einem kleinen Bastelprojekt war geweckt. Kapazitive Bodenfeuchte Sensoren und 4 kleine *12V* Ventile waren schnell bestellt, Arduinos hatte ich genug zu Hause, so kam es innerhalb einer Woche zu [Version 1](#v1) und das Überleben der Pflanzen war gesichert. Es entstand aus der Not ein Projekt das mich eine Weile Beschäftigt hat, kontinuierlich erweitert und verbessert erfüllt es nach momentanem Stand weit mehr als zuerst geplant. <br>
<br>

Zum Abschluss eine kleine Sammlung von Fotos über mehrere Versionen des Projekts, die über die Zeit entstanden sind:

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
