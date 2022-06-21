# Bewae - Bewässerungsprojekt v3.3

v3.3 WIP versions now are not stable

- added openweather scripts (saving current conditions & air condition to influxDB)
- added corresponding .sh launcher files
- added english translation
- added ESP32 build (dropping nano in future?)
- added 2nd circuit handling pumps & solenoids

todo:
- added openwather forecast script
- added dynamic watering controlled by Pi (using ML later)
- updating main circuit, correct bugs & problems on ESP32 build

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
- [Details](#details)
  * [Platinen](#platinen)
  * [Code Arduino Nano](#code-arduino-nano)
  * [Code ESP8266-01](#code-esp8266-01)
  * [RaspberryPi](#raspberrypi)
- [Bilder](#bilder)

## Introduction (EN)
This version is more of a test version. The controllability via [*MQTT*](#mqtt) is now added and tested. <br>
This is **not** a step-by-step guide - a little basic knowledge in dealing with Linux and microcontrollers is required. It should only be given an insight into the project and make it easier for me to rebuild in spring or at new locations. That's why I decided to use German, but I would like to add an English version later. <br>

## Einleitung (DE)
Diese Version ist eher als Testversion zu sehen.  Neu hinzugefügt und getestet wird nun die Steuerbarkeit über [*MQTT*](#mqtt). <br>
Bei der bisherigen Aufarbeitung handelt es sich **nicht** um eine Step-by-step Anleitung es ist ein wenig Grundwissen im Umgang mit Linux und Mikrokontrollern vorausgesetzt. Es soll lediglich ein Einblick in das Projekt gegeben werden und mir den Wiederaufbau im Frühjahr oder an neuen Standorten erleichtern. Deshalb habe ich mich auch für Deutsch entschieden, möchte jedoch noch eine Englische Version ergänzen. <br>
<br>
**Entstehung:** <br>
Das Projekt selbst entstand aus einer mehrwöchigen Abwesenheit in der die Balkonpflanzen ohne Versorgung gewesen wären. Das grün sollte mit einem Bewässerungsset, Schläuche und ein paar Düsen und einer Pumpe am Leben gehalten werden. Eine Reguläre Bewässerung wie bei einer Zeitschaltuhr kam mir jedoch zu langweilig vor und mein Interesse an einem kleinen Bastelprojekt war geweckt. Kapazitive Bodenfeuchte Sensoren und 4 kleine *12V* Ventile waren schnell bestellt, Arduinos hatte ich genug zu Hause, so kam es innerhalb einer Woche zu [Version 1](#v1) und das Überleben der Pflanzen war gesichert. Es entstand aus der Not ein Projekt das mich eine Weile Beschäftigt hat, kontinuierlich erweitert und verbessert erfüllt es nach momentanem Stand weit mehr als zuerst geplant. <br>
<br>
**Stand jetzt:** <br>
**Technik:** Für die einfachere Handhabung wurde mit [fritzing](https://fritzing.org/) ein Plan für ein [PCB](#platinen) erstellt und gedruckt. Sensordaten von diversen Sensoren (Bodenfeuchte, Temperatur etc.) werden an einen [RaspberryPi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) gesendet und dort in eine Datenbank gespeichert. Die Steuerung der Bewässerung funktioniert entweder automatisiert Sensorgesteuert oder über eine [MQTT-messaging App](#mqttdash-app-optional) über das Smartphone. Dank [VPN](https://www.pivpn.io/) sind auch von unterwegs Steuerung und Beobachtung möglich. <br>
**Versorgung:** Mittlerweile wurde daraus ein autonom funktionierendes System, ausgestattet mit einem kleinen PV Modul einem Blei Akku und einem kleinen *100l* Wassertank. Je nach Temperatur muss nun nur noch einmal pro Woche daran gedacht werden den Tank zu füllen. <br>

### aktueller Aufbau
- Arduino Nano
- ESP-8266 01
- RaspberryPi 4 B (*4GB*)
- *6* Ventile *12V*
- *2* Pumpen (aktuell nur eine *12V* im Betrieb)
- *16* Analoge/Digitale Pins für Sensoren & andere Messungen (Bodenfeuchte, Photoresistor, etc.)
- BME280 Temperatur/Luftfeuchtigkeit/Druck Sensor
- RTC DS3231 Real Time Clock
- micro SD module & micro SD Karte
- Bewässerung Kit Schläuche, Sprinkler, etc.
- *10W 12V* Solar Panel
- *7.2 Ah* *12V* Bleiakku
- Solarladeregler

### Beschreibung
Übersicht des Schaltungsaufbaus im beigefügten [*Systemdiagramm.png*](#systemdiagramm) als Blockschaltbild. Sensoren und Module zeichnen jede Menge Daten auf, dazu zählen Bodenfeuchtigkeits Sensoren, Temperatur-, Luftfeuchte- und Luftdruckdaten sowie Sonnenscheindauer. Die Daten werden via [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) an den [RaspberryPi](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/) gesendet, dort gespeichert und dank Grafana als schöne Diagramme dargestellt.
Automatisiert bewässert wird momentan *2* mal Täglich morgens und abends, aktuell sind bis zu *6* Ventile und *2* Pumpen schalt und steuerbar (erweiterbar). Die Bewässerung passt sich in der aktuellen Version nicht mehr an die Messdaten aus den Feuchtigkeitssensoren im Boden an, um die Steuerung über die [*mqttdash*](https://play.google.com/store/apps/details?id=net.routix.mqttdash&hl=en&gl=US) App zu testen, wird aber später wieder hinzugefügt. Mit einem [*MQTT*](#mqtt) messaging client ist möglich in die Bewässerung einzugreifen und gespeicherte Werte zu verändern. Die Messdaten können über Grafana im Auge zu behalten werden, so kann man mit eingerichtetem [*VPN*](https://www.pivpn.io/) auch von unterwegs seine Pflanzen im Auge behalten und Notfalls eingreifen.
In den weiteren Ordnern befinden sich der Code für beide Controller sowie das json export für das Grafana Dashboard und die Python scripts zur Verarbeitung der [*MQTT*](#mqtt) messages. Die verwendete Datenbank ist [*InfluxDB*](#influxdb) in der alle gesendeten Daten gespeichert werden. Alle relevanten Programme und Scripte starten automatisiert dank crontab bei jedem bootvorgang. <br>
Ursprünglich war das Projekt für den Offlinebetrieb gedacht, es wäre natürlich einfacher einen [*ESP-32*](https://de.wikipedia.org/wiki/ESP32) zu verwenden anstatt einem [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) mit einem [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) ein "Upgrade" zu verpassen. Da die Platine für einen Arduino Nano konzipiert und schon bestellt wurde habe ich mich für die zweite Variante entschieden, ein Mehraufwand der nicht notwendig aber lehrreich gewesen ist. <br>


## Systemdiagramm
### V3.0:
![System](/docs/pictures/Systemdiagramm.png "Systemdiagramm")
### V3.3 preview:
![System](/docs/pictures/Systemdiagramm3_3.png "Systemdiagramm 3.3")
# Details

## Platinen
Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen. Ab einer gewissen größe des Projekts ist fritzing allerdings nicht mehr ideal. <br>
Auf die Verkabelung wird nicht weiter eingegangen, jedoch ein paar hinweise:
- A4/A5 (SDA/SCL) richtig mit Platine_01 verbinden (reihenfolg im Code beachten)
- Da Platine_01 einen micro USB Anschluss besitzt empfiehlt es sich diesen zu verwenden und von dort die Hauptplatine über jumper zu versorgen <br>

### **bew_entwurf_v2_7.fzz** Hauptplatine (PCB)
Hat den Zweck das System zu Steuern und alle Daten zu Sammeln. Es werden alle relevanten Sensoren und Module sowie Steuerungen auf diese Platine zusammengeführt und von einem [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) verarbeitet. Für den Offline betrieb alleine würde diese Platine ausreichen. Ursprünglich sollte das System über Powerbanks versorgt werden. Um zu verhindern, dass die Powerbank abschaltet ist ein *NE555*-Schaltung eingeplant die, richtig Dimensioniert, den Schutzmechanismus der Powerbank bei geringer last umgeht. Im momentanen Aufbau werden jedoch keine Powerbanks verwendet daher ist dieser Teil der Platine obsolet. Die gesammelten Daten werden auf eine SD Karte gespeichert. Diese ist nach aktuellem stand Notwendig durch Anpassungen im [Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp) könnte man sie aber auch weglassen. <br>

![Main PCB](/docs/pictures/bewae_v3_1.png "Main board")

<br>

### **Platine_01.fzz** als Erweiterung mit Esp01, Buckconverter etc. (Lochrasterboard)
Sie ist als Erweiterung gedacht um eine Anbindung an das Netzwerk zu ermöglichen sowie größere Spannungen (*12V*) Schalten zu können. Auch die Spannung der Bleibatterie (Akku) soll über den Spannungsteiler abgegriffen werden können. Außerdem besitzt sie einen micro USB Anschluss und kann damit gut per USB Kabel vom Solarladeregler versorgt werden. So wird kein extra Spannungswandler von *12* auf *5* Volt benötigt. Von hier aus kann auch die Hauptplatine versorgt werden. Der [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) kann bei richtiger Verkabelung über I²C mit dem [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) kommunizieren sowie über den *'enable Pin'* ein und ausgeschalten werden um Strom zu sparen.
<br>

## Code Arduino Nano
[Arduino-Nano Code](/code/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>
### Allgemein
Der Code wird mit Visual Studio Code und der Platformio extension aufgespielt, VS code bietet eine umfangreichere IDE als die Arduino IDE und ist damit für das deutlich umfangreichere Programm einfach besser geeignet. <br>

- Damit das Programm ausgeführt werden kann werden alle Module benötigt. Es muss eine SD Karte als Backup gesteckt sein, der BME280 muss funktionieren sowie das RTC Modul.
- Um die fehlerfreie Kommunikation zum [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) als "slave" zu ermöglichen setze ich im Register TWBR manuell wie in der Setup Funktion ersichtlich, dies ermöglicht eine präzise Steuerung des [*I²C*](#ic) Bus Taktes über SCL. Durch "trial and error" bin ich zu dem Wert **TWBR=230** gekommen. Es gibt auch noch eine zweite Möglichkeit die Frequenz zu ändern mit **Wire.setClock(clockFrequency)**, dies hatte jedoch leider öfter zu Fehlern geführt (nicht nur in der Kommunikation). 
```
#Formula:
freq = clock / (16 + (2 * TWBR * prescaler)) #für den fall TWBR=230 --> 8.62 kHz (prescaler = 4, clock = 16MHz)
```

### I²C
**I²C** (Inter-Integrated Circuit) ein serieller Datenbus der von Philips Semiconductors entwickelt wurde und hauptsächlich für sehr kurze Strecken (gerätintern) vorgesehen ist. <br>

Im Fall dieses Projekts spielt die Kommunikation der beiden Mikrocontroller eine tragende Rolle. Grundsätzlich ist ein Multi-master Betrieb möglich wenn auch oft nicht empfohlen. Für die fehlerfreie Kommunikation in beide Richtungen müssen leider beide Controller als Master dem Bus beitreten. Hauptsächlich als Master verwende ich den [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) der dem [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) signalisiert wann er als Master die Kommunikation übernehmen darf um Komplikationen zu vermeiden.


## Code ESP8266 01
[ESP8266-01 Code](/code/esp01_bewae_reporterv3_5_beta/esp01_bewae_reporterv3_5_beta.ino)
<br>

### Allgemein
Dieser Mikrocontroller wird verwendet um dem System den Zugang zum Lokalen Netzwerk zu verschaffen. Dabei stellt der [*ESP8266-01*](https://de.wikipedia.org/wiki/ESP8266) die Verbindung ins WLAN-Netz her und die Kommunikation mit dem [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) erfolgt seriell mit I²C. Den [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) als "slave" des [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) zu verwenden bringt einige Probleme mit sich daher tritt auch dieser Mikrocontroller dem Bus als Master bei. Für eine fehlerfreie Funktion müssen die CPU Frequenz des [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) muss auf *160MHz* statt den standardmäßigen *80MHz* angehoben werden und am [*Arduino Nano*](https://store.arduino.cc/products/arduino-nano) die Frequenz des Buses reduziert werden ([siehe TWBR](#code-arduino-nano)). Damit auch der [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) dem Arduino zuverlässig Daten senden kann wird der Bus nun mit *2* Master also Multimasterbetrieb betrieben. <br>Nach all diesen Anpassungen funktioniert die Kommunikation sehr stabil. Sehr selten kommt es zu kleinen Fehlern in der Übertragung, deshalb wird am Schluss einer Übertragung als letztes Byte eine festgelegte Zahl gesendet, die der Zuordnung und Kontrolle zum Ausschluss von Fehlern dient.

### Programmierung mit Arduino IDE
Für die Programmierung des [*ESP8266-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) empfiehlt es sich Programmier-Module zu verwenden. Ich verwende ein Modul auf Basis CH340, hierzu muss man die Treiber im Vorhinein [installieren](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all). Wichtig ist auf die Spannung zu achten je nach Modul kann es notwendig sein den Output erst auf *3.3V* zu schalten. <br>
Um die CPU Frequenz des [*ESP-01*](https://de.wikipedia.org/wiki/ESP8266) über [*MQTT*](#mqtt) beim Programmieren mit der Arduino IDE zu ändern muss man beim Menüpunkt 'File' --> 'Preferences' und unter 'Additional Boards Manager URLs:' Folgendes ergänzt werden (mehrere URLs einfach mit Kommas trennen) *http://arduino.esp8266.com/stable/package_esp8266com_index.json* <br>
Nun kann man unter dem Menüpunkt 'Tools' --> 'Board' nach 'Generic ESP8266 Module' wählen. Nachdem die restlichen Einstellungen - wie aus dem Bild zu entnehmen - vorgenommen wurden, ist die IDE bereit für den Upload. Eventuell fehlende librarys müssen gegebenenfalls noch installiert werden, dies ist einfach über den library Manager möglich. <br>
![ESP01 upload configuration](/docs/pictures/esp01upload.png "Upload configuration")

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
  topic_prefix = "home/sens3/z";
  #send multiple measurments from nano to raspberry pi unter fortlaufender nummer die angehängt wird

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
### V1

![Bild](/docs/pictures/bewaeV1(2).jpg) <br>
![Bild](/docs/pictures/bewaeV1.jpg) <br>

### V2

![Bild](/docs/pictures/bewaeV2.jpg) <br>

### V3

![Bild](/docs/pictures/bewaeV3(Sommer).jpg) <br>
![Bild](/docs/pictures/bewaeV3(Herbst).jpg) <br>
![Bild](/docs/pictures/bewaeV3(Box).jpg) <br>
![Bild](/docs/pictures/MainPCB.jpg) <br>
