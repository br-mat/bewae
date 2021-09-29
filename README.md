# Bewae - Bewässerungsprojekt v3.1
Diese Version ist eher als Testversion zu sehen. Neu hinzugefügt und getestet wird nun die steuerbarkeit über MQTT.<br>

In den weiteren Ordnern befinden sich der Code für beide Controller sowie das json export für das Grafana dashboard. Die Konfiguration des Raspberrypi fehlt noch, die verwendete Datenbank ist Influxdb. (Die Scripts befinden sich am Pi im ordner py_scripts und werden automatisiert gestartet mit cron als su)

## Systemdiagramm
![System](/Systemdiagramm.png "Systemdiagramm")

## Mainboard (bew_entwurf_v2_7)
**bew_entwurf_v2_7.fzz** Hauptplatine
**Platine_01.fzz** als Erweiterung mit esp01, Buckconverter etc. <br>

Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen.
![Main PCB](/bewae_v3_1.png "Main board")

## Ziele:
- Automatisierte bewässerung der Balkonpflanzen
- Speichern von Sensordaten & cool graphs
- Überwachung & Steuerung von unterwegs

## Beschreibung:
Schaltungsaufbau grob übersichtlich im beigefügten Systemdiagramm.png als Blockschaltbild. Die Daten werden via ESP01 über MQTT an den RaspberryPi gesendet, dort gespeichert und dank Grafana als schöne Diagramme dargestellt.
Automatisiert bewässert wird momentan 2 mal Täglich morgends und abends, theoretisch sind bis zu 6 Ventile und 2 Pumpen schalt und steuerbar (erweiterbar). Mit einem MQTT messaging client ist möglich in die bewässerung einzugreifen und gespeicherte werte zu verändern sowie die Messdaten über Grafana im auge zu behalten.

## Details:
### Kommunikationn:
### I2C:
I²C (Inter-Integrated Circuit) ein Serieller Datenbus der von Philips Semiconductors entwickelt wurde und hauptsächlich für sehr kurze strecken (gerätintern) vorgesehen ist. <br>

Im fall dieses Projektes spielt die Kommunikationn der beiden Mikrocontroller eine tragende Rolle. Grundsätzlich ist ein Multi-master betrieb möglich wenn auch oft nicht empfohlen. Für die fehlerfreie kommunikation in beide Richtungen müssen leider beide controller als Master dem Bus beitreten. Hauptsächlich als Master verwende ich den Arduino Nano der dem ESP-01 signalisiert wann er als Master die Kommunikationn übernehmen darf um komplikationen zu vermeiden. In den weiteren Iterrationen des Programms möchte ich diese Eigenschaft noch mehr besser herausarbeiten.

### MQTT:
(beschreibung ergänzen)
(alle relevanten Topics sind auf den controllern zu finden hier folgt eine auflistung:)

## Code Arduino nano:

## Code ESP8266-01:
### Allgemein:
<<<<<<< Updated upstream
Dieser $\mu C$ wwird verwendet um dem System Zugang zum Lokalen Netzwerk zu verschaffen. Die Kommunikation erfolgt Seriel mit i2c. Den ESP-01 als slave des Nanos zu verwenden bringt einige Probleme mit sich. Die CPU frequenz des ESP-01 muss auf $160MHz$ statt den standardmäßigen $80MHz$ angehoben werden und am Nano muss die Frequenz des Buses reduziert werden. Damit auch der ESP-01 dem Arduino zuverläßig Daten senden kann wird der Bus mit 2 Master also Multimasterbetrieb betrieben. <br>Nach all diesen Anpassungen funktioniert die Kommunikation sehr stabil, sehr selten kommt es zu kleinen Fehlern in der Übertragung deshalb wird am Schluss einer Übertragung als letztes Byte eine festgelegte Zahl, die der Zuordnung und Kontrolle zum ausschluss von fehlern dient, gesendet.
=======
Dieser Mikrocontroller wwird verwendet um dem System Zugang zum Lokalen Netzwerk zu verschaffen. Die Kommunikation erfolgt Seriel mit i2c. Den ESP-01 als slave des Nanos zu verwenden bringt einige Probleme mit sich. Die CPU frequenz des ESP-01 muss auf *160MHz* statt den standardmäßigen *80MHz* angehoben werden und am Nano muss die Frequenz des Buses reduziert werden. Damit auch der ESP-01 dem Arduino zuverläßig Daten senden kann wird der Bus mit 2 Master also Multimasterbetrieb betrieben. <br>Nach all diesen Anpassungen funktioniert die Kommunikation sehr stabil, sehr selten kommt es zu kleinen Fehlern in der Übertragung deshalb wird am Schluss einer Übertragung als letztes Byte eine festgelegte Zahl, die der Zuordnung und Kontrolle zum ausschluss von fehlern dient, gesendet.
>>>>>>> Stashed changes
### Details: Programmierung mit Arduino IDE
Um die CPU frequenz des ESP beim programmieren mit der Arduino IDE zu ändern muss man beim Menüpunkt 'File' --> 'Preferences' und unter 'Additional Boards Manager URLs:' folgende ergänzt werden *https://dl.espressif.com/dl/package_esp32_index.json*, *http://arduino.esp8266.com/stable/package_esp8266com_index.json* <br>
Nun kann man unter dem Menüpunkt 'Tools' --> 'Board' nach 'Generic ESP8266 Module' wählen. Nachdem die restlichen einstellugnen wie im aus dem Bild zu entnehmen vorgenommen wurden ist die IDE bereit für den Upload eventuell fehlende librarys müssen gegebenenfalls noch Installiert werden, dies ist einfach über den library manager möglich.
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
```

```
### Grafana:

### MQTT&Python:
