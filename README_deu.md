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
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Ehrlichkeitscheck - was heute tatsächlich läuft:** die Regeln des Knotens sind in C++17 geschrieben und auf einem Computer getestet: die Einstellungsdatei mit ihren sicheren Vorgaben, die Messung der Tischhöhe aus mehreren Laserwerten (Ausreißer werden verworfen, eine zu stark streuende Messung wird abgelehnt), die Drehzahl der Spindel aus den Impulsen eines Sensors, die Temperatur mit Warnstufe, kritischer Stufe und Hysterese, der Schutzausgang und die Wörter, die der Knoten im Netz sagt (der Status, die Messung, das Token). Die ESP-IDF-Firmware (Kabelnetz, Webserver, Kamera, Spindelimpulse, Schutzausgang) ist geschrieben und lässt sich mit ESP-IDF 5.5.5 bauen, wurde aber nie geflasht oder auf einer Platine ausgeführt; **die Treiber des Lasers (VL53L1X), der Temperatursensoren (DS18B20, NTC, MLX90614) und der Kamera (OV2640 oder OV5640) sind geschrieben, aber nie auf einer Platine gelaufen**, daher wurde noch nie eine Messung gemacht, und nichts wurde an eine Maschine angeschlossen.

---

> **Safety- und Security-Status:** dies ist kein Sicherheitssystem (Safety). Der Schutzausgang ist ein Signal für die Stopp-Leitung der Steuereinheit, kein Ersatz dafür: der Stopp, der Menschen und Maschine schützt, ist ein Hardware-Taster, der den Antrieb von selbst abschaltet. Der Ausgang öffnet, wenn die Spindel zu schnell oder zu heiß ist, wenn ein eingeschalteter Temperatursensor nicht antwortet und wenn dieser Knoten nicht läuft; ein Knoten ohne Token lehnt jede Anfrage ab, die misst oder die Kamera zeigt. Verlasse dich nicht darauf, bis die ganze Kette an einer echten Maschine erprobt ist. Das Projekt ist unabhängig: es ist nicht von Bungard Elektronik hergestellt, empfohlen oder unterstützt, und Produktnamen erscheinen nur, um zu sagen, mit welchen Maschinen es arbeiten soll.

## 📋 Auf einen Blick

| | |
|---|---|
| Version | `0.0.2` |
| Reifegrad | Gerüst |
| Tests | 10 Testprogramme am Computer |
| Technologie-Stack | C++17, ESP-IDF 5.5, ESP32-S3, Netzwerk W5500 |
| Auf echter Hardware | Nein: die Firmware lässt sich bauen, wurde aber nicht geflasht, und nichts wurde an eine Maschine angeschlossen |

**Springe zu:** [Überblick](#-überblick) · [Architektur](#-architektur) · [Schnellstart](#-schnellstart) · [Tests](#-tests) · [Struktur](#-struktur-des-repositorys) · [Verwandte](#-verwandte-projekte)

## 🚦 Projektstatus
Jede Zeile ist ein Teil dieses Repositorys, und die Ampel sagt, wie weit er heute ist. Sie beruht auf dem, was das Repository zeigen kann (Tests, Builds, Läufe), nicht auf dem, was geplant ist.

🟢 Fertig und überprüft · 🟡 Funktioniert, mit bekannten Grenzen · 🟠 Teilweise fertig oder noch nicht überprüft · 🔴 Nicht gemacht oder funktioniert nicht

| Bereich | Stand | Was das hier bedeutet |
|---|---|---|
| Software | 🟠 | Das ESP-IDF-Projekt (Kabelnetz mit einem W5500, Webserver, Kamera, Spindelimpulse, Schutzausgang, die Einstellungsdatei in ihrer eigenen Partition) ist geschrieben und baut; es wurde nicht geflasht. |
| Tests | 🟢 | 10 Testprogramme laufen durch (`ctest`): die Einstellungen, die Messung des Lasers, Drehzahl und Temperatur der Spindel, der Schutzausgang, die Worte des Netzes, das Video (RTSP, ONVIF), das Einstellungsdokument des Kopfes, das Update von GitHub, die Uhr, was alle Knoten teilen, die Texte des Panels in acht Sprachen und die Stacks der Tasks. |
| Laser (Tischhöhe) | 🟡 | Die Messung aus mehreren Werten (Median, Ausreißer verworfen, Streuung geprüft, Bereich und Versatz) ist getestet; der Treiber des VL53L1X ist geschrieben, aber nie gelaufen, daher wurde noch keine Messung gemacht. |
| Spindel | 🟡 | Drehzahl aus den Impulsen eines Sensors (beliebig viele pro Umdrehung, Stillstand wenn die Impulse ausbleiben), Temperatur mit Warnung, kritischer Stufe und Hysterese und der Schutzausgang sind getestet; die Treiber des DS18B20, des NTC und des MLX90614 sind geschrieben, aber nie gelaufen. |
| Kamera | 🟠 | Der MJPEG-Strom ist für den Kameraanschluss der Platine geschrieben und braucht die Pins der Platine in menuconfig; er ist nicht gelaufen. |
| Video (RTSP, ONVIF) | 🟡 | Die RTSP-Sitzung (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, über UDP oder in der TCP-Verbindung, Digest- und Basic-Anmeldung), die RTP-Nutzlast von JPEG, die Erkennung und die SOAP-Dienste von ONVIF (Profile, Encoder mit ihren Größen, Adressen von Strom und Schnappschuss, das Benutzer-Token) sind auf einem Rechner getestet; kein Player und kein Rekorder hat das Video von einer echten Platine geholt. |
| Echte Hardware | 🔴 | Keine Platine hat diese Firmware ausgeführt, und nichts wurde an eine Maschine angeschlossen. |
| Dokumentation | 🟢 | README in acht Sprachen, die Einstellungsreferenz und die Hardware-Notizen. |

## 🎯 Überblick

* Die **Lasermessung**: der Knoten fragt den Sensor nach mehreren Werten, verwirft die Ausreißer (Median und mittlere absolute Abweichung), lehnt eine Messung ab, die mehr als `laser_sigma_max_mm` streut oder außerhalb des Bereichs liegt, und antwortet mit dem Abstand zum Tisch und der Streuung; die Desktop-Programme machen daraus einen Punkt der Tischkarte (`Z x y Höhe`, gelesen von `eh3d-levelling`).
* Die **Spindel**: die Drehzahl kommt aus den Impulsen eines Sensors (Hall-Sensor oder Schlitzscheibe, beliebig viele Impulse pro Umdrehung) und ist null, wenn die Impulse ausbleiben; die Temperatur hat eine Warnstufe, eine kritische Stufe und eine Hysterese, damit der Zustand nicht flackert; ein eingeschalteter Sensor, der nicht antwortet, gilt als heiß.
* Der **Schutzausgang**: ein GPIO, der low ist, sobald die Spindel stoppen muss (zu schnell, zu heiß, ein stummer Sensor) und auch solange der Knoten nicht läuft, zum Anschluss an die Stopp-Leitung der Steuereinheit über ein Relais oder einen Optokoppler; er lässt sich in den Einstellungen abschalten.
* Die **Kamera und das Netz**: das Kamerabild als MJPEG auf `/stream` und die Messwerte auf `/api/status` über einen Kabel-Netzwerkanschluss (W5500 über SPI); das Token schützt die Messung und die Kamera.
* Die **Einstellungen**: Jeder abgestimmte Wert (das Netzwerk, die Kamera, der Laser, die Spindel, die Temperatur, der Schutzausgang) ist eine Einstellung des Knotens, im Flash gespeichert und im Web-Panel bearbeitet, nie eine Zahl im Code; ein Wert außerhalb des Bereichs wird mit Begründung abgelehnt.
* Das **Video, wie es eine IP-Kamera sendet**: RTSP (`rtsp://ADRESSE/stream1`, JPEG über RTP, per UDP oder in der TCP-Verbindung, mit Digest- oder Basic-Anmeldung) und ONVIF (WS-Discovery, damit Rekorder und Verwaltungsprogramme den Knoten finden, und die Dienste des Profils S: Profile, Encoder mit den Größen, die die Kamera kann, die Adresse von Strom und Schnappschuss, das Benutzer-Token); beide nutzen Benutzer und Passwort der Einstellungen, ohne Passwort wird nichts ausgeliefert.
* **Das Panel und das Update wie bei den anderen Knoten:** Der Knoten hat das Web-Panel der Knoten von A.R.M.O.R. und von EH3D-NODE-BUNGARD (das Menü in Gruppen mit Sprache und Abmelden am Fuß; eine Übersicht; eine WLAN-Seite mit bis zu drei Ersatznetzen für den Fall, dass das Hauptnetz ausfällt; eine Seite Kopf, die die Einstellungen von Kamera, Streams, Laser und Spindel bearbeitet; Hilfe; Info; Benutzer; das Protokoll), das Kopieren eines Firmware-Slots auf den anderen in beide Richtungen und das Update von der neuesten GitHub-Release dieses Projekts (der von der Release veröffentlichte SHA-256 wird geprüft, die Adresse muss die des Projekts selbst sein und nur ein Administrator löst es aus; die Releases sind nicht signiert). Er teilt seine Firmware-Basis mit EH3D-NODE-BUNGARD.

## 🧩 Rolle in EH3D

EH3D-NODE-BUNGARD-AUX ist der Knoten am Bohrkopf einer Bungard-CCD-Maschine, neben EH3D-NODE-BUNGARD (dem Knoten, der die Maschine antreibt) und EH3D-NODE-BUNGARD-HMI (dem Bildschirm). Er sitzt am Kopf, sieht die Platine mit seiner Kamera, misst den Tisch unter dem Kopf und überwacht die Spindel. Die Desktop-Programme (EH3D-BUNGARD-GUI, DRILLPRO, ROUTEPRO) nutzen seine Messungen, um die Tischkarte zu bauen (`eh3d-levelling` in EH3D-COMMON) und das Z der Aufträge zu korrigieren; EH3D-BUNGARD-GUI fragt den Knoten direkt oder über EH3D-SERVER ab (das Fenster des Knotens am Bohrkopf).

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

<p align="center">
  <img src="images/screenshots/panel-overview.png" alt="panel-overview" width="100%">
</p>

<p align="center"><em>Die Seite Übersicht: der Knoten, seine Hardware, seine Uhr, sein Flash mit den zwei Firmware-Slots und sein Netz.</em></p>

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>Die Seite Status im Forge-Design der anderen EH3D-Programme.</em></p>

<p align="center">
  <img src="images/screenshots/panel-head.png" alt="panel-head" width="100%">
</p>

<p align="center"><em>Die Seite Kopf: Kamera, Streams, Laser, Spindelsensoren und Schutzausgang (Passwörter und Token sind nur schreibbar).</em></p>

<p align="center">
  <img src="images/screenshots/panel-wifi.png" alt="panel-wifi" width="100%">
</p>

<p align="center"><em>Die WLAN-Seite: das Netz, dem der Knoten beitritt, und bis zu drei Ersatznetze für den Fall des Verlusts.</em></p>

<p align="center">
  <img src="images/screenshots/panel-update.png" alt="panel-update" width="100%">
</p>

<p align="center"><em>Die Seite Firmware: das Update von GitHub, das Hochladen einer Datei und die zwei Slots mit dem Kopieren des einen auf den anderen.</em></p>

<p align="center">
  <img src="images/screenshots/panel-help.png" alt="panel-help" width="100%">
</p>

<p align="center"><em>Die Hilfeseite: ein Thema für jede Seite des Panels.</em></p>

## ✨ Funktionen

* **Eine Messung, der man trauen kann, oder keine:** mehrere Werte, Ausreißer verworfen, und eine Ablehnung (mit Grund), wenn die Werte zu stark streuen, außerhalb des Bereichs liegen oder zu wenige sind.
* **Eine ständig überwachte Spindel:** Drehzahl aus den Impulsen, Temperatur mit Warnstufe, kritischer Stufe und Hysterese, und ein Schutzausgang, der von selbst öffnet, wenn etwas nicht stimmt oder der Knoten nicht läuft.
* **Video wie eine IP-Kamera:** RTSP und ONVIF mit Anmeldung, damit jeder Player, Rekorder oder ONVIF-Manager es abholen kann; und die Kamera als MJPEG auf `/stream` hinter dem Token.
* **Standardmäßig sicher:** ohne Token keine Messung und keine Kamera; jede Grenze hat einen Schalter in den Einstellungen und beginnt in der konservativen Stellung.
* **Zwei Kameras, die OV2640 und die OV5640:** der Sensor sagt, welche es ist, und der Knoten liefert die größte Größe, die die Einstellungen erlauben und die dieser Sensor kann (die OV2640 bis 1600x1200, die OV5640 bis 2592x1944), meldet sie in `/api/status` (`camera_model`) und in den Encoder-Optionen von ONVIF; `camera_model = auto | ov2640 | ov5640` in die Seite Kopf. Die Pins des Kameraanschlusses der Waveshare ESP32-S3-ETH sind die Standardwerte von `menuconfig`.
* **Drei Temperatursensoren:** ein DS18B20 an einem Draht, ein NTC-Thermistor an einem Analogeingang (`ntc_series_ohm`, `ntc_r25_ohm`, `ntc_beta` in die Seite Kopf) oder ein Infrarotthermometer MLX90614, das die Spindel berührungslos ansieht (am I2C-Bus des Lasers); die Prüfsummen und die Umrechnungen sind auf einem Computer getestet.

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
| Einstellungen | ein Dokument im Flash, bearbeitet auf der Seite Kopf des Web-Panels; die Schlüssel sind die aus `head.cfg.example` | `core/bungard_config.hpp`, `core/aux_config.hpp` |
| Status | `GET /api/status`: der Knoten, die Kamera, der Laser, die Drehzahl, die Temperatur, der Schutzausgang und die Laufzeit als eine JSON-Zeile | `core/protocol.hpp` |
| Messung | `POST /api/measure[?samples=N]` mit Token: der Abstand zum Tisch, die Streuung und warum eine Messung abgelehnt wurde | `core/laser.hpp`, `main/app_main.cpp` |
| Kamera | `GET /stream` mit Token: MJPEG | `main/app_main.cpp` |
| Spindelimpulse | ein GPIO (`AUX_RPM_GPIO` in menuconfig), steigende Flanken | `main/sensors.cpp` |
| Schutzausgang | ein GPIO (`AUX_PROTECT_GPIO`), low, wenn die Spindel stoppen muss | `main/sensors.cpp` |
| Laser und Temperatur | I2C für den Laser und One-Wire für den Temperatursensor; die Treiber sind geschrieben und nie gelaufen (das Thermometer MLX90614 teilt sich den I2C-Bus, der DS18B20 nutzt einen Pin, der NTC einen Analogeingang) | `main/sensors.cpp` |
| RTSP | `rtsp://ADRESSE:PORT/stream1`: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN; JPEG über RTP per UDP oder verschachtelt in TCP; Digest- oder Basic-Anmeldung | `core/rtsp.hpp`, `core/rtsp_session.hpp`, `core/rtp_jpeg.hpp`, `main/stream.cpp` |
| ONVIF | WS-Discovery an der Multicast-Adresse `239.255.255.250:3702`; SOAP auf `/onvif/device_service` und `/onvif/media_service` (Profil S: Fähigkeiten, Dienste, Profile, Encoder, Adressen von Strom und Schnappschuss); der Schnappschuss auf `/snapshot.jpg` | `core/onvif.hpp`, `main/stream.cpp` |

## ⚙️ Konfiguration

Die Einstellungen liegen als ein einziges Dokument im Flash und werden auf der Seite Kopf des Web-Panels bearbeitet (Passwörter und Token sind nur schreibbar: das Panel zeigt sie nie). Die Schlüssel und ihre Bereiche sind die in `head.cfg.example` aufgeführten; die Datei die Seite Kopf früherer Versionen wird nicht mehr gelesen.

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
| `stream_user / stream_password` | `viewer / none` | 8 - 64 | die Anmeldung von RTSP und ONVIF; ohne Passwort bedienen sie niemanden |
| `rtsp_port` | `554` | 1 - 65535 | der Port des RTSP-Servers (der des Webservers ist `port`) |

## 🚀 Schnellstart

Du brauchst einen C++17-Compiler und CMake für die Tests auf dem Rechner und ESP-IDF 5.5.5 für die Firmware (das Ziel ESP32-S3).

```bash
build.bat            # die Tests auf dem Rechner (Windows)  -  ./build.sh unter Linux
python -I tools/ci_validate.py
```

Die Firmware wird mit ESP-IDF 5.5.5 gebaut: erzeugt `idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` das Image (die Pins deiner Platine unter `EH3D Bungard AUX` setzen), und die Einstellungen werden im Web-Panel vorgenommen, sobald der Knoten läuft.

## 🧪 Tests

Die Tests auf dem Rechner (`build.bat` oder `./build.sh`, dann `ctest`) sind zwei Programme und alles besteht: `test_aux` (103 Prüfungen) zur Einstellungsdatei (Standardwerte, Werte, falsche Werte, Grenzen, die zusammen keinen Sinn ergeben), zur Lasermessung (der Median, der verworfene Ausreißer, der Versatz, als schlecht markierte Werte, der Bereich, keine Werte, eine zu große Streuung), zur Spindeldrehzahl (Impulse je Umdrehung, eine langsamer werdende Spindel, Stillstand bei ausbleibenden Impulsen), zur Temperatur (die Hysterese, ein ausfallender Sensor), zum Schutzausgang und zu den Wörtern des Netzes (die Anfragezeile, die Routen, das Token im Header oder in der Abfrage, die Zahl der Werte, das JSON von Status und Messung, die Zeile eines Kartenpunkts). Die Firmware ist nicht abgedeckt: sie lässt sich bauen, die Tests laufen aber auf dem Computer. Das zweite Programm, `test_stream` (100 Prüfungen), deckt das Video ab: die Hashes der Anmeldungen gegen ihre veröffentlichten Vektoren (MD5, SHA-1, Base64 und das Digest-Beispiel des RFC 2617), die RTSP-Anfragen und die ganze Sitzung (Anmeldung, SDP, UDP- und verschachtelter Transport, Play, Pause, Teardown, eine falsche Sitzung, Multicast abgelehnt), die RTP-Nutzlast von JPEG (die Tabellen im ersten Paket, die Reihenfolge, die Markierung, der Überlauf der Sequenz) und ONVIF (Erkennung, das Benutzer-Token mit dem Beispiel des Standards, die offenen und die geschützten Aufrufe, die Dienste und ein nicht ausgeführter Aufruf). Neben diesen beiden Programmen liest, prüft und schreibt `test_head_settings` den Kopf-Abschnitt des Einstellungsdokuments wie das Panel (Passwörter und Token bleiben nur schreibbar), und die gemeinsamen Tests der Firmware-Basis decken das Update von GitHub, die Uhr und die Benutzer ab.

## 🏗️ Build und Veröffentlichung

Die Version steht in `eh3d.project.json`, und jede Version hat ihren Eintrag in [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` bauen und führen die Tests auf dem Rechner aus, `test.sh` / `test.bat` die Prüfungen des Repositorys. Die Firmware wird mit ESP-IDF 5.5.5 gebaut (`idf.py build`), was noch nicht geschehen ist; es wurde noch kein Release veröffentlicht.

## 🩺 Fehlerbehebung

| Symptom | Was tun |
|---|---|
| Eine Messung wird mit `no_readings` abgelehnt | Kein VL53L1X antwortet am I2C-Bus: prüfe die Pins SDA und SCL in `menuconfig` (die Kamera belegt auf der Waveshare ESP32-S3-ETH GPIO 41 und 42) und die Verdrahtung; der Rest der Antwort ist getestet. |
| Eine Messung wird mit `too_noisy` abgelehnt | Die Werte streuen mehr als `laser_sigma_max_mm`: lass Kopf und Tisch zur Ruhe kommen oder erhöhe die Grenze etwas; prüfe, dass nichts wackelt. |
| Der Knoten antwortet 401 auf eine Messung oder die Kamera | Setze `token` in die Seite Kopf (8 bis 64 Zeichen) und sende es im Header `Authorization: Bearer ...` oder in `?token=`. |
| Der Schutzausgang ist immer offen | Ein eingeschalteter Temperatursensor, der nicht antwortet, gilt als heiß: prüfe sein Kabel und seinen Pin in `menuconfig` sowie `temp_sensor` in die Seite Kopf, oder setze `temp_sensor = none` auf dem Prüfstand. |

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
│   ├── bungard_config.hpp                       die Einstellungen des Knotens und der Kopf-Abschnitt
│   ├── laser.hpp                                die Messung der Tischhöhe
│   ├── camera.hpp                               die Kameras OV2640 und OV5640 und ihre Größen
│   ├── spindle.hpp                              Drehzahl, Wärme und Schutzausgang
│   ├── thermo.hpp                               die Temperatur: DS18B20, NTC, MLX90614
│   ├── hash.hpp                                 die Anmeldungen: MD5, SHA-1, Base64
│   ├── rtsp.hpp rtsp_session.hpp rtp_jpeg.hpp   RTSP und die RTP-Nutzlast von JPEG
│   ├── onvif.hpp                                ONVIF: Erkennung und SOAP-Dienste
│   ├── protocol.hpp                             die Wörter des Netzes: Routen, Token, JSON
│   ├── node_settings.hpp auth.hpp               Einstellungen, Benutzer (Basis)
│   ├── netplan.hpp json.hpp                     Netzplan und JSON (Basis)
│   └── release_assets.hpp semver.hpp            welche Datei eines Releases das Image dieser Platine ist
├── main/
│   ├── app_main.cpp                             Start, Einstellungen, Netz, Panel
│   ├── head.cpp head.hpp                        Kamera, Laser, Spindel, Routen
│   ├── sensors.cpp sensors.hpp                  die Pins: Impulse, Schutzausgang, Sensoren
│   ├── stream.cpp stream.hpp                    RTSP-Server, ONVIF-Dienste und Erkennung
│   ├── idf_component.yml Kconfig.projbuild      Komponentenliste und menuconfig-Pins
│   ├── web_server.cpp network.cpp               Panel-Server, Netz (Basis)
│   ├── node_store.cpp node_guard.cpp            Speicher, Wächter (Basis)
│   └── github_update.cpp entropy.cpp            das Update von GitHub und das Kopieren eines Slots
├── panel/
│   ├── index.html style.css                     die Seite und das Layout des Seitenmenüs
│   └── app.js text.js                           die Seiten und die Texte in acht Sprachen
├── tests/                                       Host-Tests von Einstellungen, Tor, Sprache und Bildschirmtexten
├── docs/                                        die Hardware-Hinweise und die Quellen der beiden Bilder
├── tools/                                       CI-Prüfung, README-Prüfung und Bildwerkzeuge
├── CMakeLists.txt                               das ESP-IDF-Projekt
├── partitions.csv                               die Flash-Aufteilung mit zwei Slots
├── head.cfg.example                             jede Einstellung mit Standardwert und Bereich
├── sdkconfig.defaults                           ESP-IDF-Einstellungen für beide Platinen
├── sdkconfig.board.s3-eth                       Einstellungen der Kabel-Platine
├── sdkconfig.board.s3-wifi                      Einstellungen der WLAN-Platine
├── dependencies.lock                            die genauen Versionen der Komponenten
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

* Nichts lief auf einer Platine: die Firmware lässt sich bauen, wurde aber nicht geflasht oder erprobt, und die Pins der Kamera und des Netzwerkchips müssen mit dem Schaltplan der Platine geprüft werden.
* Die Treiber des Lasers (VL53L1X über I2C, mit der wenig genutzten Komponente `grrtzm/vl53l1x_library`), der Temperatursensoren und der Kamera sind geschrieben und nie gelaufen: der erste Sensor, der auf einer Platine antwortet, kann einen Fehler zeigen.
* EH3D-BUNGARD-GUI (das Fenster des Knotens am Bohrkopf) und EH3D-SERVER fragen den Knoten ab, direkt oder über den Server; keiner wurde an einem echten Knoten probiert.
* Der Netzwerkport ist einfaches HTTP: halte ihn in einem eigenen Netz.
* RTSP und ONVIF wurden mit keinem echten Player, Rekorder oder ONVIF-Manager erprobt; das Kameramodell auf der Platine entscheidet über Größen und Pins.

**Als Nächstes**

* Die Firmware bauen und die Treiber auf einer Platine ausprobieren.
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
