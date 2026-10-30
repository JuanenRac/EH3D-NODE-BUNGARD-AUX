<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-AUX banner" width="100%">
</p>

# 🎛️ EH3D-NODE-BUNGARD-AUX

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | 🇫🇷 <b>Français</b> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un petit nœud sur la tête de perçage d'une machine Bungard CCD (ESP32-S3 avec un port réseau filaire) : il envoie la vidéo de la caméra, mesure la hauteur de la table avec un laser, lit la vitesse et la température de la broche et ouvre une sortie de protection quand la broche est trop rapide ou trop chaude.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.1-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Vérification d'honnêteté - ce qui tourne aujourd'hui :** les règles du nœud sont écrites en C++17 et testées sur un ordinateur : le fichier de réglages avec ses valeurs sûres, la mesure de la hauteur de la table à partir de plusieurs lectures du laser (les valeurs aberrantes sont écartées et une mesure trop dispersée est refusée), la vitesse de la broche à partir des impulsions d'un capteur, la température avec son avertissement, son niveau critique et une hystérésis, la sortie de protection et les mots que le nœud dit sur le réseau (l'état, la mesure, le jeton). Le firmware ESP-IDF (réseau filaire, serveur web, caméra, impulsions de la broche, sortie de protection) est écrit mais n'a pas été compilé, flashé ni exécuté sur une carte ; **les pilotes du capteur laser et des capteurs de température ne sont pas encore écrits**, donc une mesure est refusée aujourd'hui, et rien n'a été branché à une machine.

---

> **État de sécurité (safety) et de sécurité informatique :** ceci n'est pas un système de sécurité (safety). La sortie de protection est un signal pour la ligne d'arrêt de l'unité de commande, pas un remplacement : l'arrêt qui protège les personnes et la machine est un bouton matériel qui coupe l'entraînement par lui-même. La sortie s'ouvre quand la broche est trop rapide ou trop chaude, quand un capteur de température activé ne répond pas et quand ce nœud ne tourne pas ; un nœud sans jeton refuse toute requête qui mesure ou montre la caméra. Ne vous y fiez pas avant que toute la chaîne ait été essayée sur une vraie machine. Le projet est indépendant : il n'est ni fabriqué, ni approuvé, ni soutenu par Bungard Elektronik, et les noms de produits n'apparaissent que pour dire avec quelles machines il est prévu de fonctionner.

## 📋 En un coup d'œil

| | |
|---|---|
| Version | `0.0.1` |
| Maturité | Échafaudage |
| Tests | 2 programmes de test sur l'ordinateur (175 vérifications) |
| Pile technique | C++17, ESP-IDF 5.5, ESP32-S3, réseau W5500 |
| Sur du matériel réel | Non : le firmware n'a pas été compilé ni flashé, et rien n'a été branché à une machine |

**Aller à  :** [Présentation](#-présentation) · [Architecture](#-architecture) · [Démarrage rapide](#-démarrage-rapide) · [Tests](#-tests) · [Structure](#-structure-du-dépôt) · [Liés](#-projets-liés)

## 🚦 État du projet
Chaque ligne est une partie de ce dépôt et le feu dit où elle en est aujourd'hui. Elle est écrite d'après ce que le dépôt peut montrer (tests, compilations, exécutions), pas d'après ce qui est prévu.

🟢 Fait et vérifié · 🟡 Fonctionne, avec des limites connues · 🟠 À moitié fait ou pas encore vérifié · 🔴 Pas fait ou ne fonctionne pas

| Domaine | État | Ce que cela veut dire ici |
|---|---|---|
| Logiciel | 🟠 | Le projet ESP-IDF (réseau filaire avec un W5500, serveur web, caméra, impulsions de la broche, sortie de protection, le fichier de réglages dans sa propre partition) est écrit ; il n'a pas été compilé ni flashé. |
| Tests | 🟢 | 2 programmes de test sur l'ordinateur réussissent (`ctest`) : 175 vérifications du fichier de réglages, de la mesure du laser, de la vitesse et de la température de la broche, de la sortie de protection et des mots du réseau. |
| Laser (hauteur de la table) | 🟡 | La mesure à partir de plusieurs lectures (médiane, lectures aberrantes écartées, dispersion vérifiée, plage et décalage) est testée ; le pilote du capteur n'est pas écrit, donc aucune mesure n'est possible aujourd'hui. |
| Broche | 🟡 | La vitesse à partir des impulsions d'un capteur (n'importe quel nombre par tour, arrêtée quand les impulsions cessent), la température avec avertissement, niveau critique et hystérésis et la sortie de protection sont testées ; les pilotes des capteurs de température ne sont pas écrits. |
| Caméra | 🟠 | Le flux MJPEG est écrit pour le connecteur de caméra de la carte et demande les broches de la carte dans menuconfig ; il n'a pas tourné. |
| Vidéo (RTSP, ONVIF) | 🟡 | La session RTSP (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, en UDP ou dans la connexion TCP, connexion Digest et Basic), la charge RTP de JPEG, la découverte et les services SOAP d'ONVIF (profils, encodeurs avec leurs tailles, adresses du flux et de l'instantané, le jeton d'utilisateur) sont testés sur un ordinateur ; aucun lecteur ni enregistreur n'a pris la vidéo d'une vraie carte. |
| Matériel réel | 🔴 | Aucune carte n'a exécuté ce firmware et rien n'a été branché à une machine. |
| Documentation | 🟢 | README en huit langues, la référence des réglages et les notes matérielles. |

## 🎯 Présentation

* La **mesure du laser** : le nœud demande plusieurs lectures au capteur, écarte les aberrantes (médiane et écart absolu médian), refuse une mesure qui se disperse plus que `laser_sigma_max_mm` ou qui sort de la plage, et répond la distance à la table avec la dispersion ; les programmes de bureau en font un point de la carte de la table (`Z x y hauteur`, lu par `eh3d-levelling`).
* La **broche** : la vitesse vient des impulsions d'un capteur (capteur à effet Hall ou disque à fentes, n'importe quel nombre d'impulsions par tour) et vaut zéro quand les impulsions cessent ; la température a un avertissement, un niveau critique et une hystérésis pour que l'état ne clignote pas ; un capteur activé qui ne répond pas compte comme chaud.
* La **sortie de protection** : un GPIO à l'état bas dès que la broche doit s'arrêter (trop rapide, trop chaude, un capteur muet) et aussi tant que le nœud ne tourne pas, à câbler sur la ligne d'arrêt de l'unité de commande par un relais ou un optocoupleur ; elle peut être désactivée dans les réglages.
* La **caméra et le réseau** : la vidéo de la caméra en MJPEG sur `/stream` et les lectures sur `/api/status`, par un port réseau filaire (W5500 sur SPI) ; le jeton protège la mesure et la caméra.
* Le **fichier de réglages** `head.cfg` : chaque valeur ajustée (le réseau, la caméra, le laser, la broche, la température, la sortie de protection) est une ligne d'un fichier dans sa propre partition, jamais un nombre dans le code ; une valeur hors plage est signalée et la valeur par défaut reste.
* La **vidéo comme l'envoie une caméra IP** : RTSP (`rtsp://ADRESSE/stream1`, JPEG sur RTP, en UDP ou dans la connexion TCP, avec connexion Digest ou Basic) et ONVIF (WS-Discovery, pour que enregistreurs et gestionnaires trouvent le nœud, et les services du profil S : profils, encodeurs avec les tailles que la caméra sait faire, l'adresse du flux et de l'instantané, le jeton d'utilisateur) ; les deux utilisent l'utilisateur et le mot de passe des réglages, et sans mot de passe rien n'est servi.

## 🧩 Rôle dans EH3D

EH3D-NODE-BUNGARD-AUX est le nœud de la tête de perçage d'une machine Bungard CCD, à côté d'EH3D-NODE-BUNGARD (le nœud qui pilote la machine) et d'EH3D-NODE-BUNGARD-HMI (l'écran). Il se place sur la tête, voit la carte avec sa caméra, mesure la table sous la tête et surveille la broche. Les programmes de bureau (EH3D-BUNGARD-GUI, DRILLPRO, ROUTEPRO) utilisent ses mesures pour construire la carte de la table (`eh3d-levelling` dans EH3D-COMMON) et corriger le Z des travaux ; la liaison se fera directement ou par EH3D-SERVER, et aucune n'existe encore.

## 🏛️ Architecture

<p align="center">
  <img src="images/architecture.svg" alt="Architecture de EH3D-NODE-BUNGARD-AUX : qui l'utilise, ce qu'il contient et de quoi il dépend" width="100%">
</p>

Comment s'assemblent les parties de ce dépôt : qui lui parle, ce qu'il contient et de quoi il dépend. Le dessin est tiré de `docs/architecture.json` et vérifié par rapport au code.

## 🖼️ Captures d'écran

<p align="center">
  <img src="images/screenshots/answers.png" alt="ce que répond le nœud" width="100%">
</p>

<p align="center"><em>Ce que le nœud répond sur le réseau : l'état (`GET /api/status`), une mesure de la table (`POST /api/measure`) et la ligne que les programmes de bureau lisent comme un point de la carte. Un dessin des formats que vérifient les tests, pas une capture d'une carte.</em></p>

## ✨ Fonctionnalités

* **Une mesure fiable, ou aucune :** plusieurs lectures, les aberrantes écartées, et un refus (avec le motif) quand les lectures se dispersent trop, sortent de la plage ou sont trop peu nombreuses.
* **Une broche surveillée en permanence :** vitesse à partir des impulsions, température avec un avertissement, un niveau critique et une hystérésis, et une sortie de protection qui s'ouvre toute seule quand quelque chose ne va pas ou que le nœud ne tourne pas.
* **Vidéo comme une caméra IP :** RTSP et ONVIF avec connexion, pour que n'importe quel lecteur, enregistreur ou gestionnaire ONVIF la prenne ; et la caméra en MJPEG sur `/stream` derrière le jeton.
* **Sûr par défaut :** sans jeton, ni mesure ni caméra ; chaque limite a un interrupteur dans les réglages et part de sa position conservatrice.

## 🔒 Sécurité et protection

| Menace | Défense |
|---|---|
| Une broche trop rapide ou trop chaude | la sortie de protection s'ouvre au-dessus de `rpm_max` et à la température critique, avec une hystérésis pour ne pas clignoter ; un capteur de température activé et muet compte comme chaud |
| Le nœud perd l'alimentation ou plante | la sortie est basse tant que le nœud n'a pas vérifié la broche et reste basse tant qu'il ne tourne pas, donc elle doit être câblée comme un arrêt qui s'ouvre tout seul |
| Quelqu'un sur le réseau mesure ou regarde l'atelier | sans jeton le nœud refuse la mesure et la caméra ; le jeton est comparé en temps constant et jamais affiché ni journalisé |
| Une mauvaise mesure prise pour bonne | les lectures aberrantes sont écartées, une dispersion au-dessus de `laser_sigma_max_mm`, la plage ou trop peu de lectures refusent la mesure avec le motif |
| Une faute de frappe dans les réglages | une valeur hors plage ou une clé inconnue est signalée et la valeur sûre reste ; une température critique sous l'avertissement, ou une plage à l'envers, revient aux valeurs par défaut |
| Un secret dans le firmware ou le dépôt | rien de secret n'est compilé ; le jeton est un réglage dans sa partition |

**Ce qui n'est pas défendu :** le nœud n'est pas un dispositif de sécurité. La sortie de protection vaut ce que vaut le câblage qui la mène à la ligne d'arrêt, et rien de tout cela n'a été essayé sur une machine. Le port réseau n'est pas chiffré (un simple port HTTP sur le réseau de l'atelier) : gardez-le sur un réseau à part.

## 🔌 Interfaces

| Interface | Ce que c'est | Où |
|---|---|---|
| Fichier de réglages | `head.cfg` dans la partition `config` : lignes `clé = valeur` ; voir `head.cfg.example` | `core/aux_config.hpp` |
| État | `GET /api/status` : le nœud, la caméra, le laser, la vitesse, la température, la sortie de protection et la durée de fonctionnement en une ligne de JSON | `core/protocol.hpp` |
| Mesure | `POST /api/measure[?samples=N]` avec le jeton : la distance à la table, la dispersion et pourquoi une mesure a été refusée | `core/laser.hpp`, `main/app_main.cpp` |
| Caméra | `GET /stream` avec le jeton : MJPEG | `main/app_main.cpp` |
| Impulsions de la broche | un GPIO (`AUX_RPM_GPIO` dans menuconfig), fronts montants | `main/sensors.cpp` |
| Sortie de protection | un GPIO (`AUX_PROTECT_GPIO`), bas quand la broche doit s'arrêter | `main/sensors.cpp` |
| Laser et température | I2C pour le laser et un fil pour le capteur de température ; les pilotes ne sont pas écrits | `main/sensors.cpp` |
| RTSP | `rtsp://ADRESSE:PORT/stream1` : OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN ; JPEG sur RTP en UDP ou entrelacé dans TCP ; connexion Digest ou Basic | `core/rtsp.hpp`, `core/rtsp_session.hpp`, `core/rtp_jpeg.hpp`, `main/stream.cpp` |
| ONVIF | WS-Discovery à l'adresse multicast `239.255.255.250:3702` ; SOAP sur `/onvif/device_service` et `/onvif/media_service` (profil S : capacités, services, profils, encodeurs, adresses du flux et de l'instantané) ; l'instantané sur `/snapshot.jpg` | `core/onvif.hpp`, `main/stream.cpp` |

## ⚙️ Configuration

Tout ce qui est réglé est une ligne de `head.cfg` (copiez `head.cfg.example` dans la partition `config`) ; une valeur hors plage est signalée dans le journal et la valeur par défaut reste. Les plus importants :

| Réglage | Défaut | Plage | Ce que ça fait |
|---|---|---|---|
| `token` | none | 8 - 64 | le jeton d'accès ; sans lui le nœud refuse la mesure et la caméra |
| `laser_samples` | `7` | 1 - 31 | lectures d'une mesure (la médiane des bonnes) |
| `laser_offset_mm` | `0` | -50 - 50 | ajouté à chaque distance : où le capteur se trouve au-dessus de la pointe de l'outil |
| `laser_sigma_max_mm` | `0.15` | 0.01 - 5 | une mesure dont les lectures se dispersent plus que cela est refusée |
| `rpm_pulses_per_rev` | `1` | 1 - 64 | impulsions du capteur par tour de la broche |
| `rpm_max` | `65000` | 1000 - 200000 | au-dessus de cette vitesse la sortie de protection s'ouvre |
| `temp_sensor` | none | none ds18b20 ntc mlx90614 | le capteur de la température de la broche |
| `temp_warn_c / temp_critical_c` | `55 / 70` | 20 - 200 | l'avertissement et le niveau auquel la sortie de protection s'ouvre |
| `protect_output` | `on` | on / off | la sortie de protection fonctionne ; ne la désactivez qu'au banc d'essai |
| `stream_user / stream_password` | `viewer / none` | 8 - 64 | la connexion de RTSP et ONVIF ; sans mot de passe ils ne servent personne |
| `rtsp_port` | `554` | 1 - 65535 | le port du serveur RTSP (celui du serveur web est `port`) |

## 🚀 Démarrage rapide

Il faut un compilateur C++17 et CMake pour les tests sur l'ordinateur, et ESP-IDF 5.5.5 pour le firmware (la cible ESP32-S3).

```bash
build.bat            # les tests sur l'ordinateur (Windows)  -  ./build.sh sous Linux
python -I tools/ci_validate.py
```

Le firmware n'a pas encore été compilé : quand il le sera, `idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` produit l'image (réglez les broches de votre carte sous `EH3D Bungard AUX`), et une partition nommée `config` contient `head.cfg`.

## 🧪 Tests

Les tests sur l'ordinateur (`build.bat` ou `./build.sh`, puis `ctest`) sont deux programmes et tout passe : `test_aux` (74 vérifications) sur le fichier de réglages (valeurs par défaut, valeurs, valeurs erronées, limites incohérentes entre elles), la mesure du laser (la médiane, la lecture aberrante écartée, le décalage, les lectures marquées mauvaises, la plage, aucune lecture, une dispersion trop grande), la vitesse de la broche (impulsions par tour, une broche qui ralentit, arrêtée quand les impulsions cessent), la température (l'hystérésis, un capteur qui tombe en panne), la sortie de protection et les mots du réseau (la ligne de requête, les routes, le jeton dans l'en-tête ou la requête, le nombre de lectures, le JSON de l'état et de la mesure, la ligne d'un point de la carte). Le firmware n'est pas couvert : il n'a pas été compilé. Le deuxième programme, `test_stream` (100 vérifications), couvre la vidéo : les empreintes des connexions contre leurs vecteurs publiés (MD5, SHA-1, base64 et l'exemple Digest du RFC 2617), les requêtes RTSP et toute la session (connexion, SDP, transport UDP et entrelacé, play, pause, teardown, une session erronée, multicast refusé), la charge RTP de JPEG (les tables dans le premier paquet, l'ordre, le marqueur, le bouclage de la séquence) et ONVIF (découverte, le jeton d'utilisateur avec l'exemple de la norme, les appels ouverts et protégés, les services et un appel non réalisé).

## 🏗️ Compilation et publication

La version se trouve dans `eh3d.project.json`, et chaque version a son entrée dans [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilent et exécutent les tests sur l'ordinateur et `test.sh` / `test.bat` exécutent les vérifications du dépôt. Le firmware se compile avec ESP-IDF 5.5.5 (`idf.py build`), ce qui n'a pas encore été fait ; aucune release n'a été publiée.

## 🩺 Dépannage

| Symptôme | Que faire |
|---|---|
| Une mesure est refusée avec `no_readings` | Le pilote du laser n'est pas écrit dans cette version, donc aucune lecture n'arrive jamais ; le reste de la réponse est testé. |
| Une mesure est refusée avec `too_noisy` | Les lectures se dispersent plus que `laser_sigma_max_mm` : laissez la tête et la table se stabiliser, ou relevez un peu la limite ; vérifiez que rien ne vibre. |
| Le nœud répond 401 à une mesure ou à la caméra | Réglez `token` dans `head.cfg` (8 à 64 caractères) et envoyez-le dans l'en-tête `Authorization: Bearer ...` ou dans `?token=`. |
| La sortie de protection est toujours ouverte | Un capteur de température activé qui ne répond pas compte comme chaud, et les pilotes ne sont pas encore écrits : mettez `temp_sensor = none` au banc d'essai, ou branchez un capteur quand son pilote existera. |

## 📂 Structure du dépôt

<p align="center">
  <img src="images/repository-structure.svg" alt="Structure du dépôt EH3D-NODE-BUNGARD-AUX" width="100%">
</p>

<details>
<summary>Version texte de l'arborescence</summary>

```text
EH3D-NODE-BUNGARD-AUX/
├── core/
│   ├── aux_config.hpp                           le fichier de réglages et ses valeurs sûres
│   ├── laser.hpp                                la mesure de la hauteur de la table
│   ├── spindle.hpp                              vitesse, chaleur et sortie de protection
│   ├── hash.hpp                                 les connexions : MD5, SHA-1, base64
│   ├── rtsp.hpp rtsp_session.hpp rtp_jpeg.hpp   RTSP et la charge RTP de JPEG
│   ├── onvif.hpp                                ONVIF : découverte et services SOAP
│   └── protocol.hpp                             les mots du réseau : routes, jeton, JSON
├── main/
│   ├── app_main.cpp                             démarrage, réglages, réseau, serveur web, caméra
│   ├── sensors.cpp sensors.hpp                  les broches : impulsions, sortie de protection, capteurs
│   ├── stream.cpp stream.hpp                    serveur RTSP, services ONVIF et découverte
│   └── idf_component.yml Kconfig.projbuild      liste des composants et broches de menuconfig
├── tests/                                       tests sur ordinateur des réglages, de la porte, de la voix et des textes d'écran
├── docs/                                        les notes matérielles et les sources des deux images
├── tools/                                       contrôle CI, contrôle du README et outils des images
├── CMakeLists.txt                               le projet ESP-IDF
├── partitions.csv                               la répartition de la flash à deux emplacements
├── head.cfg.example                             chaque réglage avec sa valeur par défaut et sa plage
├── sdkconfig.defaults                           réglages ESP-IDF pour le module ESP32-P4
├── images/                                      bannière et images d'Architecture et de Structure du dépôt
├── .github/                                     workflows de CI et de publication, modèles d'issues et de pull requests
├── build.sh build.bat                           compile et exécute les tests sur ordinateur dans un panneau en direct
├── eh3d.project.json                            manifeste : version, rôle, maturité et commande de compilation
├── CHANGELOG.md                                 ce qui a changé dans chaque version
├── SECURITY.md SUPPORT.md                       signaler une vulnérabilité et poser des questions
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   contribuer, code de conduite et licence
└── bump_manifest_version.py                     utilitaire de version du manifeste
```

</details>

## 🗺️ Feuille de route et limites connues

**Limites connues aujourd'hui**

* Rien n'a tourné sur une carte : le firmware n'a pas été compilé, flashé ni essayé, et les broches de la caméra et de la puce réseau doivent être vérifiées avec le schéma de la carte.
* Le pilote du capteur laser (VL53L1X en I2C) et ceux des capteurs de température ne sont pas écrits, donc aucune mesure n'est possible et un capteur de température activé compte comme chaud.
* La liaison avec les programmes de bureau et avec EH3D-SERVER n'existe pas encore : le nœud répond sur son port, personne ne l'interroge.
* Le port réseau est du HTTP simple : gardez-le sur un réseau à part.
* RTSP et ONVIF n'ont pas été essayés avec un vrai lecteur, enregistreur ou gestionnaire ONVIF ; le modèle de la caméra sur la carte décidera des tailles et des broches.

**Suite**

* Écrire les pilotes du laser et des capteurs de température et compiler le firmware.
* Ajouter la liaison avec EH3D-BUNGARD-GUI, avec EH3D-SERVER et avec l'écran, et la vue de la caméra.
* Essayer la sortie de protection câblée sur une ligne d'arrêt d'une machine.

## 🌍 Langues

Le README et les textes existent en anglais, espagnol, français, italien, allemand, chinois, japonais et portugais européen.

## 🔗 Projets liés

* [EH3D-BUNGARD-GUI](https://github.com/JuanenRac/EH3D-BUNGARD-GUI): l'interface moderne qui utilise les mesures pour construire la carte de la table
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): le nœud sur le port série de l'unité de commande qui pilote la machine
* [EH3D-NODE-BUNGARD-HMI](https://github.com/JuanenRac/EH3D-NODE-BUNGARD-HMI): l'écran tactile de l'opérateur
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): le centre de la famille : il portera les réponses du nœud à chaque client
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): les parties communes de la famille, dont `eh3d-levelling` (la carte des hauteurs de la table)

## 📚 Documentation

* Les réglages : `head.cfg.example` liste chaque clé avec sa valeur par défaut et sa plage.
* Le matériel : les broches, le câblage de la sortie de protection et les pièces, dans `docs/HARDWARE.md`.
* [Journal des modifications](CHANGELOG.md)

## 🤝 Contribuer et communauté

* Avant une pull request, lancez les tests sur ordinateur (`./build.sh` ou `build.bat`) et `python -I tools/ci_validate.py` : tout doit passer. Voir [CONTRIBUTING.md](CONTRIBUTING.md) et le [code de conduite](CODE_OF_CONDUCT.md).
* Le README existe en huit langues : une modification du texte se fait dans les huit fichiers, et les deux images sont dessinées depuis `docs/architecture.json` et `docs/structure.json` par `tools/make_architecture.py` et `tools/make_structure.py`.
* Rien de ce qui change la façon dont le nœud donne une mesure ou ouvre la sortie de protection ne change sans un test qui le couvre et une note de ce qui a été observé sur le matériel.
* Signalez une vulnérabilité en privé : [SECURITY.md](SECURITY.md). Questions, idées et signalements : electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTEUR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCE

Logiciel : GPL-3.0-or-later. Voir [LICENSE](LICENSE). Les conceptions matérielles de la suite utilisent la CERN OHL-S.
