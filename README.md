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
- [Systemdiagramm](#systemdiagramm)
- [Steuerung der Bewässerung](#steuerung-der-bewässerung)
- [Konfiguration der Sensoren](#konfiguration-der-sensoren)
- [Details](#details)
  * [Platinen](#platinen)
  * [Code ESP32](#code-esp32)
  * [RaspberryPi](#raspberrypi)
- [Bilder & Entstehung](#bilder--Entstehung)

## Einleitung (DE)
Diese Version ist eher als Testversion zu sehen. <br>
Bei der bisherigen Aufarbeitung handelt es sich nicht um eine Schritt-für-Schritt-Anleitung. Es wird ein gewisses Grundwissen im Umgang mit Linux und Mikrokontrollern vorausgesetzt. Ziel ist es, lediglich einen Einblick in das Projekt zu geben und den Wiederaufbau im Frühjahr oder an neuen Standorten zu erleichtern. <br>

### Beschreibung

Zur Steuerung wird ein ESP-32 verwendet. Verschiedene Sensoren und Module erfassen eine Vielzahl von Daten, darunter Bodenfeuchtigkeits-, Temperatur-, Luftfeuchtigkeits- und Luftdruckdaten sowie Sonnenscheindauer. Diese Daten werden mithilfe eines InfluxDB-Clients an einen Raspberry Pi gesendet und in InfluxDB 2.0 gespeichert. <br>

Die Bewässerung folgt einem Zeitplan, der entweder mit vorprogrammierten Werten arbeitet oder über das Netzwerk gesteuert werden kann. Durch Einrichten eines VPNs (z.B. PiVPN) kann man auch von unterwegs seine Pflanzen im Auge behalten und bewässern. <br>

**Versorgung:**
Das System ist mit einem kleinen PV-Modul, einem Blei-Akku und einem kleinen 100l Wassertank ausgestattet. Je nach Temperatur muss nun nur noch einmal pro Woche daran gedacht werden, den Tank zu füllen. <br>


## Systemdiagramm

### V3.3 Abbildung:

 System Setup:                  | Solar Setup:
:-------------------------:|:-------------------------:
![System](/docs/pictures/SystemdiagrammV3_3.png "Systemdiagramm 3.3") | ![Solar](/docs/pictures/systemdiagramSolar.png "Solar diagramm 3.3")
(zeigt grobe Skizzierung des Systems)

### aktueller Aufbau
- ESP32
- RaspberryPi 4 B (4GB)
- 10 Ventile 12V
- 2 Pumpen (aktuell nur eine 12V im Betrieb)
- 16 Analoge/Digitale Pins für Sensoren & andere Messungen (Bodenfeuchte, Photoresistor, etc.)
- BME280 Temperatur/Luftfeuchtigkeit/Druck Sensor
- RTC DS3231 Real Time Clock
- Bewässerungs Kit: Schläuche, Sprinkler, etc.
- 20W 12V Solar Panel
- 12 Ah 12V Bleiakku
- Solarladeregler

## Steuerung der Bewässerung

## Steuerung der Bewässerung

Die Bewässerung wird mit den Einstellungen in der config.JSON-Datei gesteuert. Dabei werden 2 Variablen verwendet - timetable und water-time:

- timetable: Wird durch die ersten 24 Bit einer Integer-Zahl repräsentiert, wobei jedes Bit für eine Stunde des Tages steht. Um Platz zu sparen, wird es als Long Integer abgespeichert.
- watertime: Ist die Zeit (in Sekunden), die die jeweilige Gruppe bewässert wird. Hierbei können mehrere Ventile oder Ventil + Pumpe gleichzeitig gesetzt werden.

<br>

```cpp
// Beispiel-Zeitplan                      23211917151311 9 7 5 3 1
//                                       | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                       | | | | | | | | | | | |
// Dezimale Darstellung: 1049600        22201816141210 8 6 4 2 0
//
```

Die Steuerung selbst kann auf folgenden Arten erfolgen:

- MQTT über W-LAN (Telefon oder Pi) (noch nicht wieder implementiert)
- Config-Datei im Flash (SPIFFS) wenn kein Netzwerk gefunden wurde

### config.JSON:

In dieser Datei befinden sich alle relevanten Einstellungen. Diese wird dank Node-Red vom Raspberry Pi innerhalb des Netzwerks zur Verfügung gestellt und kann vom Mikrocontroller abgefragt werden. Hierfür muss die Adresse in der connection.h angepasst werden. <br>
Somit kann über Änderungen der beiden genannten Variablen in die Bewässerung eingegriffen werden.

```json
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

Der verwendete Node-Red-Flow befindet sich im Ordner node-red-flows. Gegebenenfalls Adresse bzw. Pfad zur Datei ändern.

## Konfiguration der Sensoren:

TODO: fill out section

<br>

# Details

## Platinen
Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen. Ab einer gewissen Größe des Projekts ist fritzing allerdings nicht mehr ideal. Gerber files sind vorhanden. Für die Fritzing files ist lediglich die Platinen Ansicht relevant. Alle Boards verwenden auf den ESP-32 als Mikrocontroller. <br>


 Board 1:                  | Board 3:                  | Board 5:
:-------------------------:|:-------------------------:|:-------------------------:
![Board1](/docs/pictures/bewae3_3_board1v3_838_Leiterplatte.png) | ![Board3](/docs/pictures/bewae3_3_board3v22_Leiterplatte.png) | ![Board5](/docs/pictures/bewae3_3_board5v5_final_Leiterplatte.png)

### **bewae3_3_board1v3_838.fzz** Hauptplatine (PCB)
Große Hauptplatine. Platz für bis zu 16 analoge Sensoren sowie BME280- und RTC-Modul über I2C. 8 Pins für Ventile/Pumpen (erweiterbar). Diese können entweder über Relais oder dem PCB Board 3 einfach verwendet werden.<br>

### **bewae3_3_board3v22.fzz** als Erweiterung (PCB)
Als Erweiterung gedacht. Hat den Zweck, Steckplätze für Sensoren und weitere Verbraucher bereitzustellen. Zur Erweiterung der 8 Pins der Hauptplatine kann das Schieberegister verwendet werden. Insgesamt 16 Steckplätze für Sensoren inklusive Versorgung, sowie 2 Pumpen (*12V*) und 10 kleinere Ventile (*12V*).<br>

### **bewae3_3_board5v5_final.fzz** als Hauptplatine (PCB)
Kleinere Hauptplatine. Verzichtet auf eine große Anzahl an Sensoranschlüssen. Platz für 2x 5V Sensoren, 2x 3V Sensoren, 1x LDR, I2C-Bus, 1-Wire-Bus. BME280- und RTC-Modul sind ebenfalls vorgesehen.<br>


## Code ESP32
[Arduino-Nano Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>

### Installation des codes:
Stellen Sie zunächst sicher, dass PlatformIO in Visual Studio Code installiert ist. Öffnen Sie dann den Projektordner in VS Code. Schließen Sie als Nächstes Ihren ESP-32-Controller an und klicken Sie auf das Pfeilsymbol in der PlatformIO-Taskleiste, um Ihren Code hochzuladen. Das Gerät sollte automatisch erkannt werden. <br>

Bevor Sie das Programm auf dem Controller ausführen, überprüft es, ob einige Geräte antworten. Stellen Sie daher sicher, dass sie ordnungsgemäß verbunden sind. <br>

Vergessen Sie nicht, Ihre Verbindungseinstellungen zu aktualisieren. Ändern Sie einfach die Datei 'connection.h' im Ordner 'src', um sie an Ihre (Netzwerk-)Konfiguration anzupassen. <br>

### ändern der Zeit am RTC Modul:

Stellen Sie sicher, dass Sie die Zeit des RTC einstellen, entweder durch Einrichten eines Codebeispiels für dieses Gerät oder durch Kommentieren meiner Funktion im Setup. Wenn meine Methode verwendet wird, vergessen Sie nicht, sie erneut auszukommentieren, da sonst die Zeit bei jedem Neustart zurückgesetzt wird. <br>

## Raspberrypi

### Node-Red

TODO: update documentation

### InfluxDB 2.0

TODO: update documentation

### Grafana (OUTDATED!)
![grafana dashboard example](/docs/pictures/grafanarainyday.png "Grafana rainy day") <br>

[[Tutorials]](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) Grafana kann genutzt werden um die gesammelten Daten in schönen plots darzustellen. Somit ist eine Überwachung der Feuchtigkeitswerte der Pflanzen sehr leicht möglich. <br>

Sobald Grafana installiert ist kann das [json](/grafana_dashboard/bewaeMonitor.json) export (OUTDATED!!) über das Webinterface importiert werden. <br>

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

Auf mein **Android** Smartphone habe ich die App mqttdash geladen. Diese ist sehr einfach und intuitiv zu verwenden man muss Adresse Nutzer und Passwort die oben angelegt wurden eintragen und kann dann die Topics konfigurieren. 

Ein Beispiel Screenshot aus der App, die Zahlen stehen für die Zeit (s) in der das jeweilige Ventil geöffnet ist und Wasser gepumpt wird (ca. 0.7l/min):
![mqttdash app](/docs/pictures/mqttdash.jpg) <br>

## Bilder & Entstehung

**Entstehung:** <br>
Das Projekt entstand aus einer mehrwöchigen Abwesenheit, während der die Balkonpflanzen ohne Versorgung gewesen wären. Die Pflanzen sollten mithilfe eines Bewässerungssets, Schläuchen, ein paar Düsen und einer Pumpe am Leben gehalten werden. Eine reguläre Bewässerung wie bei einer Zeitschaltuhr erschien mir jedoch zu langweilig, und mein Interesse an einem kleinen Bastelprojekt war geweckt. Kapazitive Bodenfeuchtesensoren und 4 kleine 12V Ventile wurden schnell bestellt, da ich zu Hause genügend Arduinos hatte. So kam es innerhalb einer Woche zur [Version 1](#v1), und das Überleben der Pflanzen war gesichert. Aus der Not heraus entstand ein Projekt, das mich eine Weile beschäftigte, und durch kontinuierliche Erweiterung und Verbesserung erfüllt es nach momentanem Stand weit mehr, als ursprünglich geplant. <br>
<br>

Zum Abschluss eine kleine Sammlung von Fotos über mehrere Versionen des Projekts, die im Laufe der Zeit entstanden sind:

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
