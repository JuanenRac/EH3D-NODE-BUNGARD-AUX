<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-AUX banner" width="100%">
</p>

# 🎛️ EH3D-NODE-BUNGARD-AUX

<p align="center"><a href="README.md">🇺🇸 English</a> | 🇪🇸 <b>Español</b> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### Un pequeño nodo en el cabezal de taladro de una máquina Bungard CCD (ESP32-S3 con puerto de red por cable): envía el vídeo de la cámara, mide la altura de la mesa con un láser, lee la velocidad y la temperatura del husillo y abre una salida de protección cuando el husillo va demasiado rápido o está demasiado caliente.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.6-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - lo que se ejecuta hoy:** las reglas del nodo están escritas en C++17 y probadas en un ordenador: el fichero de ajustes con sus valores seguros, la medida de la altura de la mesa a partir de varias lecturas del láser (las raras se descartan y una medida que se dispersa demasiado se rechaza), la velocidad del husillo a partir de los pulsos de un sensor, la temperatura con su aviso, su nivel crítico y una histéresis, la salida de protección y las palabras que el nodo dice en la red (el estado, la medida, el token). El firmware de ESP-IDF (red por cable, servidor web, cámara, pulsos del husillo, salida de protección) está escrito y se compila con ESP-IDF 5.5.5, pero nunca se ha grabado ni ejecutado en una placa; **los controladores del láser (VL53L1X), de los sensores de temperatura (DS18B20, NTC, MLX90614) y de la cámara (OV2640 u OV5640) están escritos pero nunca se han ejecutado en una placa**, así que nunca se ha hecho una medida, y no se ha conectado nada a una máquina.

---

> **Estado de seguridad (safety) y de seguridad informática:** esto no es un sistema de seguridad (safety). La salida de protección es una señal para la línea de parada de la unidad de control, no un sustituto de ella: la parada que protege a las personas y a la máquina es un pulsador por hardware que corta el accionamiento por sí mismo. La salida se abre cuando el husillo va demasiado rápido o está demasiado caliente, cuando un sensor de temperatura activado no responde y cuando este nodo no está funcionando; un nodo sin token rechaza toda petición que mida o muestre la cámara. No confíes en él hasta que toda la cadena se haya probado en una máquina real. El proyecto es independiente: no está hecho, avalado ni respaldado por Bungard Elektronik, y los nombres de producto aparecen solo para decir con qué máquinas está pensado para funcionar.

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.0.6` |
| Madurez | Andamiaje |
| Pruebas | 12 programas de prueba en el ordenador |
| Pila tecnológica | C++17, ESP-IDF 5.5, ESP32-S3, red W5500 |
| En hardware real | No: el firmware se compila pero no se ha grabado, y no se ha conectado nada a una máquina |

**Ir a:** [Resumen](#-resumen) · [Arquitectura](#-arquitectura) · [Inicio rápido](#-inicio-rápido) · [Pruebas](#-pruebas) · [Estructura](#-estructura-del-repositorio) · [Relacionados](#-proyectos-relacionados)

## 🚦 Estado del proyecto
Cada línea es una parte de este repositorio y la luz dice hasta dónde llega hoy. Se escribe con lo que el repositorio puede demostrar (pruebas, compilaciones, ejecuciones), no con lo previsto.

🟢 Hecho y verificado · 🟡 Funciona, con límites conocidos · 🟠 Hecho a medias o sin verificar · 🔴 Sin hacer o sin funcionar

| Área | Estado | Qué significa aquí |
|---|---|---|
| Software | 🟠 | El proyecto de ESP-IDF (red por cable con un W5500, servidor web, cámara, pulsos del husillo, salida de protección, el fichero de ajustes en su propia partición) está escrito y se compila; no se ha grabado. |
| Pruebas | 🟡 | 12 programas de prueba pasan (`ctest`): los ajustes, la medida del láser, la velocidad y la temperatura del husillo, la salida de protección, las palabras de la red, el vídeo (RTSP, ONVIF), el documento de ajustes del cabezal, la actualización desde GitHub, el reloj, lo que comparten todos los nodos, los textos del panel en 37 idiomas y las pilas de las tareas. |
| Láser (altura de la mesa) | 🟡 | La medida a partir de varias lecturas (mediana, lecturas raras descartadas, dispersión comprobada, rango y desplazamiento) está probada; el controlador del VL53L1X está escrito pero nunca se ha ejecutado, así que aún no se ha hecho ninguna medida. |
| Husillo | 🟡 | La velocidad a partir de los pulsos de un sensor (cualquier número por vuelta, parado cuando cesan los pulsos), la temperatura con aviso, nivel crítico e histéresis y la salida de protección están probadas; los controladores del DS18B20, del NTC y del MLX90614 están escritos pero nunca se han ejecutado. |
| Cámara | 🟠 | El flujo MJPEG está escrito para el conector de cámara de la placa y necesita los pines de la placa en menuconfig; no se ha ejecutado. |
| Vídeo (RTSP, ONVIF) | 🟡 | La sesión RTSP (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, por UDP o dentro de la conexión TCP, acceso Digest y Basic), la carga RTP de JPEG, el descubrimiento y los servicios SOAP de ONVIF (perfiles, codificadores con sus tamaños, direcciones del flujo y de la instantánea, el token de usuario) están probados en un ordenador; ningún reproductor ni grabador ha tomado el vídeo de una placa real. |
| Hardware real | 🔴 | Ninguna placa ha ejecutado este firmware y no se ha conectado nada a una máquina. |
| Documentación | 🟡 | README en ocho idiomas, la referencia de ajustes y las notas de hardware. |

## 🎯 Resumen

* La **medida del láser**: el nodo pide al sensor varias lecturas, descarta las raras (mediana y desviación absoluta mediana), rechaza una medida que se dispersa más de `laser_sigma_max_mm` o que cae fuera del rango, y responde con la distancia a la mesa y su dispersión; los programas de escritorio la convierten en un punto del mapa de la mesa (`Z x y altura`, que lee `eh3d-levelling`).
* El **husillo**: la velocidad sale de los pulsos de un sensor (un sensor hall o un disco ranurado, cualquier número de pulsos por vuelta) y es cero cuando los pulsos cesan; la temperatura tiene un aviso, un nivel crítico y una histéresis para que el estado no parpadee; un sensor activado que no responde cuenta como caliente.
* La **salida de protección**: un GPIO que está en bajo siempre que el husillo debe parar (demasiado rápido, demasiado caliente, un sensor mudo) y también mientras el nodo no funciona, para cablearlo a la línea de parada de la unidad de control mediante un relé o un optoacoplador; se puede desactivar en los ajustes.
* La **salida protect está vigilada**: la tarea que la escribe está suscrita al watchdog de tareas y figura entre las tareas que espera el guardián del nodo, así que un cuelgue reinicia el chip, el pin vuelve a entrada y se abre la línea de paro, en vez de quedarse la salida abierta para siempre. Nada de esto se ha ejecutado en una placa.
* La **cámara y la red**: el vídeo de la cámara como MJPEG en `/stream` y las lecturas en `/api/status`, por un puerto de red por cable (W5500 sobre SPI); el token protege la medida y la cámara.
* Los **ajustes**: cada valor afinado (la red, la cámara, el láser, el husillo, la temperatura, la salida de protección) es un ajuste del nodo, guardado en la flash y editado en el panel web, nunca un número en el código; un valor fuera de rango se rechaza con su motivo.
* El **vídeo como lo envía una cámara IP**: RTSP (`rtsp://DIRECCIÓN/stream1`, JPEG sobre RTP, por UDP o dentro de la conexión TCP, con acceso Digest o Basic) y ONVIF (WS-Discovery, para que grabadores y gestores encuentren el nodo, y los servicios del perfil S: perfiles, codificadores con los tamaños que la cámara puede hacer, la dirección del flujo y de la instantánea, el token de usuario); los dos usan el usuario y la contraseña de los ajustes, y sin contraseña no se sirve nada.
* **El panel y la actualización, como los otros nodos:** el nodo tiene el panel web de los nodos de A.R.M.O.R. y de EH3D-NODE-BUNGARD (el menú en grupos con el idioma y el cierre de sesión al pie; un Resumen; una página Wi-Fi con hasta tres redes de respaldo por si cae la principal; una página Cabezal que edita los ajustes de la cámara, las transmisiones, el láser y el husillo; Ayuda; Acerca de; usuarios; el registro), la copia de un slot de firmware sobre el otro en ambos sentidos y la actualización desde la última release de GitHub de este proyecto (se comprueba el SHA-256 que publica la release, la dirección debe ser la del propio proyecto y solo la pide un administrador; las releases no están firmadas). Comparte su base de firmware con EH3D-NODE-BUNGARD.

## 🧩 Papel en EH3D

EH3D-NODE-BUNGARD-AUX es el nodo del cabezal de taladro de una máquina Bungard CCD, junto a EH3D-NODE-BUNGARD (el nodo que maneja la máquina) y EH3D-NODE-BUNGARD-HMI (la pantalla). Va en el cabezal, ve la placa con su cámara, mide la mesa bajo el cabezal y vigila el husillo. Los programas de escritorio (EH3D-BUNGARD-GUI, DRILLPRO, ROUTEPRO) usan sus medidas para construir el mapa de la mesa (`eh3d-levelling` en EH3D-COMMON) y corregir la Z de los trabajos; EH3D-BUNGARD-GUI consulta el nodo directamente o a través de EH3D-SERVER (la ventana del nodo del cabezal).

## 🏛️ Arquitectura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitectura de EH3D-NODE-BUNGARD-AUX: quién lo usa, qué contiene y de qué depende" width="100%">
</p>

Cómo encajan las partes de este repositorio: quién habla con él, qué contiene y de qué depende. El dibujo sale de `docs/architecture.json` y se comprueba contra el código.

## 🖼️ Capturas de pantalla

<p align="center">
  <img src="images/screenshots/answers.png" alt="lo que responde el nodo" width="100%">
</p>

<p align="center"><em>Lo que el nodo responde en la red: el estado (`GET /api/status`), una medida de la mesa (`POST /api/measure`) y la línea que los programas de escritorio leen como un punto del mapa. Un dibujo de los formatos que comprueban las pruebas, no una captura de una placa.</em></p>

<p align="center">
  <img src="images/screenshots/panel-overview.png" alt="panel-overview" width="100%">
</p>

<p align="center"><em>La página Resumen: el nodo, su hardware, su reloj, su flash con los dos slots de firmware y su red.</em></p>

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>La página Estado, en el tema Forge de los otros programas EH3D.</em></p>

<p align="center">
  <img src="images/screenshots/panel-head.png" alt="panel-head" width="100%">
</p>

<p align="center"><em>La página Cabezal: la cámara, las transmisiones, el láser, los sensores del husillo y la salida de protección (las contraseñas y el token son de solo escritura).</em></p>

<p align="center">
  <img src="images/screenshots/panel-wifi.png" alt="panel-wifi" width="100%">
</p>

<p align="center"><em>La página Wi-Fi: la red a la que se une el nodo y hasta tres redes de respaldo por si se pierde.</em></p>

<p align="center">
  <img src="images/screenshots/panel-update.png" alt="panel-update" width="100%">
</p>

<p align="center"><em>La página Firmware: la actualización desde GitHub, la subida de un fichero y los dos slots, con la copia de uno sobre otro.</em></p>

<p align="center">
  <img src="images/screenshots/panel-help.png" alt="panel-help" width="100%">
</p>

<p align="center"><em>La página Ayuda: un tema para cada página del panel.</em></p>

## ✨ Funciones

* **Una medida de fiar, o ninguna:** varias lecturas, las raras descartadas, y un rechazo (con el motivo) cuando las lecturas se dispersan demasiado, están fuera del rango o son pocas.
* **Un husillo vigilado todo el tiempo:** velocidad a partir de los pulsos, temperatura con un aviso, un nivel crítico y una histéresis, y una salida de protección que se abre sola cuando algo va mal o el nodo no funciona.
* **Vídeo como una cámara IP:** RTSP y ONVIF con acceso, para que lo tome cualquier reproductor, grabador o gestor ONVIF; y la cámara como MJPEG en `/stream` detrás del token.
* **Seguro por defecto:** sin token no hay medida ni cámara; cada límite tiene un interruptor en los ajustes y empieza en su posición conservadora.
* **Dos cámaras, la OV2640 y la OV5640:** el sensor dice cuál es y el nodo da el mayor tamaño que permiten los ajustes y que ese sensor puede dar (la OV2640 hasta 1600x1200, la OV5640 hasta 2592x1944), lo informa en `/api/status` (`camera_model`) y en las opciones del codificador de ONVIF; `camera_model = auto | ov2640 | ov5640` en la página Cabezal. Los pines del conector de cámara de la Waveshare ESP32-S3-ETH son los valores por defecto de `menuconfig`.
* **Tres sensores de temperatura:** un DS18B20 en un hilo, un termistor NTC en una entrada analógica (`ntc_series_ohm`, `ntc_r25_ohm`, `ntc_beta` en la página Cabezal) o un termómetro infrarrojo MLX90614 que mira el husillo sin tocarlo (en el bus I2C del láser); las sumas de comprobación y las conversiones están probadas en un ordenador.
* **Un aro de LEDs alrededor de la cámara:** ocho LEDs RGB del tipo WS2812 en un solo hilo iluminan la zona bajo el cabezal para que la cámara vea bien. El blanco es la luz para la cámara; los niveles siguen al ojo y nunca pasan de un techo; un solo lado da una luz rasante; se puede pedir cualquier color, y el aro dice cómo fue una verificación (verde, rojo parpadeando, un punto azul que gira, ámbar, rojo rápido). Se apaga solo pasado un tiempo. `GET` y `POST /api/light` y `GET /api/snapshot` sirven a los programas; los ajustes están en el panel y en `head.cfg` (`light_*`), y el pin en el menú del firmware (`AUX_LIGHT_GPIO`).
* **37 idiomas:** el panel web del nodo habla inglés, español, italiano, alemán, francés, portugués, chino, japonés, búlgaro, croata, checo, danés, neerlandés, estonio, finés, griego, húngaro, irlandés, letón, lituano, maltés, polaco, rumano, eslovaco, esloveno, sueco, noruego, islandés, ucraniano, ruso, serbio, albanés, catalán, gallego, euskera, portugués de Brasil y francés de Canadá. Las palabras de los 29 idiomas posteriores a los ocho primeros están traducidas por el modelo y esperan la revisión de hablantes nativos; un texto que a un idioma le falta se muestra en inglés.
* **Una barra de guardar siempre presente y una copia de seguridad de la configuración:** Guardar y Guardar y reiniciar están siempre en las páginas con ajustes (apagados, con un aviso, hasta que algo cambia), Reiniciar está siempre a mano para un administrador, y la página Sistema exporta todos los ajustes a un fichero (con las contraseñas Wi-Fi y el token: guárdalo en privado) y los importa de vuelta a los formularios sin enviar nada, de modo que cada página se guarda con su propio Guardar; el id, el nombre y el nombre de host del nodo nunca cambian con una importación. La página Red edita además el nombre, el servidor de hora, la zona horaria y el reinicio periódico, y las redes Wi-Fi de respaldo ahora cuentan como un cambio y se envían al guardar. Escrito para el navegador; no probado en placa.
* **El láser, ajustado al trabajo:** el VL53L1X tiene modo de distancia (corto o largo) y presupuesto de tiempo, un área de recepción que se puede estrechar para mirar un punto pequeño, un filtro que descarta lecturas lejos de la mediana, un estado para cada lectura (válida, señal débil, dispersión alta, fuera de rango) y una calibración a distancia conocida (desfase y diafonía) que un administrador debe activar antes. Aún no probado con un sensor.
* **Varios DS18B20 en el mismo cable:** el nodo busca en el cable todos los sensores (con el CRC de cada dirección), toma el del husillo por su dirección (`temp_rom`, o el único, o el primero que encuentre con un aviso en la página de estado), fija la resolución de 9 a 12 bits (`temp_resolution`) y espera solo lo que esa resolución necesita. Probado en un ordenador con un bus simulado; aún no probado en un cable real.
* **Un sensor de velocidad que distingue el ruido de las vueltas:** el TCRT5000 (reflectivo, con su marca en el husillo) funciona con su salida digital por interrupción, o con su salida analógica leída por un ADC rápido y un disparador Schmitt cuyos umbrales siguen la señal; se filtran los pulsos más rápidos de lo que el husillo puede girar, los picos en mitad de una vuelta y los pulsos perdidos, y el estado dice si los pulsos son limpios o ruidosos. Probado con trenes de pulsos simulados a 400 Hz y 1 kHz; aún no probado en un husillo.
* **La imagen de la cámara, ajustada desde el panel:** espejo, volteo, brillo, contraste, saturación, exposición automática o manual y balance de blancos automático, aplicados a la OV2640 y a la OV5640 al arrancar la cámara (un ajuste que un sensor no tiene se deja como está). Aún no probado con una cámara.

## 🔒 Seguridad y protección

| Amenaza | Defensa |
|---|---|
| Un husillo que va demasiado rápido o demasiado caliente | la salida de protección se abre por encima de `rpm_max` y en la temperatura crítica, con una histéresis para que no parpadee; un sensor de temperatura activado y mudo cuenta como caliente |
| El nodo pierde la alimentación o se cuelga | la salida está en bajo hasta que el nodo ha comprobado el husillo y sigue en bajo mientras no funciona, así que debe cablearse como una parada que se abre sola |
| Alguien en la red mide o mira el taller | sin token el nodo rechaza la medida y la cámara; el token se compara en tiempo constante y nunca se muestra ni se registra |
| Una mala medida tomada por buena | las lecturas raras se descartan, una dispersión por encima de `laser_sigma_max_mm`, un rango o pocas lecturas rechazan la medida con el motivo |
| Un error al escribir los ajustes | un valor fuera de rango o una clave desconocida se avisa y se queda el valor seguro; una temperatura crítica por debajo del aviso, o un rango del revés, vuelve a los valores por defecto |
| Un secreto en el firmware o en el repositorio | nada secreto se compila; el token es un ajuste en su partición |

**Lo que no se defiende:** el nodo no es un dispositivo de seguridad. La salida de protección vale lo que valga el cableado que la lleva a la línea de parada, y nada de esto se ha probado en una máquina. El puerto de red no tiene cifrado (un puerto HTTP sin más en la red del taller): mantenlo en una red propia.

## 🔌 Interfaces

| Interfaz | Qué es | Dónde |
|---|---|---|
| Ajustes | un documento en la flash, editado en la página Cabezal del panel web; las claves son las de `head.cfg.example` | `core/bungard_config.hpp`, `core/aux_config.hpp` |
| Estado | `GET /api/status`: el nodo, la cámara, el láser, la velocidad, la temperatura, la salida de protección y el tiempo encendido en una línea de JSON | `core/protocol.hpp` |
| Medida | `POST /api/measure[?samples=N]` con el token: la distancia a la mesa, la dispersión y por qué se rechazó una medida | `core/laser.hpp`, `main/app_main.cpp` |
| Cámara | `GET /stream` con el token: MJPEG | `main/app_main.cpp` |
| Pulsos del husillo | un GPIO (`AUX_RPM_GPIO` en menuconfig), flancos de subida | `main/sensors.cpp` |
| Salida de protección | un GPIO (`AUX_PROTECT_GPIO`), en bajo cuando el husillo debe parar | `main/sensors.cpp` |
| Láser y temperatura | I2C para el láser y un hilo para el sensor de temperatura; los controladores están escritos y nunca se han ejecutado (el termómetro MLX90614 comparte el bus I2C, el DS18B20 usa un pin, el NTC una entrada analógica) | `main/sensors.cpp` |
| RTSP | `rtsp://DIRECCIÓN:PUERTO/stream1`: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN; JPEG sobre RTP por UDP o intercalado en TCP; acceso Digest o Basic | `core/rtsp.hpp`, `core/rtsp_session.hpp`, `core/rtp_jpeg.hpp`, `main/stream.cpp` |
| ONVIF | WS-Discovery en la dirección multicast `239.255.255.250:3702`; SOAP en `/onvif/device_service` y `/onvif/media_service` (perfil S: capacidades, servicios, perfiles, codificadores, direcciones del flujo y de la instantánea); la instantánea en `/snapshot.jpg` | `core/onvif.hpp`, `main/stream.cpp` |

## ⚙️ Configuración

Los ajustes se guardan en la flash como un único documento y se editan en la página Cabezal del panel web (las contraseñas y el token son de solo escritura: el panel nunca los muestra). Las claves y sus rangos son los de `head.cfg.example`; el fichero la página Cabezal de versiones anteriores ya no se lee.

| Ajuste | Por defecto | Rango | Qué hace |
|---|---|---|---|
| `token` | none | 8 - 64 | el token de acceso; sin él el nodo rechaza la medida y la cámara |
| `laser_samples` | `7` | 1 - 31 | lecturas de una medida (la mediana de las buenas) |
| `laser_offset_mm` | `0` | -50 - 50 | se suma a cada distancia: dónde está el sensor sobre la punta de la herramienta |
| `laser_sigma_max_mm` | `0.15` | 0.01 - 5 | una medida cuyas lecturas se dispersan más que esto se rechaza |
| `rpm_pulses_per_rev` | `1` | 1 - 64 | pulsos del sensor por cada vuelta del husillo |
| `rpm_max` | `65000` | 1000 - 200000 | por encima de esta velocidad se abre la salida de protección |
| `temp_sensor` | none | none ds18b20 ntc mlx90614 | el sensor de la temperatura del husillo |
| `temp_warn_c / temp_critical_c` | `55 / 70` | 20 - 200 | el aviso y el nivel en que se abre la salida de protección |
| `protect_output` | `on` | on / off | la salida de protección funciona; desactívala solo en el banco de pruebas |
| `stream_user / stream_password` | `viewer / none` | 8 - 64 | el acceso de RTSP y ONVIF; sin contraseña no sirven a nadie |
| `rtsp_port` | `554` | 1 - 65535 | el puerto del servidor RTSP (el del servidor web es `port`) |

## 🚀 Inicio rápido

Necesitas un compilador de C++17 y CMake para las pruebas en el ordenador, y ESP-IDF 5.5.5 para el firmware (el objetivo ESP32-S3).

```bash
build.bat            # las pruebas en el ordenador (Windows)  -  ./build.sh en Linux
python -I tools/ci_validate.py
```

El firmware se compila con ESP-IDF 5.5.5: `idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` genera la imagen (fija los pines de tu placa en `EH3D Bungard AUX`), y los ajustes se hacen en el panel web cuando el nodo está en marcha.

## 🧪 Pruebas

Las pruebas en el ordenador (`build.bat` o `./build.sh`, luego `ctest`) son dos programas y todo pasa: `test_aux` (103 comprobaciones) sobre el fichero de ajustes (valores por defecto, valores, valores erróneos, límites que no tienen sentido juntos), la medida del láser (la mediana, la lectura rara descartada, el desplazamiento, lecturas marcadas como malas, el rango, ninguna lectura, una dispersión demasiado grande), la velocidad del husillo (pulsos por vuelta, un husillo que frena, parado cuando cesan los pulsos), la temperatura (la histéresis, un sensor que falla), la salida de protección y las palabras de la red (la línea de petición, las rutas, el token en la cabecera o en la consulta, el número de lecturas, el JSON del estado y de la medida, la línea de un punto del mapa). El firmware no está cubierto: se compila, pero las pruebas son del ordenador. El segundo programa, `test_stream` (100 comprobaciones), cubre el vídeo: los resúmenes de los accesos contra sus vectores publicados (MD5, SHA-1, base64 y el ejemplo Digest del RFC 2617), las peticiones RTSP y toda la sesión (acceso, SDP, transporte UDP e intercalado, play, pause, teardown, una sesión errónea, multicast rechazado), la carga RTP de JPEG (las tablas en el primer paquete, el orden, el marcador, el cambio de la secuencia) y ONVIF (descubrimiento, el token de usuario con el ejemplo del estándar, las llamadas abiertas y las protegidas, los servicios y una llamada que no se hace). Además de esos dos programas, `test_head_settings` lee, comprueba y escribe la sección del cabezal del documento de ajustes como lo hace el panel (las contraseñas y el token siguen siendo de solo escritura), y las pruebas compartidas de la base del firmware cubren la actualización desde GitHub, el reloj y los usuarios.

Autotest para el día en que la cabeza se encuentre con el hardware: `python tools/selftest.py --host NODO --token TOKEN` lee el nodo (versión, estado, exportación de la configuración, redes de respaldo, temperatura, rpm, luz, imagen de la cámara, flujo, RTSP, una lectura del láser) y escribe un informe con una línea por paso; con `--active` y una confirmación escrita también enciende el aro de luz dos segundos y lo deja como estaba. Nunca ejecuta la calibración del láser. Sus pruebas se ejecutan en la CI contra un nodo de juguete (`python -I -m unittest discover -s tools -p "test_*.py"`). Los pasos manuales están en [docs/HARDWARE_CHECKLIST.md](docs/HARDWARE_CHECKLIST.md).

## 🏗️ Compilación y publicación

La versión está en `eh3d.project.json`, y cada versión tiene su entrada en [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilan y ejecutan las pruebas en el ordenador y `test.sh` / `test.bat` ejecutan las comprobaciones del repositorio. El firmware se compila con ESP-IDF 5.5.5 (`idf.py build`), lo que todavía no se ha hecho; no se ha publicado ninguna versión.

## 🩺 Resolución de problemas

| Síntoma | Qué hacer |
|---|---|
| Una medida se rechaza con `no_readings` | Ningún VL53L1X responde en el bus I2C: comprueba los pines SDA y SCL en `menuconfig` (la cámara usa los GPIO 41 y 42 en la Waveshare ESP32-S3-ETH) y el cableado; el resto de la respuesta está probado. |
| Una medida se rechaza con `too_noisy` | Las lecturas se dispersan más que `laser_sigma_max_mm`: deja que se asienten el cabezal y la mesa, o sube un poco el límite; comprueba que nada vibra. |
| El nodo responde 401 a una medida o a la cámara | Pon `token` en la página Cabezal (de 8 a 64 caracteres) y envíalo en la cabecera `Authorization: Bearer ...` o en `?token=`. |
| La salida de protección está siempre abierta | Un sensor de temperatura activado que no responde cuenta como caliente: comprueba su cable y su pin en `menuconfig` y `temp_sensor` en la página Cabezal, o pon `temp_sensor = none` en el banco. |

## 📂 Estructura del repositorio

<p align="center">
  <img src="images/repository-structure.svg" alt="Estructura del repositorio EH3D-NODE-BUNGARD-AUX" width="100%">
</p>

<details>
<summary>Versión en texto del árbol</summary>

```text
EH3D-NODE-BUNGARD-AUX/
├── core/
│   ├── aux_config.hpp                           el fichero de ajustes y sus valores seguros
│   ├── bungard_config.hpp                       los ajustes del nodo y la sección del cabezal
│   ├── laser.hpp                                la medida de la altura de la mesa
│   ├── laser_hw.hpp                             los registros del láser
│   ├── light.hpp                                el aro de LEDs: niveles, colores, señales
│   ├── camera.hpp                               las cámaras OV2640 y OV5640 y sus tamaños
│   ├── spindle.hpp                              el sensor de velocidad: pulsos, ruido, Schmitt
│   ├── thermo.hpp                               la temperatura: DS18B20, NTC, MLX90614
│   ├── onewire.hpp                              el bus 1-Wire: búsqueda, dirección, resolución
│   ├── hash.hpp                                 los accesos: MD5, SHA-1, base64
│   ├── rtsp.hpp rtsp_session.hpp rtp_jpeg.hpp   RTSP y la carga RTP de JPEG
│   ├── onvif.hpp                                ONVIF: descubrimiento y servicios SOAP
│   ├── protocol.hpp                             las palabras de la red: rutas, token, JSON
│   ├── node_settings.hpp auth.hpp               ajustes y usuarios (base común)
│   ├── netplan.hpp json.hpp                     plan de red y JSON (base común)
│   └── release_assets.hpp semver.hpp            qué fichero de una versión es la imagen de esta placa
├── main/
│   ├── app_main.cpp                             arranque, ajustes, red, panel
│   ├── head.cpp head.hpp                        cámara, láser, husillo, rutas
│   ├── light.cpp light.hpp                      el controlador del aro de LEDs
│   ├── sensors.cpp sensors.hpp                  los pines: pulsos, salida de protección, sensores
│   ├── stream.cpp stream.hpp                    servidor RTSP, servicios ONVIF y descubrimiento
│   ├── idf_component.yml Kconfig.projbuild      lista de componentes y pines de menuconfig
│   ├── web_server.cpp network.cpp               servidor del panel, red (base)
│   ├── node_store.cpp node_guard.cpp            almacén y vigilante (base)
│   └── github_update.cpp entropy.cpp            la actualización desde GitHub y la copia de una ranura
├── panel/
│   ├── index.html style.css                     la página y la disposición del menú lateral
│   ├── app.js text.js                           las páginas y los textos en 37 idiomas
│   └── locales/                                 las palabras de 29 idiomas más, un fichero cada uno
├── tests/                                       pruebas en el ordenador de los ajustes, la compuerta, la voz y los textos de pantalla
├── docs/                                        las notas de hardware y las fuentes de los dos dibujos
├── tools/                                       comprobación de CI, de README y herramientas de dibujos
├── CMakeLists.txt                               el proyecto ESP-IDF
├── partitions.csv                               el reparto de la flash con dos ranuras
├── head.cfg.example                             cada ajuste con su valor por defecto y su rango
├── sdkconfig.defaults                           ajustes de ESP-IDF comunes a las dos placas
├── sdkconfig.board.s3-eth                       ajustes de la placa con cable
├── sdkconfig.board.s3-wifi                      ajustes de la placa Wi-Fi
├── dependencies.lock                            las versiones exactas de los componentes
├── images/                                      banner y las imágenes de Arquitectura y Estructura del repositorio
├── .github/                                     flujos de CI y de publicación, plantillas de issues y pull requests
├── build.sh build.bat                           compila y ejecuta las pruebas en el ordenador con un panel en vivo
├── eh3d.project.json                            manifiesto: versión, rol, madurez y comando de compilación
├── CHANGELOG.md                                 qué cambió en cada versión
├── SECURITY.md SUPPORT.md                       cómo informar de una vulnerabilidad y dónde preguntar
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   cómo contribuir, el código de conducta y la licencia
└── bump_manifest_version.py                     utilidad de versión del manifiesto
```

</details>

## 🗺️ Hoja de ruta y límites conocidos

**Límites conocidos hoy**

* Nada se ha ejecutado en una placa: el firmware se compila pero no se ha grabado ni probado, y los pines de la cámara y del chip de red hay que comprobarlos con el esquema de la placa.
* Los controladores del láser (VL53L1X por I2C, con el componente `grrtzm/vl53l1x_library`, poco usado), de los sensores de temperatura y de la cámara están escritos y nunca se han ejecutado: el primer sensor que responda en una placa puede mostrar un error.
* EH3D-BUNGARD-GUI (la ventana del nodo del cabezal) y EH3D-SERVER consultan el nodo, directamente o a través del servidor; ninguno se ha probado con un nodo real.
* El puerto de red es HTTP sin más: mantenlo en una red propia.
* RTSP y ONVIF no se han probado con un reproductor, grabador o gestor ONVIF reales; el modelo de la cámara de la placa decidirá los tamaños y los pines.

**Siguiente**

* Compilar el firmware y probar los controladores en una placa.
* Añadir el enlace con EH3D-BUNGARD-GUI, con EH3D-SERVER y con la pantalla, y la vista de la cámara.
* Probar la salida de protección cableada a una línea de parada en una máquina.

## 🌍 Idiomas

El README y los textos existen en inglés, español, francés, italiano, alemán, chino, japonés y portugués de Portugal.

## 🔗 Proyectos relacionados

* [EH3D-BUNGARD-GUI](https://github.com/JuanenRac/EH3D-BUNGARD-GUI): la interfaz moderna que usa las medidas para construir el mapa de la mesa
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): el nodo del puerto serie de la unidad de control que maneja la máquina
* [EH3D-NODE-BUNGARD-HMI](https://github.com/JuanenRac/EH3D-NODE-BUNGARD-HMI): la pantalla táctil del operador
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): el centro de la familia: llevará las respuestas del nodo a cada cliente
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): las partes comunes de la familia, entre ellas `eh3d-levelling` (el mapa de alturas de la mesa)

## 📚 Documentación

* Los ajustes: `head.cfg.example` lista cada clave con su valor por defecto y su rango.
* El hardware: los pines, el cableado de la salida de protección y las piezas, en `docs/HARDWARE.md`.
* [Registro de cambios](CHANGELOG.md)

## 🤝 Contribuir y comunidad

* Antes de una pull request ejecuta las pruebas en el ordenador (`./build.sh` o `build.bat`) y `python -I tools/ci_validate.py`: todo debe pasar. Mira [CONTRIBUTING.md](CONTRIBUTING.md) y el [código de conducta](CODE_OF_CONDUCT.md).
* El README existe en ocho idiomas: un cambio del texto se hace en los ocho ficheros, y los dos dibujos salen de `docs/architecture.json` y `docs/structure.json` con `tools/make_architecture.py` y `tools/make_structure.py`.
* Nada que cambie cómo el nodo da una medida o abre la salida de protección cambia sin una prueba que lo cubra y una nota de lo observado en hardware.
* Informa de una vulnerabilidad en privado: [SECURITY.md](SECURITY.md). Preguntas, ideas e informes: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

Software: GPL-3.0-or-later. Véase [LICENSE](LICENSE). Los diseños de hardware de la suite usan CERN OHL-S.
