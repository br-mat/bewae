# Bewae - Bewässerungsprojekt v3.1
## Einleitung:
Diese Version ist eher als Testversion zu sehen.  Neu hinzugefügt und getestet wird nun die Steuerbarkeit über **MQTT**. <br>
Bei der bisherigen Aufarbeitung handelt es sich **nicht** um eine Step-by-step Anleitung es ist ein wenig Grundwissen im Umgang mit Linux und Mikrokontrollern vorrausgesetzt. <br>
Ursprünglich war das Projekt für den Offlinebetrieb gedacht, es wäre natürlich einfacher einen **ESP-32** zu verwenden anstatt einem **Nano** mit einem **ESP-01** ein "Upgrade" zu verpassen. Da die Platine aber schon bestellt wurde habe ich mich dagegen entschieden, es war ein Mehraufwand der nicht notwendig aber lehrreich gewesen ist. <br>

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
- Automatisierte Bewässerung der Balkonpflanzen
- Speichern von Sensordaten & cool graphs
- Überwachung & Steuerung von unterwegs

### Beschreibung:
Schaltungsaufbau grob übersichtlich im beigefügten **Systemdiagramm.png** als Blockschaltbild. Die Daten werden via **ESP01** über **MQTT** an den RaspberryPi gesendet, dort gespeichert und dank Grafana als schöne Diagramme dargestellt.
Automatisiert bewässert wird momentan *2* mal Täglich morgends und abends, theoretisch sind bis zu *6* Ventile und *2* Pumpen schalt und steuerbar (erweiterbar). Mit einem **MQTT** messaging client ist möglich in die bewässerung einzugreifen und gespeicherte Werte zu verändern sowie die Messdaten über Grafana im Auge zu behalten.
In den weiteren Ordnern befinden sich der Code für beide Controller sowie das json export für das Grafana Dashboard. Die Konfiguration des RaspberryPi fehlt noch, die verwendete Datenbank ist Influxdb. (Die Scripts befinden sich am Pi im ordner pi_scripts und werden automatisiert gestartet mit cron als su)

## Systemdiagramm
![System](/pictures/Systemdiagramm.png "Systemdiagramm")

## Platinen
**bew_entwurf_v2_7.fzz** Hauptplatine als PCB
**Platine_01.fzz** als Erweiterung mit Esp01, Buckconverter etc. selbst gelötet <br>
Auf die verkabelung wird nicht weiter eingegangen, jedoch ein paar hinweise:
- A4/A5 (SDA/SCL) richtig mit Platine_01 verbinden (reihenfolg im Code beachten)
- Da Platine_01 einen micro USB anschluss besitzt empfiehlt es sich diesen zu verwenden und von dort die Hauptplatine über jumper zu versorgen

Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen. Ab einer gewissen größe des Projekts ist fritzing allerdings nicht mehr ganz ideal. <br>
![Main PCB](/pictures/pictures/bewae_v3_1.png "Main board")

## Details:
## Kommunikation:
### I²C:
I²C (Inter-Integrated Circuit) ein serieller Datenbus der von Philips Semiconductors entwickelt wurde und hauptsächlich für sehr kurze Strecken (gerätintern) vorgesehen ist. <br>

Im Fall dieses Projekts spielt die Kommunikation der beiden Mikrocontroller eine tragende Rolle. Grundsätzlich ist ein Multi-master Betrieb möglich wenn auch oft nicht empfohlen. Für die fehlerfreie Kommunikation in beide Richtungen müssen leider beide Controller als Master dem Bus beitreten. Hauptsächlich als Master verwende ich den **Arduino Nano** der dem **ESP-01** signalisiert wann er als Master die Kommunikation übernehmen darf um Komplikationen zu vermeiden.

### MQTT:
(beschreibung ergänzen)
(alle relevanten Topics sind auf den controllern zu finden hier folgt eine auflistung:)

## Code Arduino Nano:
[Arduino-Nano Code](/bewae_main_nano/bewae_v3_nano/src/main.cpp)
<br>
### Allgemein:
Der Code wird mit Visual Studio Code und der Platformio extension aufgespielt, VS code bietet eine umfangreichere IDE als die Arduino IDE und ist damit für das deutlich umfangreichere Programm einfach besser geeignet. <br>

- Damit das Programm ausgeführt werden kann werden alle Module benötigt. Es muss eine SD Karte als Backup gesteckt sein, der BME280 muss funktionieren sowie das RTC Modul.
- Um die fehlerfreie Kommunikation zum **ESP** als "slave" zu ermöglichen setze ich im Register TWBR manuel wie in der Setup Funktion ersichtlich, dies ermöglicht eine präzise Steuerung des I²C Bus Taktes über SCL. Durch "trial and error" bin ich zu dem Wert **TWBR=230** gekommen. Es gibt auch noch eine zweite Möglichkeit die Frequenz zu ändern mit **Wire.setClock(clockFrequency)**, dies hatte jedoch leider öfter zu Fehlern geführt (nicht nur in der Kommunikation). 
```
#Formula:
freq = clock / (16 + (2 * TWBR * prescaler)) #für den fall TWBR=230 --> 8.62 kHz
```

## Code ESP8266-01:
[ESP8266-01 Code](/esp01_bewae_reporterv3_4.ino/esp01_bewae_reporterv3_4.ino)
<br>

### Allgemein:
Dieser Mikrocontroller wird verwendet um dem System den Zugang zum Lokalen Netzwerk zu verschaffen. Die Kommunikation erfolgt seriell mit I²C. Den **ESP-01** als "slave" des **Nano** zu verwenden bringt einige Probleme mit sich. Die CPU Frequenz des **ESP-01** muss auf *160MHz* statt den standardmäßigen *80MHz* angehoben werden und am **Nano** muss die Frequenz des Buses reduziert werden (siehe **Arduino-Nano** Code, Allgemein). Damit auch der **ESP-01** dem Arduino zuverlässig Daten senden kann wird der Bus mit *2* Master also Multimasterbetrieb betrieben. <br>Nach all diesen Anpassungen funktioniert die Kommunikation sehr stabil. Sehr selten kommt es zu kleinen Fehlern in der Übertragung, deshalb wird am Schluss einer Übertragung als letztes Byte eine festgelegte Zahl gesendet, die der Zuordnung und Kontrolle zum Ausschluss von Fehlern dient.

### Details: Programmierung mit Arduino IDE
Für die Programmierung des **ESP8266-01** empfiehlt es sich Programier-Module zu verwenden. Ich verwende ein Modul auf Basis CH340, hierzu muss man die Treiber im vorhinein installieren (LINK!!!!!). Wichtig ist dabei auf die Spannung zu achten je nach Modul kann es notwendig sein den Output erst auf **3.3V** zu regulieren. <br>
Um die CPU Frequenz des **ESP** beim Programmieren mit der Arduino IDE zu ändern muss man beim Menüpunkt 'File' --> 'Preferences' und unter 'Additional Boards Manager URLs:' Folgendes ergänzt werden (mehrere URLs einfach mit Kommas trennen) *http://arduino.esp8266.com/stable/package_esp8266com_index.json* <br>
Nun kann man unter dem Menüpunkt 'Tools' --> 'Board' nach 'Generic ESP8266 Module' wählen. Nachdem die restlichen Einstellungen - wie aus dem Bild zu entnehmen - vorgenommen wurden, ist die IDE bereit für den Upload. Eventuell fehlende librarys müssen gegebenenfalls noch installiert werden, dies ist einfach über den library Manager möglich. <br>
![ESP01 upload configuration](/pictures/esp01upload.png "Upload configuration")

## Raspberrypi
### Allgemein:
Persönlich verwende ich gerade einen RaspberryPi 4 mit 4GB Ram auf dem auch ein VPN (wireguard) und Pihole installiert sind. Durch den VPN kann ich auch von unterwegs auf die Daten zugreifen oder gegebenenfalls in die Bewässerung eingreifen. Auf die Installation gehe ich nicht weiter ein, sie ist aber relativ einfach und in zahlreichen Tutorials gut beschrieben. <br>Für den Betrieb genügt auch die Verfügbarkeit im lokalen Netzwerk, hierbei notwendig sind:
- SSid (Netzwerkname): BeispielNetzwerkXY
- Passwort:            BeispielpasswortXY
- IP oder hostname des RaspberryPi (IP statisch vergeben)
Für die erleichterte Konfiguration entweder Bildschirm, ssh, teamviewer, vnc viewer ... am Raspberrypi auch auf diese Punkte gehe ich nicht weiter ein, da sie anderswo gut zu finden sind. <br>

### InfluxDB:
Falls influx noch nicht installiert wurde, gibt es nach kurzer Suche jede menge brauchabre Anleitungen z.B: [(pimylifeup)](https://pimylifeup.com/raspberry-pi-influxdb/).
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
Um aus diesem "interface" wieder herauszukommen muss man einfach den Befehl **exit** eingeben und man befindet sich wieder im terminal.
```
exit
```

### Grafana:
Auch hier gibt es sehr gute [Tutorials](https://grafana.com/tutorials/install-grafana-on-raspberry-pi/) auf die man zurückgreifen kann, daher gehe ich nicht genauer auf den Installationsprozess ein. <br>

Sobald Grafana installiert ist kann das [json](/grafana_dashboard/bewaeMonitor.json) export über das Webinterface importiert werden. <br>

Unter 'Configuration' muss man nun die 'Datasources' eintragen. Hierbei muss man nun darauf achten die gleichen Datenbanken zu verwenden die man erstellt und in der die Daten abgespeichert werden. In meinem Fall wäre das z.B ***main*** für alle Daten der Bewässerung. Sowie ***pidb*** für die CPU Temperatur am RaspberryPi die im Import mit dabei ist, jedoch rein optional. <br>

![Datasource configuration](/pictures/datasources.png "Datasource configuration example") <br>

### MQTT&Python:
Um am die über **MQTT** gesedeten Daten nun in die Datenbank schreiben oder lesen zu können wird das Python script [MQTTInfluxDBBridge3.py](/pi_scripts/MQTTInfluxDBBridge3.py) verwendet. Der Python code kann mit dem shell script [launcher1.sh](/pi_scripts/launcher1.sh) automatisiert mit crontab bei jedem Bootvorgang mitgestartet werden. Da der Pi beim Hochfahren eine gewisse Zeit benötigt um alles fehlerfrei zu starten, verzögere ich den Start des scripts um *20* Sekunden. <br>
Um Fehler zu vermeiden sollten über **MQTT** nur **int** Werte verschickt werden (*2* **byte**), der Datentyp **int** ist am **Arduino Nano** *2* **byte** groß.

#### Raspi status Daten (optional)
Ein sehr einfaches script für die Aufzeichnung und Speicherung der CPU Temperatur und Ram Auslastung am RaspberryPi. WO WIE WAS?

## Bilder:

![Bild](/pictures/bewaeV1(2).jpg) <br>
![Bild](/pictures/bewaeV1.jpg) <br>
![Bild](/pictures/bewaeV2.jpg) <br>
![Bild](/pictures/bewaeV3.jpg) <br>
![Bild](/pictures/MainPCB.jpg) <br>