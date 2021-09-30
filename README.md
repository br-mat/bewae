# Bewae - Bewässerungsprojekt v3.1
## Einleitung:
Diese Version ist eher als Testversion zu sehen.  Neu hinzugefügt und getestet wird nun die steuerbarkeit über **MQTT**. <br>
Bei der bisherigen Aufarbeitung handelt es sich **nicht** um eine Step-by-step Anleitung es ist ein wenig Grundwissen im Umgang mit Linux und Mikrokontrollern vorrausgesetzt. <br>
Ursprünglich war das Projekt für den Offlinebetrieb gedacht, es wäre natührlich einfacher einen **ESP-32** zu verwenden anstatt dem **Nano** mit einem **ESP-01** ein "Upgrade" zu verpassen. Da die Platine aber schon bestellt wurde habe ich mich dagegen entschieden, es war ein Mehraufwand der nicht Notwendig aber lehrreich gewesen ist.<br>

### aktueller Aufbau:
- Arduino Nano
- ESP-8266 01
- RaspberryPi 4 B (*4GB*)
- *6* Ventile *12V*
- *2* Pumpen (aktuell nur eine *12V* im Betrieb)
- *16* Analoge/Digitale Sensoren
- Bewässerung Kit Schläuche, Sprinkler, etc.
- *10W 12V* Solar Panel
- *7.2 Ah* *12V* Bleiakku
- Solarladeregler

### Ziele:
- Automatisierte bewässerung der Balkonpflanzen
- Speichern von Sensordaten & cool graphs
- Überwachung & Steuerung von unterwegs

### Beschreibung:
Schaltungsaufbau grob übersichtlich im beigefügten Systemdiagramm.png als Blockschaltbild. Die Daten werden via **ESP01** über **MQTT** an den RaspberryPi gesendet, dort gespeichert und dank Grafana als schöne Diagramme dargestellt.
Automatisiert bewässert wird momentan *2* mal Täglich morgends und abends, theoretisch sind bis zu *6* Ventile und *2* Pumpen schalt und steuerbar (erweiterbar). Mit einem **MQTT** messaging client ist möglich in die bewässerung einzugreifen und gespeicherte werte zu verändern sowie die Messdaten über Grafana im auge zu behalten.
In den weiteren Ordnern befinden sich der Code für beide Controller sowie das json export für das Grafana dashboard. Die Konfiguration des Raspberrypi fehlt noch, die verwendete Datenbank ist Influxdb. (Die Scripts befinden sich am Pi im ordner py_scripts und werden automatisiert gestartet mit cron als su)

## Systemdiagramm
![System](/Systemdiagramm.png "Systemdiagramm")

## Mainboard (bew_entwurf_v2_7)
**bew_entwurf_v2_7.fzz** Hauptplatine
**Platine_01.fzz** als Erweiterung mit esp01, Buckconverter etc. <br>

Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen.<br>
![Main PCB](/bewae_v3_1.png "Main board")

## Details:
### Kommunikationn:
### I2C:
I²C (Inter-Integrated Circuit) ein Serieller Datenbus der von Philips Semiconductors entwickelt wurde und hauptsächlich für sehr kurze strecken (gerätintern) vorgesehen ist. <br>

Im fall dieses Projektes spielt die Kommunikationn der beiden Mikrocontroller eine tragende Rolle. Grundsätzlich ist ein Multi-master betrieb möglich wenn auch oft nicht empfohlen. Für die fehlerfreie kommunikation in beide Richtungen müssen leider beide controller als Master dem Bus beitreten. Hauptsächlich als Master verwende ich den Arduino Nano der dem **ESP-01** signalisiert wann er als Master die Kommunikationn übernehmen darf um komplikationen zu vermeiden.

### MQTT:
(beschreibung ergänzen)
(alle relevanten Topics sind auf den controllern zu finden hier folgt eine auflistung:)

## Code Arduino nano:
[Arduino-Nano Code](/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>
### Allgemein:
Der Code wird mit Visual Studio Code und der Platformio extension aufgespielt, VS code bietet eine Umfangreichere IDE als die Arduino IDE und ist damit für das deutlich Umfangreichere Programm einfach besser geeignet. <br>

- Damit das Programm ausgeführt werden kann werden alle Module benötigt. Es muss eine SD Karte als backup gesteckt sein, der BME280 muss funktionieren sowie das RTC Modul.
- Um die fehlerfreie kommunikation zum ESP als "slave" zu ermöglichen setze ich im register TWBR manuel wie in der Setup funktion ersichtlich, dies ermöglicht eine präzise steuerung des I²C Bus taktes über SCL. Durch Trial and error bin ich zu dem Wert **TWBR=230** gekommen grundsätzlich hat aber auch höher funktioniert, es gibt auch noch eine zweite möglichkeit die frequenz zu ändern mit **Wire.setClock(clockFrequency)** dies hatte jedoch leider öfter zu fehlern geführt (nicht nur in der Kommunikation). 
```
#Formula:
freq = clock / (16 + (2 * TWBR * prescaler)) #für den fall TWBR=230 --> 8.62 kHz
```

## Code ESP8266-01:
[ESP8266-01 Code](/bewae_esp01/esp01_bewae_reporterv3_4.ino)
<br>

### Allgemein:
Dieser Mikrocontroller wwird verwendet um dem System Zugang zum Lokalen Netzwerk zu verschaffen. Die Kommunikation erfolgt Seriel mit i2c. Den **ESP-01** als slave des Nanos zu verwenden bringt einige Probleme mit sich. Die CPU frequenz des **ESP-01** muss auf *160MHz* statt den standardmäßigen *80MHz* angehoben werden und am Nano muss die Frequenz des Buses reduziert werden indem . Damit auch der **ESP-01** dem Arduino zuverläßig Daten senden kann wird der Bus mit *2* Master also Multimasterbetrieb betrieben. <br>Nach all diesen Anpassungen funktioniert die Kommunikation sehr stabil, sehr selten kommt es zu kleinen Fehlern in der Übertragung deshalb wird am Schluss einer Übertragung als letztes Byte eine festgelegte Zahl, die der Zuordnung und Kontrolle zum ausschluss von fehlern dient, gesendet.

### Details: Programmierung mit Arduino IDE
Für die Programmierung des ESP8266-01 empfiehlt es sich Programier Module zu verwenden. Ich verwende ein Modul auf basis CH340, hierzu muss man die Treiber im vorhinein installieren (LINK!!!!!). Wichtig ist dabei auf die Spannung zu achten je nach Modul kann es Notwendig sein den Output erst auf **3.3V** zu regulieren. <br>
Um die CPU frequenz des ESP beim programmieren mit der Arduino IDE zu ändern muss man beim Menüpunkt 'File' --> 'Preferences' und unter 'Additional Boards Manager URLs:' folgenden ergänzt werden (mehrere URLs einfach mit Kommas trennen) *http://arduino.esp8266.com/stable/package_esp8266com_index.json* <br>
Nun kann man unter dem Menüpunkt 'Tools' --> 'Board' nach 'Generic ESP8266 Module' wählen. Nachdem die restlichen einstellugnen wie im aus dem Bild zu entnehmen vorgenommen wurden ist die IDE bereit für den Upload eventuell fehlende librarys müssen gegebenenfalls noch Installiert werden, dies ist einfach über den library manager möglich. <br>
![ESP01 upload configuration](/esp01upload.png "Upload configuration")

## Raspberrypi
### Allgemein:
Persöhnlich verwende ich gerade einen RaspberryPi 4 mit 4GB Ram auf dem auch ein VPN (wireguard) und Pihole installiert sind. Durch den VPN kann ich auch von Unterwegs auf die Daten zugreifen oder gegebenenfalls in die Bewässerung eingreifen, auf die Installation gehe ich nicht weiter ein sie ist aber relativ einfach und in zahlreichen Tutorials gut beschrieben. <br>Für den Betrieb genügt auch die verfügbarkeit im lokalen Netzwerk, hierbei notwendig sind:
- SSid (Netzwerkname): BeispielNetzwerkXY
- Passwort:            BeispielpasswortXY
- IP oder hostname des RaspberryPi (IP statisch vergeben)
Für die erleichterte Konfiguration entweder Bildschirm, ssh, teamviewer, vnc viewer ... am Raspberrypi auch auf diese Punkte gehe ich nicht weiter ein da anderswo gut zu finden. <br>

### InfluxDB:
Falls influx noch nicht installiert wurde gibt es nach kurzer suche jede menge brauchabre Anleitungen z.B: [(pimylifeup)](https://pimylifeup.com/raspberry-pi-influxdb/).
<br>

Für Einrichtung der Datenbank beginnen wir erstmal mit dem Erstellen eines users. Dafür wird InfluxDB gestartet indem man in das Terminal folgende Zeile eingiebt.
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
Um aus diesem "interface" wieder herrauszukommen muss man einfach den befehl exit eingeben und man befinden sich wieder im terminal.
```
exit
```

### Grafana:
Auch hier gibt es sehr gute [Tutorials](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) auf die man zurückgreifen kann daher gehe ich nicht genauer auf den Installationsprozess ein. <br>

Sobald Grafana Installiert ist kann das [json](/Pi Monitor bewae Copy-1632943050742.json) export über das Webinterface importieren. <br>

Unter 'Configuration' muss man nun die 'Datasources' eintragen. Hierbei muss man nun darauf achten die Gleichen Datenbanken zu verwenden die man erstellt und in der dei Daten abgespeichert werden in meinem fall wäre das z.B ***main*** für alle daten der Bewässerung. Sowie ***pidb*** für die CPU Temperatur am RaspberryPi die im Import mit dabei ist, jedoch rein optional. <br>

![Datasource configuration](/datasources.png "Datasource configuration example") <br>

### MQTT&Python:
Um am die über **MQTT** gesedeten daten nun in die Datenbank schreiben oder lesen zu können wird ein Python [script](/MQTTInfluxDBBridge3.py) verwendet. Das [script](/launcher1.sh) wird mit einem shell script automatich dank crontab bei jedem bootvorgang mitgestartet. Der Pi beim hochfahren eine gewisse zeit benötigt um alles fehlerfrei zu starten verzögere ich den start des scripts um *20* Sekunden. <br>
Um Fehler zu vermeiden sollten über **MQTT** nur **int** Werte verschickt werden (*2* **byte**), der Datentyp **int** ist am arduino nano *2* **byte** groß.

#### Raspi status Daten (optional)
Ein sehr einfaches script für dei aufzeichnung und speicherung der CPU Temperatur und Ram auslastung am RaspberryPi. WO WIE WAS?
