# Bewae - Bewässerungsprojekt v3.3

v3.3final versions are not stable (work in progress).

## About

Automatisiert Sensorgesteuerte Bewässerung mit Raspberry Pi & Arduino
<br>

**Ziele:**

+ Sensorgesteuerte Automatisierte Bewässerung der Balkonpflanzen
+ Speichern & Darstellung von Sensordaten
+ Überwachung & Steuerung von unterwegs
+ (Zukünftig) Abfrage von Wetterdaten & Bewässerung mit Machine Learning

## Inhalt:

- [Introduction (EN)](/README_EN.md)
- [Einleitung (DE)](#einleitung-de)
- [Steuerung des Systems](#steuerung-des-systems)
- [Setup](#setup)
- [Details](#details)
- [Bilder & Entstehung](#bilder--entstehung)

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

## Steuerung des Systems

### config.JSON

In dieser Datei befinden sich alle relevanten Einstellungen. Auf dem Controller wird diese Datei im Flash hinterlegt. Um auf Änderungen einfacher reagieren zu können, kann mit Node-Red (auf einem RaspberryPi) innerhalb des Netzwerks diese Datei zur Verfügung gestellt werden. Hierfür muss die Adresse in der connection.h angepasst werden.

Hinweis: Es ist auch möglich, ohne Verbindung zu einem RaspberryPi diese Datei zu verwenden, allerdings muss sie für Änderungen dann wieder neu aufgespielt werden.

### Steuerung der Bewässerung

Die Bewässerung wird mit den Einstellungen in der config.JSON-Datei gesteuert. Dabei werden zwei Variablen verwendet - timetable und water-time:

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

Im File kann die Bewässerung wie im Beispiel unten konfiguriert werden:

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

Die Sensoren können mithilfe des ‘config.JSON’ zu jeder Zeit verändert werden. Dabei sind einige Messfunktionen mit gängigen Sensoren implementiert. Es können auch noch Änderungen vorgenommen werden, um zurückgegebenen Werte zu modifizieren oder auch relative Messwerte (%) ausgegeben werden. <br>

Um einen Sensor korrekt zu konfigurieren ist es Notwendig eine Einzigartige ID als Json Key zu verwenden. Unter Diesem Key müssen noch 'name', 'field' und 'mode' vergeben werden. 'name' und 'field' entspricht dabei dem Eintrag in der DB unter dem der Messwert in InfluxDB zu finden ist. Implementierte 'mode' varianten können unten in der Tabelle entnommen werden, es kann erforderlich sein das noch weitere Dinge wie 'pin' etc. vorhanden sind.

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

<br>


Konfiguration & Modifikatoren:
| Property     | Description        |
|--------------|--------------------|
| `vpin`       | Definiert Virtuellen Pin als input, für erweiterte Messfunktionen notwendig    |
| `pin`        | Definiert Normalen analogen Pin als input    |
| `add`        | (OPTIONAL) Addieren einer konstanten zum Ergebnis, optional wird nur bei >0     |
| `fac`        | (OPTIONAL) Multiplizieren einer konstanten zum Ergebnis, optional wird nur bei >0     |
| `hlim`       | (OPTIONAL) Obergrenze für relative Messung (%), optional wird nur bei >0 berücksichtigt   |
| `llim`       | (OPTIONAL) Untergrenze für relative Messung (%), optional wird nur bei >0 berücksichtigt    |

<br>

Messfunktionen: <br>
| Measuring Mode | Description            | Requirements           |
|----------------|------------------------|------------------------|
| `analog`       | reads analog pin       | configured 'pin' in config.JSON      |
| `vanalog`      | reads virtual analog pin | configured 'vpin' in config.JSON    |
| `bmetemp`      | reads temperature from bme280 Sensor on Default address      | Connected BME280 |
| `bmehum`       | reads humidity from bme280 Sensor on Default address       | Connected BME280 |
| `bmepress`     | reads pressure from bme280 Sensor on Default address`     | Connected BME280 |
| `soiltemp`     | reads temperature from ds18b20 sensor from default 1 wire pin     | Board need to have 1wire pin & connected Sensor |

<br>

## Setup Guide

### Mikrokontroller ESP32:

[Esp32 Code](./code/esp_32ard_bewae/) <br>

#### PlatformIO IDE

Stellen Sie zunächst sicher, dass PlatformIO in Visual Studio Code installiert ist. Dafür drücken Sie das 'Extensions' Icon in der linken Taskleiste und installieren 'PlatformIO IDE'. <br>

Öffnen Sie dann den Projektordner 'bewae' in VS Code. Schließen Sie als Nächstes Ihren ESP-32-Controller an. Zum Hochladen des Codes auf den ESP32 klicken Sie auf das Pfeilsymbol in der PlatformIO-Taskleiste. Das Gerät sollte automatisch erkannt werden, es kann nötig sein, die Boot-Taste während des Upload-Vorgangs zu drücken. Weiterhin ist es wichtig, das 'Filesystem' hochzuladen, dies kann mit dem Unterpunkt 'Upload Filesystem Image' der PlatformIO-Erweiterung gemacht werden. <br>

#### Config & WIFI (Optional)

Nun ist es wichtig, in den Dateien 'connection.h' seine Zugangsdaten für das lokale Netzwerk einzutragen. <br>
Ebenfalls wären die IP des Systems worauf Node-red und die Datenbank installiert zu ändern. Sowie die dazugehörigen Daten. <br>
Weiters könnte noch die Adresse des Zeitservers zu ändern sein.  <br>

Für den Fall, dass nichts eingetragen wurde, sollte das Programm trotzdem funktionieren. Die Anpassung der Einstellungen ist dann allerdings schwieriger, da jedes Mal bei Änderungen das FileSystem neu hochgeladen werden muss. Außerdem muss die Zeit dann ebenfalls manuell gesetzt werden! Hierzu gibt es in der Helferklasse eine Funktion 'set_time'. <br>

Hinweise:
Vergessen Sie nicht auf die Konfiguration der Hardware. Dazu passen Sie in 'Helper.h' sowie 'main.cpp' die verwendete Helper-Klasse an das genutzte Hardware-Board an. <br>

### Raspberrypi

InfluxDB 2.0 benötigt ein 64-Bit-OS. Daher verwende ich einen Raspberry Pi 4B mit einem vollwertigen 64-Bit-Image.

#### Node-Red

How to [install](https://nodered.org/docs/getting-started/raspberrypi). <br>

Im Browser Your.Raspberry.Pi.IP:1880 Benutzer einrichten bzw. einloggen. Dann kann über das Web-Interface der [flow](/node-red-flows/flows.json) importiert werden. <br>
Öffnen Sie den Flow und klicken Sie doppelt auf den orangefarbenen 'read config'-Knoten. Ändern Sie den Pfad zu dem Pfad Ihrer Konfigurationsdatei.

#### InfluxDB 2.0

How to [install](https://docs.influxdata.com/influxdb/v2/install/?t=Raspberry+Pi). <br>
Influx kann auf zwei Arten aufgesetzt werden, entweder über das UI oder das CLI, Beschreibung dazu kann online gefunden werden (install link):

Set up InfluxDB through the UI
- With InfluxDB running, visit http://localhost:8086
- Click Get Started
- Set up your initial user
- Enter a Username for your initial user.
- Enter a Password and Confirm Password for your user.
- Enter your initial Organization Name.
- Enter your initial Bucket Name.
- Click Continue.

Die Berechtigungen für Buckets können recht einfach über das UI eingestellt werden und dazu dann ein Token generiert werden.

- In der Option 'Load Data' auf 'API TOKENS' klicken
- Klicken Sie auf 'Generate API TOKEN' -> Custom API Token
- Token-Beschreibung eingeben und Berechtigung (Write) wählen -> Auf 'Generate' klicken
- InfluxDB-Token nun in der 'connection.h'-Datei ersetzen

---

Nun sollte alles vorbereitet sein. Die Konfigurationsdatei ('config.JSON) kann nach Bedarf angepasst werden, dazu einfach die benötigten Funktionen (siehe Tabellen) wie im Beispiel gezeigt konfigurieren. <br>

Falls ein Relais-Modul verwendet wird mit invertierter Logik besteht die Option, die Ausgänge des Shift Registers zu invertieren, dazu in der 'config.h'-Datei INVERT_SHIFTOUT auf true setzen. <br>
Weiterhin können noch Flags gesetzt werden, die das Debugging erleichtern. <br>

## Details

### Platinen

Die Schaltungen wurden mit Fritzing erstellt, die Steckbrettansicht bietet gute Übersicht und eignet sich ideal für Prototypen. Ab einer gewissen Größe des Projekts ist Fritzing allerdings nicht mehr ideal. Gerber-Dateien sind vorhanden. Für die Fritzing-Dateien ist lediglich die Platinenansicht relevant. Alle Boards verwenden den ESP-32 als Mikrocontroller. <br>


 Board 1:                  | Board 3:                  | Board 5:
:-------------------------:|:-------------------------:|:-------------------------:
![Board1](/docs/pictures/bewae3_3_board1v3_838_Leiterplatte.png) | ![Board3](/docs/pictures/bewae3_3_board3v22_Leiterplatte.png) | ![Board5](/docs/pictures/bewae3_3_board5v5_final_Leiterplatte.png)
Main Board | Extention Board | Main Board

#### **bewae3_3_board1v3_838.fzz** Hauptplatine (PCB)

Große Hauptplatine. Platz für bis zu 16 analoge Sensoren sowie BME280- und RTC-Modul über I2C. 8 Pins für Ventile/Pumpen (erweiterbar). Diese können entweder über Relais oder dem PCB Board 3 einfach verwendet werden.<br>

#### **bewae3_3_board3v22.fzz** als Erweiterung (PCB)

Als Erweiterung gedacht. Hat den Zweck, Steckplätze für Sensoren und weitere Verbraucher bereitzustellen. Zur Erweiterung der 8 Pins der Hauptplatine kann das Schieberegister verwendet werden. Insgesamt 16 Steckplätze für Sensoren inklusive Versorgung, sowie 2 Pumpen (*12V*) und 10 kleinere Ventile (*12V*).<br>

#### **bewae3_3_board5v5_final.fzz** als Hauptplatine (PCB)

Kleinere Hauptplatine. Verzichtet auf eine große Anzahl an Sensoranschlüssen. Platz für 2x 5V Sensoren, 2x 3V Sensoren, 1x LDR, I2C-Bus, 1-Wire-Bus. BME280- und RTC-Modul sind ebenfalls vorgesehen.<br>

### aktueller Aufbau

- ESP32 Board 1 & 3; Esp32 Board 5
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
