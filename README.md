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

## Steuerung des Systems

### config.JSON

In dieser Datei befinden sich alle relevanten Einstellungen. Auf dem Controller wird dieses File im Flash hinterlegt. Um auf Änderungen einfacher reagieren zu können kann mit Node-Red (auf einem RaspberryPi) innerhalb des Netzwerks diese Datei zur Verfügung gestellt werden. Hierfür muss die Adresse in der connection.h angepasst werden. <br>

Hinweis: Es ist auch möglich ohne verbindung zu einem RaspberryPi dieses File zu verwenden allerdings muss es für Änderungen dann wieder neu aufgespielt werden.

### Steuerung der Bewässerung

Die Bewässerung wird mit den Einstellungen in der config.JSON-Datei gesteuert. Dabei werden 2 Variablen verwendet - timetable und water-time:

- 'timetable': Wird durch die ersten 24 Bit einer Integer-Zahl repräsentiert, wobei jedes Bit für eine Stunde des Tages steht. Um Platz zu sparen, wird es als Long Integer abgespeichert.
- 'water-time': Ist die Zeit (in Sekunden), die die jeweilige Gruppe bewässert wird. Hierbei können mehrere Ventile oder Ventil + Pumpe gleichzeitig gesetzt werden.

<br>

```cpp
// Beispiel-Zeitplan                           23211917151311 9 7 5 3 1
//                                              | | | | | | | | | | | |
unsigned long int timetable_default = 0b00000000000100000000010000000000;
//                                               | | | | | | | | | | | |
// Dezimale Darstellung: 1049600                22201816141210 8 6 4 2 0
//
```

Die Steuerung selbst kann auf folgenden Arten erfolgen:

- Config-Datei: Entweder lokal im Flash (SPIFFS), oder über Node-red bereitgestellt
- (currently not implemented) MQTT über W-LAN (App oder Pi) (noch nicht wieder implementiert)

<br>

Im file kann die bewässrung wie im Beispiel unten konfiguriert werden:

```json
// JSON group template
{"group": {
    "Tomatoes": {         // name of grp
    "is_set":1,           // indicates status
    "vpins":[0,5],        // list of pins bound to group
    "water-time":10,      // water time in seconds
    "timetable":1049600,  // timetable (first 24 bit)
    "lastup":[0,0],       // runtime variable - dont touch!
    "watering":0          // runtime variable - dont touch!
    },
    .
    .
    .
  }
}
```

### Konfiguration der Sensoren:
<br>
Die Sensoren können mithilfe des 'config.JSON' zu jeder Zeit verändert werden. Dabei sind einige Messfunktionen mit gänigen Sensoren Implementiert. Es können auch noch Änderungen vorgenommen werden um zurückgegebenen Werte zu modifizieren oder auch relative Messwerte (%) ausgegeben werden.
<br>
Beispielkonfiguration:

```json
  "sensor": {
    "id00": {
      "name": "bme280",
      "field": "temp",
      "mode": "bmetemp"
    },
    .
    .
    .
    "id03": {
      "name": "Soil",
      "field": "temp",
      "mode": "soiltemp"
    },
    .
    .
    .
    "id08": {
      "name": "Soil",
      "field": "moisture",
      "mode": "vanalog",
      "vpin": 15,
      "hlim": 600,
      "llim": 250
    }
  },
  .
  .
  .
```

Konfiguration & Modifikatoren:
| Property     | Description        |
|--------------|--------------------|
| `add`        | Addieren einer konstanten zum Ergebnis, optional wird nur bei >0     |
| `fac`        | Multiplizieren einer konstanten zum Ergebnis, optional wird nur bei >0     |
| `hlim`       | Obergrenze für relative Messung (%), optional wird nur bei >0 berücksichtigt   |
| `llim`       | Untergrenze für relative Messung (%), optional wird nur bei >0 berücksichtigt    |
| `vpin`       | Definiert Virtuellen Pin als input, für erweiterte Messfunktionen notwendig    |
| `pin`        | Definiert Normalen analogen Pin als input    |
<br>

Messfunktionen: <br>
| Measuring Mode | Description            |
|----------------|------------------------|
| `analog`       | reads analog pin, requires  configured 'pin'       |
| `vanalog`      | reads virtual analog pin, requires  configured 'vpin'      |
| `bmetemp`      | reads temperature from bme280 Sensor on Default address      |
| `bmehum`       | reads humidity from bme280 Sensor on Default address       |
| `bmepress`     | reads pressure from bme280 Sensor on Default address`     |
| `soiltemp`     | reads temperature from ds18b20 sensor from default 1 wire pin     |

<br>

# SETUP

### Installation des codes:

[Esp32 Code](./code/esp_32ard_bewae/) <br>
Stellen Sie zunächst sicher, dass PlatformIO in Visual Studio Code installiert ist. Öffnen Sie dann den Projektordner in VS Code. Schließen Sie als Nächstes Ihren ESP-32-Controller an und klicken Sie auf das Pfeilsymbol in der PlatformIO-Taskleiste, um Ihren Code hochzuladen. Das Gerät sollte automatisch erkannt werden, es kann nötig sein das die Boot Taste während des Upload vorgangs zu drücken. Weiters ist es wichtig das 'Filesystem' hochzuladen, dies kann mit dem unterpunkt 'Upload Filesystem Image' von Platformio gemacht werden. <br>

Hinweise:
- Verbindungseinstellungen aktualisieren! Relevante Daten in der Datei 'connection.h' eintragen <br>
- Verbindung und Funktion weiterer Bauteile & Sensoren Prüfen <br>
- RTC-Modul Uhrzeit ändern <br>

#### ändern der Zeit am RTC Modul:

Um die korrekte Uhrzeit am RTC-Modul einzustellen, kann man entweder eines der Codebeispiele für das Modul oder die funktion 'setTime' verwenden. Diese muss nur einmal ausgeführt werden. Dafür habe ich im 'Setup' des 'main.cpp' die Nötige funktion als Kommentar hinzugefügt. (Wichtig: Dannach wieder Kommentieren und Programm erneut hochladen) <br>

## Raspberrypi

### Node-Red

TODO: complete documentation

### InfluxDB 2.0

TODO: complete documentation

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
