<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-AUX banner" width="100%">
</p>

# 🎛️ EH3D-NODE-BUNGARD-AUX

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | 🇩🇪 <b>Deutsch</b> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Ein kleiner Knoten am Bohrkopf einer Bungard-CCD-Maschine (ESP32-S3 mit Netzwerkanschluss per Kabel): er sendet das Kamerabild, misst mit einem Laser die Höhe des Tisches, liest Drehzahl und Temperatur der Spindel und öffnet einen Schutzausgang, wenn die Spindel zu schnell oder zu heiß ist.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Ehrlichkeitscheck - was heute tatsächlich läuft:** die Regeln des Knotens sind in C++17 geschrieben und auf einem Computer getestet: die Einstellungsdatei mit ihren sicheren Vorgaben, die Messung der Tischhöhe aus mehreren Laserwerten (Ausreißer werden verworfen, eine zu stark streuende Messung wird abgelehnt), die Drehzahl der Spindel aus den Impulsen eines Sensors, die Temperatur mit Warnstufe, kritischer Stufe und Hysterese, der Schutzausgang und die Wörter, die der Knoten im Netz sagt (der Status, die Messung, das Token). Die ESP-IDF-Firmware (Kabelnetz, Webserver, Kamera, Spindelimpulse, Schutzausgang) ist geschrieben, aber nicht gebaut, geflasht oder auf einer Platine ausgeführt; **die Treiber des Lasersensors und der Temperatursensoren sind noch nicht geschrieben**, daher wird eine Messung heute abgelehnt, und nichts wurde an eine Maschine angeschlossen.

---

> **Safety- und Security-Status:** dies ist kein Sicherheitssystem (Safety). Der Schutzausgang ist ein Signal für die Stopp-Leitung der Steuereinheit, kein Ersatz dafür: der Stopp, der Menschen und Maschine schützt, ist ein Hardware-Taster, der den Antrieb von selbst abschaltet. Der Ausgang öffnet, wenn die Spindel zu schnell oder zu heiß ist, wenn ein eingeschalteter Temperatursensor nicht antwortet und wenn dieser Knoten nicht läuft; ein Knoten ohne Token lehnt jede Anfrage ab, die misst oder die Kamera zeigt. Verlasse dich nicht darauf, bis die ganze Kette an einer echten Maschine erprobt ist. Das Projekt ist unabhängig: es ist nicht von Bungard Elektronik hergestellt, empfohlen oder unterstützt, und Produktnamen erscheinen nur, um zu sagen, mit welchen Maschinen es arbeiten soll.

## 📋 Auf einen Blick

| | |
|---|---|
| Version | `0.0.1` |
| Reifegrad | Gerüst |
| Tests | 1 Testprogramm auf dem Rechner (74 Prüfungen) |
| Technologie-Stack | C++17, ESP-IDF 5.5, ESP32-S3, Netzwerk W5500 |
| Auf echter Hardware | Nein: die Firmware wurde nicht gebaut oder geflasht, und nichts wurde an eine Maschine angeschlossen |

**Springe zu:** [Überblick](#-überblick) · [Architektur](#-architektur) · [Schnellstart](#-schnellstart) · [Tests](#-tests) · [Struktur](#-struktur-des-repositorys) · [Verwandte](#-verwandte-projekte)

## 🚦 Projektstatus
Jede Zeile ist ein Teil dieses Repositorys, und die Ampel sagt, wie weit er heute ist. Sie beruht auf dem, was das Repository zeigen kann (Tests, Builds, Läufe), nicht auf dem, was geplant ist.

🟢 Fertig und überprüft · 🟡 Funktioniert, mit bekannten Grenzen · 🟠 Teilweise fertig oder noch nicht überprüft · 🔴 Nicht gemacht oder funktioniert nicht

| Bereich | Stand | Was das hier bedeutet |
|---|---|---|
| Software | 🟠 | Das ESP-IDF-Projekt (Kabelnetz mit einem W5500, Webserver, Kamera, Spindelimpulse, Schutzausgang, die Einstellungsdatei in ihrer eigenen Partition) ist geschrieben; es wurde nicht gebaut oder geflasht. |
| Tests | 🟢 | 1 Testprogramm auf dem Rechner besteht (`ctest`): 74 Prüfungen der Einstellungsdatei, der Lasermessung, der Drehzahl und Temperatur der Spindel, des Schutzausgangs und der Wörter des Netzes. |
| Laser (Tischhöhe) | 🟡 | Die Messung aus mehreren Werten (Median, Ausreißer verworfen, Streuung geprüft, Bereich und Versatz) ist getestet; der Treiber des Sensors ist nicht geschrieben, daher ist heute keine Messung möglich. |
| Spindel | 🟡 | Drehzahl aus den Impulsen eines Sensors (beliebig viele pro Umdrehung, Stillstand wenn die Impulse ausbleiben), Temperatur mit Warnung, kritischer Stufe und Hysterese und der Schutzausgang sind getestet; die Treiber der Temperatursensoren sind nicht geschrieben. |
| Kamera | 🟠 | Der MJPEG-Strom ist für den Kameraanschluss der Platine geschrieben und braucht die Pins der Platine in menuconfig; er ist nicht gelaufen. |
| Echte Hardware | 🔴 | Keine Platine hat diese Firmware ausgeführt, und nichts wurde an eine Maschine angeschlossen. |
| Dokumentation | 🟢 | README in acht Sprachen, die Einstellungsreferenz und die Hardware-Notizen. |

## 🎯 Überblick

* Die **Lasermessung**: der Knoten fragt den Sensor nach mehreren Werten, verwirft die Ausreißer (Median und mittlere absolute Abweichung), lehnt eine Messung ab, die mehr als `laser_sigma_max_mm` streut oder außerhalb des Bereichs liegt, und antwortet mit dem Abstand zum Tisch und der Streuung; die Desktop-Programme machen daraus einen Punkt der Tischkarte (`Z x y Höhe`, gelesen von `eh3d-levelling`).
* Die **Spindel**: die Drehzahl kommt aus den Impulsen eines Sensors (Hall-Sensor oder Schlitzscheibe, beliebig viele Impulse pro Umdrehung) und ist null, wenn die Impulse ausbleiben; die Temperatur hat eine Warnstufe, eine kritische Stufe und eine Hysterese, damit der Zustand nicht flackert; ein eingeschalteter Sensor, der nicht antwortet, gilt als heiß.
* Der **Schutzausgang**: ein GPIO, der low ist, sobald die Spindel stoppen muss (zu schnell, zu heiß, ein stummer Sensor) und auch solange der Knoten nicht läuft, zum Anschluss an die Stopp-Leitung der Steuereinheit über ein Relais oder einen Optokoppler; er lässt sich in den Einstellungen abschalten.
* Die **Kamera und das Netz**: das Kamerabild als MJPEG auf `/stream` und die Messwerte auf `/api/status` über einen Kabel-Netzwerkanschluss (W5500 über SPI); das Token schützt die Messung und die Kamera.
* Die **Einstellungsdatei** `head.cfg`: jeder abgestimmte Wert (Netz, Kamera, Laser, Spindel, Temperatur, Schutzausgang) ist eine Zeile einer Datei in ihrer eigenen Partition, nie eine Zahl im Code; ein Wert außerhalb des Bereichs wird gemeldet und der Standard bleibt.

## 🧩 Rolle in EH3D

EH3D-NODE-BUNGARD-AUX ist der Knoten am Bohrkopf einer Bungard-CCD-Maschine, neben EH3D-NODE-BUNGARD (dem Knoten, der die Maschine antreibt) und EH3D-NODE-BUNGARD-HMI (dem Bildschirm). Er sitzt am Kopf, sieht die Platine mit seiner Kamera, misst den Tisch unter dem Kopf und überwacht die Spindel. Die Desktop-Programme (EH3D-BUNGARD-GUI, DRILLPRO, ROUTEPRO) nutzen seine Messungen, um die Tischkarte zu bauen (`eh3d-levelling` in EH3D-COMMON) und das Z der Aufträge zu korrigieren; die Verbindung wird direkt oder über EH3D-SERVER laufen, und beides gibt es noch nicht.

## 🏛️ Architektur

<p align="center">
  <img src="images/architecture.svg" alt="Architektur von EH3D-NODE-BUNGARD-AUX: wer es nutzt, was es enthält und wovon es abhängt" width="100%">
</p>

Wie die Teile dieses Repositorys zusammenpassen: wer mit ihm spricht, was es enthält und wovon es abhängt. Das Bild wird aus `docs/architecture.json` gezeichnet und gegen den Code geprüft.

## 🖼️ Screenshots

<p align="center">
  <img src="images/screenshots/answers.png" alt="was der Knoten antwortet" width="100%">
</p>

<p align="center"><em>Was der Knoten im Netz antwortet: der Status (`GET /api/status`), eine Messung des Tisches (`POST /api/measure`) und die Zeile, die die Desktop-Programme als Punkt der Karte lesen. Eine Zeichnung der Formate, die die Tests prüfen, keine Aufnahme von einer Platine.</em></p>

## ✨ Funktionen

* **Eine Messung, der man trauen kann, oder keine:** mehrere Werte, Ausreißer verworfen, und eine Ablehnung (mit Grund), wenn die Werte zu stark streuen, außerhalb des Bereichs liegen oder zu wenige sind.
* **Eine ständig überwachte Spindel:** Drehzahl aus den Impulsen, Temperatur mit Warnstufe, kritischer Stufe und Hysterese, und ein Schutzausgang, der von selbst öffnet, wenn etwas nicht stimmt oder der Knoten nicht läuft.
* **Video vom Kopf:** die Kamera als MJPEG, hinter dem Token.
* **Standardmäßig sicher:** ohne Token keine Messung und keine Kamera; jede Grenze hat einen Schalter in den Einstellungen und beginnt in der konservativen Stellung.

## 🔒 Sicherheit und Schutz

| Bedrohung | Abwehr |
|---|---|
| Eine zu schnelle oder zu heiße Spindel | der Schutzausgang öffnet über `rpm_max` und bei der kritischen Temperatur, mit Hysterese gegen Flackern; ein eingeschalteter, stummer Temperatursensor gilt als heiß |
| Der Knoten verliert die Spannung oder hängt | der Ausgang ist low, bis der Knoten die Spindel geprüft hat, und bleibt low, solange er nicht läuft; er muss also als Stopp verdrahtet werden, der von selbst öffnet |
| Jemand im Netz misst oder beobachtet die Werkstatt | ohne Token lehnt der Knoten Messung und Kamera ab; das Token wird in konstanter Zeit verglichen und nie angezeigt oder protokolliert |
| Eine schlechte Messung gilt als gut | Ausreißer werden verworfen; eine Streuung über `laser_sigma_max_mm`, der Bereich oder zu wenige Werte lehnen die Messung mit Grund ab |
| Ein Tippfehler in den Einstellungen | ein Wert außerhalb des Bereichs oder ein unbekannter Schlüssel wird gemeldet und der sichere Standard bleibt; eine kritische Temperatur unter der Warnung oder ein umgekehrter Bereich fällt auf die Standardwerte zurück |
| Ein Geheimnis in der Firmware oder im Repository | nichts Geheimes wird einkompiliert; das Token ist eine Einstellung in seiner Partition |

**Was nicht abgewehrt wird:** der Knoten ist kein Sicherheitsgerät. Der Schutzausgang ist nur so gut wie die Verdrahtung zur Stopp-Leitung, und nichts davon wurde an einer Maschine erprobt. Der Netzwerkanschluss ist nicht verschlüsselt (ein einfacher HTTP-Port im Werkstattnetz): halte ihn in einem eigenen Netz.

## 🔌 Schnittstellen

| Schnittstelle | Was es ist | Wo |
|---|---|---|
| Einstellungsdatei | `head.cfg` in der Partition `config`: Zeilen `Schlüssel = Wert`; siehe `head.cfg.example` | `core/aux_config.hpp` |
| Status | `GET /api/status`: der Knoten, die Kamera, der Laser, die Drehzahl, die Temperatur, der Schutzausgang und die Laufzeit als eine JSON-Zeile | `core/protocol.hpp` |
| Messung | `POST /api/measure[?samples=N]` mit Token: der Abstand zum Tisch, die Streuung und warum eine Messung abgelehnt wurde | `core/laser.hpp`, `main/app_main.cpp` |
| Kamera | `GET /stream` mit Token: MJPEG | `main/app_main.cpp` |
| Spindelimpulse | ein GPIO (`AUX_RPM_GPIO` in menuconfig), steigende Flanken | `main/sensors.cpp` |
| Schutzausgang | ein GPIO (`AUX_PROTECT_GPIO`), low, wenn die Spindel stoppen muss | `main/sensors.cpp` |
| Laser und Temperatur | I2C für den Laser und One-Wire für den Temperatursensor; die Treiber sind nicht geschrieben | `main/sensors.cpp` |

## ⚙️ Konfiguration

Alles Abgestimmte ist eine Zeile von `head.cfg` (kopiere `head.cfg.example` in die Partition `config`); ein Wert außerhalb des Bereichs wird im Protokoll gemeldet und der Standard bleibt. Die wichtigsten:

| Einstellung | Standard | Bereich | Was sie tut |
|---|---|---|---|
| `token` | none | 8 - 64 | das Zugriffstoken; ohne es lehnt der Knoten Messung und Kamera ab |
| `laser_samples` | `7` | 1 - 31 | Werte einer Messung (der Median der guten) |
| `laser_offset_mm` | `0` | -50 - 50 | wird jeder Distanz zugerechnet: wo der Sensor über der Werkzeugspitze sitzt |
| `laser_sigma_max_mm` | `0.15` | 0.01 - 5 | eine Messung, deren Werte stärker streuen, wird abgelehnt |
| `rpm_pulses_per_rev` | `1` | 1 - 64 | Impulse des Sensors je Umdrehung der Spindel |
| `rpm_max` | `65000` | 1000 - 200000 | über dieser Drehzahl öffnet der Schutzausgang |
| `temp_sensor` | none | none ds18b20 ntc mlx90614 | der Sensor für die Spindeltemperatur |
| `temp_warn_c / temp_critical_c` | `55 / 70` | 20 - 200 | die Warnstufe und die Stufe, bei der der Schutzausgang öffnet |
| `protect_output` | `on` | on / off | der Schutzausgang arbeitet; nur auf dem Prüfstand ausschalten |

## 🚀 Schnellstart

Du brauchst einen C++17-Compiler und CMake für die Tests auf dem Rechner und ESP-IDF 5.5.5 für die Firmware (das Ziel ESP32-S3).

```bash
build.bat            # die Tests auf dem Rechner (Windows)  -  ./build.sh unter Linux
python -I tools/ci_validate.py
```

Die Firmware wurde noch nicht gebaut: wenn es so weit ist, erzeugt `idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` das Image (die Pins deiner Platine unter `EH3D Bungard AUX` setzen), und eine Partition namens `config` enthält `head.cfg`.

## 🧪 Tests

Die Tests auf dem Rechner (`build.bat` oder `./build.sh`, dann `ctest`) sind ein Programm und alles besteht: `test_aux` (74 Prüfungen) zur Einstellungsdatei (Standardwerte, Werte, falsche Werte, Grenzen, die zusammen keinen Sinn ergeben), zur Lasermessung (der Median, der verworfene Ausreißer, der Versatz, als schlecht markierte Werte, der Bereich, keine Werte, eine zu große Streuung), zur Spindeldrehzahl (Impulse je Umdrehung, eine langsamer werdende Spindel, Stillstand bei ausbleibenden Impulsen), zur Temperatur (die Hysterese, ein ausfallender Sensor), zum Schutzausgang und zu den Wörtern des Netzes (die Anfragezeile, die Routen, das Token im Header oder in der Abfrage, die Zahl der Werte, das JSON von Status und Messung, die Zeile eines Kartenpunkts). Die Firmware ist nicht abgedeckt: sie wurde nicht gebaut.

## 🏗️ Build und Veröffentlichung

Die Version steht in `eh3d.project.json`, und jede Version hat ihren Eintrag in [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` bauen und führen die Tests auf dem Rechner aus, `test.sh` / `test.bat` die Prüfungen des Repositorys. Die Firmware wird mit ESP-IDF 5.5.5 gebaut (`idf.py build`), was noch nicht geschehen ist; es wurde noch kein Release veröffentlicht.

## 🩺 Fehlerbehebung

| Symptom | Was tun |
|---|---|
| Eine Messung wird mit `no_readings` abgelehnt | Der Treiber des Lasers ist in dieser Version nicht geschrieben, daher kommt nie ein Wert an; der Rest der Antwort ist getestet. |
| Eine Messung wird mit `too_noisy` abgelehnt | Die Werte streuen mehr als `laser_sigma_max_mm`: lass Kopf und Tisch zur Ruhe kommen oder erhöhe die Grenze etwas; prüfe, dass nichts wackelt. |
| Der Knoten antwortet 401 auf eine Messung oder die Kamera | Setze `token` in `head.cfg` (8 bis 64 Zeichen) und sende es im Header `Authorization: Bearer ...` oder in `?token=`. |
| Der Schutzausgang ist immer offen | Ein eingeschalteter Temperatursensor, der nicht antwortet, gilt als heiß, und die Treiber sind noch nicht geschrieben: setze `temp_sensor = none` auf dem Prüfstand oder schließe einen Sensor an, sobald sein Treiber existiert. |

## 📂 Struktur des Repositorys

<p align="center">
  <img src="images/repository-structure.svg" alt="Struktur des Repositorys EH3D-NODE-BUNGARD-AUX" width="100%">
</p>

<details>
<summary>Textversion des Baums</summary>

```text
EH3D-NODE-BUNGARD-AUX/
├── core/
│   ├── aux_config.hpp                           die Einstellungsdatei und ihre sicheren Standardwerte
│   ├── laser.hpp                                die Messung der Tischhöhe
│   ├── spindle.hpp                              Drehzahl, Wärme und Schutzausgang
│   └── protocol.hpp                             die Wörter des Netzes: Routen, Token, JSON
├── main/
│   ├── app_main.cpp                             Start, Einstellungen, Netz, Webserver, Kamera
│   ├── sensors.cpp sensors.hpp                  die Pins: Impulse, Schutzausgang, Sensoren
│   └── idf_component.yml Kconfig.projbuild      Komponentenliste und menuconfig-Pins
├── tests/                                       Host-Tests von Einstellungen, Tor, Sprache und Bildschirmtexten
├── docs/                                        die Hardware-Hinweise und die Quellen der beiden Bilder
├── tools/                                       CI-Prüfung, README-Prüfung und Bildwerkzeuge
├── CMakeLists.txt                               das ESP-IDF-Projekt
├── partitions.csv                               die Flash-Aufteilung mit zwei Slots
├── head.cfg.example                             jede Einstellung mit Standardwert und Bereich
├── sdkconfig.defaults                           ESP-IDF-Einstellungen für das ESP32-P4-Modul
├── images/                                      Banner und die Bilder von Architektur und Repository-Struktur
├── .github/                                     CI- und Release-Workflows, Vorlagen für Issues und Pull Requests
├── build.sh build.bat                           baut und führt die Host-Tests in einem Live-Panel aus
├── eh3d.project.json                            Manifest: Version, Rolle, Reifegrad und Build-Befehl
├── CHANGELOG.md                                 was sich in jeder Version geändert hat
├── SECURITY.md SUPPORT.md                       Schwachstellen melden und Fragen stellen
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   Mitwirken, Verhaltenskodex und Lizenz
└── bump_manifest_version.py                     Versionswerkzeug des Manifests
```

</details>

## 🗺️ Roadmap und bekannte Grenzen

**Bekannte Grenzen heute**

* Nichts lief auf einer Platine: die Firmware wurde nicht gebaut, geflasht oder erprobt, und die Pins der Kamera und des Netzwerkchips müssen mit dem Schaltplan der Platine geprüft werden.
* Der Treiber des Lasersensors (VL53L1X über I2C) und die der Temperatursensoren sind nicht geschrieben, daher ist keine Messung möglich, und ein eingeschalteter Temperatursensor gilt als heiß.
* Die Verbindung zu den Desktop-Programmen und zu EH3D-SERVER gibt es noch nicht: der Knoten antwortet an seinem Port, niemand fragt ihn.
* Der Netzwerkport ist einfaches HTTP: halte ihn in einem eigenen Netz.

**Als Nächstes**

* Die Treiber des Lasers und der Temperatursensoren schreiben und die Firmware bauen.
* Die Verbindung zu EH3D-BUNGARD-GUI, zu EH3D-SERVER und zum Bildschirm sowie die Kameraansicht hinzufügen.
* Den Schutzausgang an einer Stopp-Leitung einer Maschine erproben.

## 🌍 Sprachen

README und Texte gibt es auf Englisch, Spanisch, Französisch, Italienisch, Deutsch, Chinesisch, Japanisch und europäischem Portugiesisch.

## 🔗 Verwandte Projekte

* [EH3D-BUNGARD-GUI](https://github.com/JuanenRac/EH3D-BUNGARD-GUI): die moderne Oberfläche, die die Messungen für die Tischkarte nutzt
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): der Knoten an der seriellen Schnittstelle der Steuereinheit, der die Maschine antreibt
* [EH3D-NODE-BUNGARD-HMI](https://github.com/JuanenRac/EH3D-NODE-BUNGARD-HMI): der Touchscreen des Bedieners
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): das Zentrum der Familie: er wird die Antworten des Knotens zu jedem Client tragen
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): die gemeinsamen Teile der Familie, darunter `eh3d-levelling` (die Höhenkarte des Tisches)

## 📚 Dokumentation

* Die Einstellungen: `head.cfg.example` listet jeden Schlüssel mit Standardwert und Bereich.
* Die Hardware: die Pins, die Verdrahtung des Schutzausgangs und die Teile, in `docs/HARDWARE.md`.
* [Änderungsprotokoll](CHANGELOG.md)

## 🤝 Mitwirken und Community

* Führe vor einem Pull Request die Host-Tests (`./build.sh` oder `build.bat`) und `python -I tools/ci_validate.py` aus: alles muss bestehen. Siehe [CONTRIBUTING.md](CONTRIBUTING.md) und den [Verhaltenskodex](CODE_OF_CONDUCT.md).
* Das README gibt es in acht Sprachen: Eine Textänderung wird in allen acht Dateien gemacht, und die beiden Bilder werden aus `docs/architecture.json` und `docs/structure.json` mit `tools/make_architecture.py` und `tools/make_structure.py` gezeichnet.
* Nichts, was ändert, wie der Knoten eine Messung meldet oder den Schutzausgang öffnet, ändert sich ohne einen Test, der es abdeckt, und eine Notiz über das an Hardware Beobachtete.
* Eine Schwachstelle bitte privat melden: [SECURITY.md](SECURITY.md). Fragen, Ideen und Meldungen: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LIZENZ

Software: GPL-3.0-or-later. Siehe [LICENSE](LICENSE). Die Hardware-Entwürfe der Suite verwenden die CERN OHL-S.
