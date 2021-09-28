# Bewae - Bewässerungsprojekt v3.1
Momentan ist diese Version eher als Testversion zu sehen. Neu hinzugefügt und getestet wird nun die steuerbarkeit über MQTT.

In den weiteren Ordnern befinden sich der Code für beide Controller sowie das json export für das Grafana dashboard. Die Konfiguration des Raspberrypi fehlt noch, die verwendete Datenbank ist Influxdb. (Die Scripts befinden sich am Pi im ordner py_scripts und werden automatisiert gestartet mit cron als su)

## Systemdiagramm
![System](/Systemdiagramm.png "Systemdiagramm")

## Mainboard (bew_entwurf_v2_7)
**bew_entwurf_v2_7.fzz** Hauptplatine
**Platine_01.fzz** als Erweiterung mit esp01, Buckconverter etc.
Die Schaltungen wurden mit fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen.
![Main PCB](/bewae_v3_1.png "Main board")

## Ziel:
Automatisierte bewässerung der Balkonpflanzen. Zusätzlich speichern von Sensordaten.

## Beschreibung:
Schaltungsaufbau grob übersichtlich im beigefügten Systemdiagramm.png als Blockschaltbild. Die Daten werden via ESP01 über MQTT an den RaspberryPi gesendet, dort gespeichert und dank Grafana als schöne Diagramme dargestellt.
 Automatisiert bewässert wird 2 mal Täglich morgends und abends, momentan sind theoretisch bis zu 6 kreisläufe und 2 Pumpen schalt und steuerbar. Mit einem MQTT messaging client ist möglich in die bewässerung einzugreifen und gespeicherte werte zu verändern, wobei die bewässerungszeit immer in sekunden gerechnet wird.

## Details:
### Kommunikationn:
### I2C:
(Beschreibung iic ergänzen)
Im fall dieses Projektes spielt die Kommunikationn der beiden Mikrocontroller eine tragende Rolle. Grundsätzlich ist ein Multi-master betrieb möglich wenn auch oft nicht empfohlen. Für die fehlerfreie kommunikation müssen leider beide controller als Master dem Bus beitreten. Hauptsächlich als Master verwende ich den Arduino Nano der dem ESP-01 signalisiert wann er als Master die Kommunikationn übernehmen darf. In den weiteren Iterrationen des Programms möchte ich diese Eigenschaft noch mehr besser herausarbeiten.

### MQTT:
(beschreibung ergänzen)
(alle relevanten Topics sind auf den controllern zu finden hier folgt eine auflistung:)

## MAIN Code (Arduino nano):

## Code (ESP-01):
