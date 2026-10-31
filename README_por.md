<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-AUX banner" width="100%">
</p>

# 🎛️ EH3D-NODE-BUNGARD-AUX

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | 🇵🇹 <b>Português</b></p>

### Um pequeno nó na cabeça de furação de uma máquina Bungard CCD (ESP32-S3 com porta de rede por cabo): envia o vídeo da câmara, mede a altura da mesa com um laser, lê a velocidade e a temperatura do fuso e abre uma saída de proteção quando o fuso vai depressa demais ou está quente demais.

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.4-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que funciona hoje:** as regras do nó estão escritas em C++17 e testadas num computador: o ficheiro de definições com os seus valores seguros, a medição da altura da mesa a partir de várias leituras do laser (as estranhas são descartadas e uma medição que se dispersa demasiado é recusada), a velocidade do fuso a partir dos impulsos de um sensor, a temperatura com o seu aviso, o seu nível crítico e uma histerese, a saída de proteção e as palavras que o nó diz na rede (o estado, a medição, o token). O firmware ESP-IDF (rede por cabo, servidor web, câmara, impulsos do fuso, saída de proteção) está escrito e compila-se com o ESP-IDF 5.5.5, mas nunca foi gravado nem executado numa placa; **os controladores do laser (VL53L1X), dos sensores de temperatura (DS18B20, NTC, MLX90614) e da câmara (OV2640 ou OV5640) estão escritos mas nunca correram numa placa**, por isso nunca se fez uma medição, e nada foi ligado a uma máquina.

---

> **Estado de segurança (safety) e de segurança informática:** isto não é um sistema de segurança (safety). A saída de proteção é um sinal para a linha de paragem da unidade de controlo, não um substituto: a paragem que protege as pessoas e a máquina é um botão por hardware que corta o acionamento por si mesmo. A saída abre quando o fuso vai depressa demais ou está quente demais, quando um sensor de temperatura ligado não responde e quando este nó não está a funcionar; um nó sem token recusa todo o pedido que meça ou mostre a câmara. Não confie nele até a cadeia toda ter sido experimentada numa máquina real. O projeto é independente: não é feito, aprovado nem apoiado pela Bungard Elektronik, e os nomes de produto aparecem apenas para dizer com que máquinas se destina a funcionar.

## 📋 Num relance

| | |
|---|---|
| Versão | `0.0.4` |
| Maturidade | Estrutura inicial |
| Testes | 11 programas de teste no computador |
| Tecnologias | C++17, ESP-IDF 5.5, ESP32-S3, rede W5500 |
| Em hardware real | Não: o firmware compila-se mas não foi gravado, e nada foi ligado a uma máquina |

**Ir para:** [Visão geral](#-visão-geral) · [Arquitetura](#-arquitetura) · [Início rápido](#-início-rápido) · [Testes](#-testes) · [Estrutura](#-estrutura-do-repositório) · [Relacionados](#-projetos-relacionados)

## 🚦 Estado do projeto
Cada linha é uma parte deste repositório e a luz diz até onde chega hoje. É escrita com o que o repositório consegue demonstrar (testes, compilações, execuções), não com o que está previsto.

🟢 Feito e verificado · 🟡 Funciona, com limites conhecidos · 🟠 Feito a meio ou ainda por verificar · 🔴 Por fazer ou a não funcionar

| Área | Estado | O que significa aqui |
|---|---|---|
| Software | 🟠 | O projeto ESP-IDF (rede por cabo com um W5500, servidor web, câmara, impulsos do fuso, saída de proteção, o ficheiro de definições na sua própria partição) está escrito e compila-se; não foi gravado. |
| Testes | 🟢 | 11 programas de teste passam (`ctest`): as definições, a medição do laser, a velocidade e a temperatura do fuso, a saída de proteção, as palavras da rede, o vídeo (RTSP, ONVIF), o documento de definições do cabeçote, a atualização a partir do GitHub, o relógio, o que todos os nós partilham, os textos do painel em 37 línguas e as pilhas das tarefas. |
| Laser (altura da mesa) | 🟡 | A medição a partir de várias leituras (mediana, leituras estranhas descartadas, dispersão verificada, intervalo e deslocamento) está testada; o controlador do VL53L1X está escrito mas nunca correu, por isso ainda não se fez nenhuma medição. |
| Fuso | 🟡 | A velocidade a partir dos impulsos de um sensor (qualquer número por volta, parado quando os impulsos cessam), a temperatura com aviso, nível crítico e histerese e a saída de proteção estão testadas; os controladores do DS18B20, do NTC e do MLX90614 estão escritos mas nunca correram. |
| Câmara | 🟠 | O fluxo MJPEG está escrito para o conector de câmara da placa e precisa dos pinos da placa em menuconfig; não foi executado. |
| Vídeo (RTSP, ONVIF) | 🟡 | A sessão RTSP (OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, por UDP ou dentro da ligação TCP, acesso Digest e Basic), a carga RTP de JPEG, a descoberta e os serviços SOAP do ONVIF (perfis, codificadores com os seus tamanhos, endereços do fluxo e do instantâneo, o token de utilizador) estão testados num computador; nenhum leitor nem gravador recebeu o vídeo de uma placa real. |
| Hardware real | 🔴 | Nenhuma placa executou este firmware e nada foi ligado a uma máquina. |
| Documentação | 🟢 | README em oito idiomas, a referência das definições e as notas de hardware. |

## 🎯 Visão geral

* A **medição do laser**: o nó pede ao sensor várias leituras, descarta as estranhas (mediana e desvio absoluto mediano), recusa uma medição que se disperse mais de `laser_sigma_max_mm` ou que fique fora do intervalo, e responde com a distância à mesa e a dispersão; os programas de secretária transformam-na num ponto do mapa da mesa (`Z x y altura`, lido por `eh3d-levelling`).
* O **fuso**: a velocidade vem dos impulsos de um sensor (um sensor hall ou um disco ranhurado, qualquer número de impulsos por volta) e é zero quando os impulsos cessam; a temperatura tem um aviso, um nível crítico e uma histerese para que o estado não pisque; um sensor ligado que não responde conta como quente.
* A **saída de proteção**: um GPIO que fica em baixo sempre que o fuso deve parar (depressa demais, quente demais, um sensor mudo) e também enquanto o nó não está a funcionar, para ligar à linha de paragem da unidade de controlo por um relé ou um optoacoplador; pode ser desligada nas definições.
* A **câmara e a rede**: o vídeo da câmara como MJPEG em `/stream` e as leituras em `/api/status`, por uma porta de rede por cabo (W5500 sobre SPI); o token protege a medição e a câmara.
* As **definições**: cada valor afinado (a rede, a câmara, o laser, o fuso, a temperatura, a saída de proteção) é uma definição do nó, guardada na flash e editada no painel web, nunca um número no código; um valor fora do intervalo é recusado com o motivo.
* O **vídeo como o envia uma câmara IP**: RTSP (`rtsp://ENDEREÇO/stream1`, JPEG sobre RTP, por UDP ou dentro da ligação TCP, com acesso Digest ou Basic) e ONVIF (WS-Discovery, para que gravadores e gestores encontrem o nó, e os serviços do perfil S: perfis, codificadores com os tamanhos que a câmara consegue fazer, o endereço do fluxo e do instantâneo, o token de utilizador); os dois usam o utilizador e a palavra-passe das definições, e sem palavra-passe nada é servido.
* **O painel e a atualização, como os outros nós:** o nó tem o painel web dos nós do A.R.M.O.R. e do EH3D-NODE-BUNGARD (o menu em grupos com o idioma e o fim de sessão ao fundo; um Resumo; uma página Wi-Fi com até três redes de reserva para quando a principal cai; uma página Cabeçote que edita as definições da câmara, das transmissões, do laser e do fuso; Ajuda; Acerca de; utilizadores; o registo), a cópia de um slot de firmware para o outro nos dois sentidos e a atualização a partir da última release do GitHub deste projeto (verifica-se o SHA-256 que a release publica, o endereço tem de ser o do próprio projeto e só um administrador a pede; as releases não estão assinadas). Partilha a sua base de firmware com o EH3D-NODE-BUNGARD.

## 🧩 Papel no EH3D

EH3D-NODE-BUNGARD-AUX é o nó da cabeça de furação de uma máquina Bungard CCD, ao lado do EH3D-NODE-BUNGARD (o nó que comanda a máquina) e do EH3D-NODE-BUNGARD-HMI (o ecrã). Fica na cabeça, vê a placa com a sua câmara, mede a mesa por baixo da cabeça e vigia o fuso. Os programas de secretária (EH3D-BUNGARD-GUI, DRILLPRO, ROUTEPRO) usam as suas medições para construir o mapa da mesa (`eh3d-levelling` no EH3D-COMMON) e corrigir o Z dos trabalhos; o EH3D-BUNGARD-GUI consulta o nó diretamente ou através do EH3D-SERVER (a janela do nó da cabeça).

## 🏛️ Arquitetura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitetura de EH3D-NODE-BUNGARD-AUX: quem o usa, o que contém e de que depende" width="100%">
</p>

Como as partes deste repositório se encaixam: quem fala com ele, o que contém e de que depende. O desenho sai de `docs/architecture.json` e é verificado contra o código.

## 🖼️ Capturas de ecrã

<p align="center">
  <img src="images/screenshots/answers.png" alt="o que responde o nó" width="100%">
</p>

<p align="center"><em>O que o nó responde na rede: o estado (`GET /api/status`), uma medição da mesa (`POST /api/measure`) e a linha que os programas de secretária leem como um ponto do mapa. Um desenho dos formatos que os testes verificam, não uma captura de uma placa.</em></p>

<p align="center">
  <img src="images/screenshots/panel-overview.png" alt="panel-overview" width="100%">
</p>

<p align="center"><em>A página Resumo: o nó, o seu hardware, o seu relógio, a sua flash com os dois slots de firmware e a sua rede.</em></p>

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>A página Estado, no tema Forge dos outros programas EH3D.</em></p>

<p align="center">
  <img src="images/screenshots/panel-head.png" alt="panel-head" width="100%">
</p>

<p align="center"><em>A página Cabeçote: a câmara, as transmissões, o laser, os sensores do fuso e a saída de proteção (as palavras-passe e o token são só de escrita).</em></p>

<p align="center">
  <img src="images/screenshots/panel-wifi.png" alt="panel-wifi" width="100%">
</p>

<p align="center"><em>A página Wi-Fi: a rede a que o nó se liga e até três redes de reserva para quando se perde.</em></p>

<p align="center">
  <img src="images/screenshots/panel-update.png" alt="panel-update" width="100%">
</p>

<p align="center"><em>A página Firmware: a atualização a partir do GitHub, o envio de um ficheiro e os dois slots, com a cópia de um sobre o outro.</em></p>

<p align="center">
  <img src="images/screenshots/panel-help.png" alt="panel-help" width="100%">
</p>

<p align="center"><em>A página Ajuda: um tema para cada página do painel.</em></p>

## ✨ Funcionalidades

* **Uma medição de confiança, ou nenhuma:** várias leituras, as estranhas descartadas, e uma recusa (com o motivo) quando as leituras se dispersam demasiado, estão fora do intervalo ou são poucas.
* **Um fuso vigiado o tempo todo:** velocidade a partir dos impulsos, temperatura com um aviso, um nível crítico e uma histerese, e uma saída de proteção que abre sozinha quando algo corre mal ou o nó não está a funcionar.
* **Vídeo como uma câmara IP:** RTSP e ONVIF com acesso, para que qualquer leitor, gravador ou gestor ONVIF o receba; e a câmara como MJPEG em `/stream` atrás do token.
* **Seguro por omissão:** sem token não há medição nem câmara; cada limite tem um interruptor nas definições e começa na sua posição conservadora.
* **Duas câmaras, a OV2640 e a OV5640:** o sensor diz qual é e o nó dá o maior tamanho que as definições permitem e que esse sensor consegue (a OV2640 até 1600x1200, a OV5640 até 2592x1944), indica-o em `/api/status` (`camera_model`) e nas opções do codificador do ONVIF; `camera_model = auto | ov2640 | ov5640` em a página Cabeçote. Os pinos do conector de câmara da Waveshare ESP32-S3-ETH são os valores por omissão de `menuconfig`.
* **Três sensores de temperatura:** um DS18B20 num fio, um termístor NTC numa entrada analógica (`ntc_series_ohm`, `ntc_r25_ohm`, `ntc_beta` em a página Cabeçote) ou um termómetro de infravermelhos MLX90614 que olha para o fuso sem lhe tocar (no barramento I2C do laser); as somas de verificação e as conversões estão testadas num computador.
* **Um anel de LEDs à volta da câmara:** oito LEDs RGB do tipo WS2812 num só fio iluminam a zona sob a cabeça para a câmara ver bem. O branco é a luz para a câmara; os níveis seguem o olho e nunca passam de um limite; um só lado dá luz rasante; pode pedir-se qualquer cor, e o anel diz como correu uma verificação (verde, vermelho a piscar, um ponto azul a girar, âmbar, vermelho rápido). Apaga-se sozinho ao fim de algum tempo. `GET` e `POST /api/light` e `GET /api/snapshot` servem os programas; as definições estão no painel e em `head.cfg` (`light_*`), e o pino no menu do firmware (`AUX_LIGHT_GPIO`).
* **37 línguas:** o painel web do nó fala inglês, espanhol, italiano, alemão, francês, português, chinês, japonês, búlgaro, croata, checo, dinamarquês, neerlandês, estónio, finlandês, grego, húngaro, irlandês, letão, lituano, maltês, polaco, romeno, eslovaco, esloveno, sueco, norueguês, islandês, ucraniano, russo, sérvio, albanês, catalão, galego, basco, português do Brasil e francês do Canadá. As palavras das 29 línguas além das oito primeiras foram traduzidas pelo modelo e aguardam a revisão de falantes nativos; um texto que falte numa língua aparece em inglês.
* **O laser, ajustado ao trabalho:** o VL53L1X tem modo de distância (curto ou longo) e orçamento de tempo, uma área de receção que se pode estreitar para olhar um ponto pequeno, um filtro que descarta leituras longe da mediana, um estado para cada leitura (válida, sinal fraco, dispersão alta, fora de alcance) e uma calibração a distância conhecida (desvio e diafonia) que um administrador tem de ativar primeiro. Ainda não experimentado num sensor.
* **Vários DS18B20 no mesmo fio:** o nó procura no fio todos os sensores (com o CRC de cada endereço), usa o do fuso pelo seu endereço (`temp_rom`, ou o único, ou o primeiro encontrado com um aviso na página de estado), define a resolução de 9 a 12 bits (`temp_resolution`) e espera só o que essa resolução precisa. Testado num computador com um bus simulado; ainda não experimentado num fio real.
* **Um sensor de velocidade que distingue o ruído das voltas:** o TCRT5000 (refletivo, com a sua marca no fuso) funciona com a saída digital por interrupção, ou com a saída analógica lida por um ADC rápido e um disparador Schmitt cujos limiares seguem o sinal; filtram-se os impulsos mais rápidos do que o fuso pode rodar, os picos a meio de uma volta e os impulsos perdidos, e o estado diz se os impulsos são limpos ou ruidosos. Testado com comboios de impulsos simulados a 400 Hz e 1 kHz; ainda não experimentado num fuso.

## 🔒 Segurança e proteção

| Ameaça | Defesa |
|---|---|
| Um fuso demasiado rápido ou quente | a saída de proteção abre acima de `rpm_max` e à temperatura crítica, com uma histerese para não piscar; um sensor de temperatura ligado e mudo conta como quente |
| O nó perde a alimentação ou bloqueia | a saída fica em baixo até o nó ter verificado o fuso e continua em baixo enquanto não está a funcionar, por isso deve ser ligada como uma paragem que abre sozinha |
| Alguém na rede mede ou vê a oficina | sem token o nó recusa a medição e a câmara; o token compara-se em tempo constante e nunca é mostrado nem registado |
| Uma má medição tomada por boa | as leituras estranhas são descartadas, uma dispersão acima de `laser_sigma_max_mm`, o intervalo ou poucas leituras recusam a medição com o motivo |
| Um erro ao escrever as definições | um valor fora do intervalo ou uma chave desconhecida é avisado e fica o valor seguro; uma temperatura crítica abaixo do aviso, ou um intervalo ao contrário, volta aos valores predefinidos |
| Um segredo no firmware ou no repositório | nada secreto é compilado; o token é uma definição na sua partição |

**O que não é defendido:** o nó não é um dispositivo de segurança. A saída de proteção vale o que valer a cablagem que a leva à linha de paragem, e nada disto foi experimentado numa máquina. A porta de rede não tem cifragem (uma simples porta HTTP na rede da oficina): mantenha-a numa rede própria.

## 🔌 Interfaces

| Interface | O que é | Onde |
|---|---|---|
| Definições | um documento na flash, editado na página Cabeçote do painel web; as chaves são as de `head.cfg.example` | `core/bungard_config.hpp`, `core/aux_config.hpp` |
| Estado | `GET /api/status`: o nó, a câmara, o laser, a velocidade, a temperatura, a saída de proteção e o tempo ligado numa linha de JSON | `core/protocol.hpp` |
| Medição | `POST /api/measure[?samples=N]` com o token: a distância à mesa, a dispersão e porque uma medição foi recusada | `core/laser.hpp`, `main/app_main.cpp` |
| Câmara | `GET /stream` com o token: MJPEG | `main/app_main.cpp` |
| Impulsos do fuso | um GPIO (`AUX_RPM_GPIO` em menuconfig), flancos de subida | `main/sensors.cpp` |
| Saída de proteção | um GPIO (`AUX_PROTECT_GPIO`), em baixo quando o fuso deve parar | `main/sensors.cpp` |
| Laser e temperatura | I2C para o laser e um fio para o sensor de temperatura; os controladores estão escritos e nunca correram (o termómetro MLX90614 partilha o barramento I2C, o DS18B20 usa um pino, o NTC uma entrada analógica) | `main/sensors.cpp` |
| RTSP | `rtsp://ENDEREÇO:PORTA/stream1`: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, GET_PARAMETER, TEARDOWN; JPEG sobre RTP por UDP ou intercalado em TCP; acesso Digest ou Basic | `core/rtsp.hpp`, `core/rtsp_session.hpp`, `core/rtp_jpeg.hpp`, `main/stream.cpp` |
| ONVIF | WS-Discovery no endereço multicast `239.255.255.250:3702`; SOAP em `/onvif/device_service` e `/onvif/media_service` (perfil S: capacidades, serviços, perfis, codificadores, endereços do fluxo e do instantâneo); o instantâneo em `/snapshot.jpg` | `core/onvif.hpp`, `main/stream.cpp` |

## ⚙️ Configuração

As definições guardam-se na flash como um único documento e editam-se na página Cabeçote do painel web (as palavras-passe e o token são só de escrita: o painel nunca os mostra). As chaves e os seus intervalos são os de `head.cfg.example`; o ficheiro a página Cabeçote das versões anteriores já não é lido.

| Definição | Predefinido | Intervalo | O que faz |
|---|---|---|---|
| `token` | none | 8 - 64 | o token de acesso; sem ele o nó recusa a medição e a câmara |
| `laser_samples` | `7` | 1 - 31 | leituras de uma medição (a mediana das boas) |
| `laser_offset_mm` | `0` | -50 - 50 | soma-se a cada distância: onde o sensor está acima da ponta da ferramenta |
| `laser_sigma_max_mm` | `0.15` | 0.01 - 5 | uma medição cujas leituras se dispersem mais do que isto é recusada |
| `rpm_pulses_per_rev` | `1` | 1 - 64 | impulsos do sensor por cada volta do fuso |
| `rpm_max` | `65000` | 1000 - 200000 | acima desta velocidade abre a saída de proteção |
| `temp_sensor` | none | none ds18b20 ntc mlx90614 | o sensor da temperatura do fuso |
| `temp_warn_c / temp_critical_c` | `55 / 70` | 20 - 200 | o aviso e o nível a que abre a saída de proteção |
| `protect_output` | `on` | on / off | a saída de proteção funciona; desligue só na bancada |
| `stream_user / stream_password` | `viewer / none` | 8 - 64 | o acesso de RTSP e ONVIF; sem palavra-passe não servem ninguém |
| `rtsp_port` | `554` | 1 - 65535 | a porta do servidor RTSP (a do servidor web é `port`) |

## 🚀 Início rápido

Precisa de um compilador C++17 e do CMake para os testes no computador, e do ESP-IDF 5.5.5 para o firmware (o alvo ESP32-S3).

```bash
build.bat            # os testes no computador (Windows)  -  ./build.sh em Linux
python -I tools/ci_validate.py
```

O firmware compila-se com o ESP-IDF 5.5.5: `idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` gera a imagem (defina os pinos da sua placa em `EH3D Bungard AUX`), e as definições fazem-se no painel web quando o nó está a funcionar.

## 🧪 Testes

Os testes no computador (`build.bat` ou `./build.sh`, depois `ctest`) são dois programas e tudo passa: `test_aux` (103 verificações) sobre o ficheiro de definições (valores predefinidos, valores, valores errados, limites que não fazem sentido juntos), a medição do laser (a mediana, a leitura estranha descartada, o deslocamento, leituras marcadas como más, o intervalo, nenhuma leitura, uma dispersão demasiado grande), a velocidade do fuso (impulsos por volta, um fuso que abranda, parado quando os impulsos cessam), a temperatura (a histerese, um sensor que falha), a saída de proteção e as palavras da rede (a linha de pedido, as rotas, o token no cabeçalho ou na consulta, o número de leituras, o JSON do estado e da medição, a linha de um ponto do mapa). O firmware não está coberto: compila-se, mas os testes correm no computador. O segundo programa, `test_stream` (100 verificações), cobre o vídeo: os resumos dos acessos contra os seus vetores publicados (MD5, SHA-1, base64 e o exemplo Digest do RFC 2617), os pedidos RTSP e toda a sessão (acesso, SDP, transporte UDP e intercalado, play, pause, teardown, uma sessão errada, multicast recusado), a carga RTP de JPEG (as tabelas no primeiro pacote, a ordem, o marcador, a volta da sequência) e ONVIF (descoberta, o token de utilizador com o exemplo da norma, as chamadas abertas e as protegidas, os serviços e uma chamada que não se faz). Além desses dois programas, o `test_head_settings` lê, verifica e escreve a secção do cabeçote do documento de definições como o painel o faz (as palavras-passe e o token continuam só de escrita), e os testes partilhados da base do firmware cobrem a atualização a partir do GitHub, o relógio e os utilizadores.

## 🏗️ Compilação e publicação

A versão está em `eh3d.project.json`, e cada versão tem a sua entrada em [CHANGELOG.md](CHANGELOG.md). `build.sh` / `build.bat` compilam e executam os testes no computador e `test.sh` / `test.bat` executam as verificações do repositório. O firmware compila-se com o ESP-IDF 5.5.5 (`idf.py build`), o que ainda não foi feito; nenhuma versão foi publicada.

## 🩺 Resolução de problemas

| Sintoma | O que fazer |
|---|---|
| Uma medição é recusada com `no_readings` | Nenhum VL53L1X responde no barramento I2C: verifique os pinos SDA e SCL em `menuconfig` (a câmara usa os GPIO 41 e 42 na Waveshare ESP32-S3-ETH) e a cablagem; o resto da resposta está testado. |
| Uma medição é recusada com `too_noisy` | As leituras dispersam-se mais do que `laser_sigma_max_mm`: deixe a cabeça e a mesa assentarem, ou suba um pouco o limite; verifique que nada vibra. |
| O nó responde 401 a uma medição ou à câmara | Defina `token` em a página Cabeçote (8 a 64 caracteres) e envie-o no cabeçalho `Authorization: Bearer ...` ou em `?token=`. |
| A saída de proteção está sempre aberta | Um sensor de temperatura ligado que não responde conta como quente: verifique o seu fio e o seu pino em `menuconfig` e `temp_sensor` em a página Cabeçote, ou ponha `temp_sensor = none` na bancada. |

## 📂 Estrutura do repositório

<p align="center">
  <img src="images/repository-structure.svg" alt="Estrutura do repositório EH3D-NODE-BUNGARD-AUX" width="100%">
</p>

<details>
<summary>Versão em texto da árvore</summary>

```text
EH3D-NODE-BUNGARD-AUX/
├── core/
│   ├── aux_config.hpp                           o ficheiro de definições e os seus valores seguros
│   ├── bungard_config.hpp                       as definições do nó e a secção do cabeçote
│   ├── laser.hpp                                a medição da altura da mesa
│   ├── laser_hw.hpp                             os registos do laser
│   ├── light.hpp                                o anel de LEDs: níveis, cores, sinais
│   ├── camera.hpp                               as câmaras OV2640 e OV5640 e os seus tamanhos
│   ├── spindle.hpp                              o sensor de velocidade: impulsos, ruído, Schmitt
│   ├── thermo.hpp                               a temperatura: DS18B20, NTC, MLX90614
│   ├── onewire.hpp                              o bus 1-Wire: pesquisa, endereço, resolução
│   ├── hash.hpp                                 os acessos: MD5, SHA-1, base64
│   ├── rtsp.hpp rtsp_session.hpp rtp_jpeg.hpp   RTSP e a carga RTP de JPEG
│   ├── onvif.hpp                                ONVIF: descoberta e serviços SOAP
│   ├── protocol.hpp                             as palavras da rede: rotas, token, JSON
│   ├── node_settings.hpp auth.hpp               definições, utilizadores (base)
│   ├── netplan.hpp json.hpp                     plano de rede e JSON (base)
│   └── release_assets.hpp semver.hpp            qual ficheiro de uma versão é a imagem desta placa
├── main/
│   ├── app_main.cpp                             arranque, definições, rede, painel
│   ├── head.cpp head.hpp                        câmara, laser, fuso, rotas
│   ├── light.cpp light.hpp                      o controlador do anel de LEDs
│   ├── sensors.cpp sensors.hpp                  os pinos: impulsos, saída de proteção, sensores
│   ├── stream.cpp stream.hpp                    servidor RTSP, serviços ONVIF e descoberta
│   ├── idf_component.yml Kconfig.projbuild      lista de componentes e pinos de menuconfig
│   ├── web_server.cpp network.cpp               servidor do painel, rede (base)
│   ├── node_store.cpp node_guard.cpp            armazenamento, guarda (base)
│   └── github_update.cpp entropy.cpp            a atualização a partir do GitHub e a cópia de um slot
├── panel/
│   ├── index.html style.css                     a página e a disposição do menu lateral
│   ├── app.js text.js                           as páginas e os textos em 37 línguas
│   └── locales/                                 as palavras de mais 29 línguas, um ficheiro cada
├── tests/                                       testes no computador das definições, da comporta, da voz e dos textos de ecrã
├── docs/                                        as notas de hardware e as fontes dos dois desenhos
├── tools/                                       verificação de CI, de README e ferramentas dos desenhos
├── CMakeLists.txt                               o projeto ESP-IDF
├── partitions.csv                               a divisão da flash com duas ranhuras
├── head.cfg.example                             cada definição com o seu valor por omissão e intervalo
├── sdkconfig.defaults                           definições do ESP-IDF comuns às duas placas
├── sdkconfig.board.s3-eth                       definições da placa com cabo
├── sdkconfig.board.s3-wifi                      definições da placa Wi-Fi
├── dependencies.lock                            as versões exatas dos componentes
├── images/                                      banner e as imagens de Arquitetura e Estrutura do repositório
├── .github/                                     fluxos de CI e de publicação, modelos de issues e pull requests
├── build.sh build.bat                           compila e executa os testes no computador num painel em direto
├── eh3d.project.json                            manifesto: versão, papel, maturidade e comando de compilação
├── CHANGELOG.md                                 o que mudou em cada versão
├── SECURITY.md SUPPORT.md                       como comunicar uma vulnerabilidade e onde perguntar
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   como contribuir, o código de conduta e a licença
└── bump_manifest_version.py                     utilitário de versão do manifesto
```

</details>

## 🗺️ Roteiro e limites conhecidos

**Limites conhecidos hoje**

* Nada foi executado numa placa: o firmware compila-se mas não foi gravado nem experimentado, e os pinos da câmara e do chip de rede têm de ser verificados com o esquema da placa.
* Os controladores do laser (VL53L1X por I2C, com o componente `grrtzm/vl53l1x_library`, pouco usado), dos sensores de temperatura e da câmara estão escritos e nunca correram: o primeiro sensor que responder numa placa pode mostrar um erro.
* o EH3D-BUNGARD-GUI (a janela do nó da cabeça) e o EH3D-SERVER consultam o nó, diretamente ou através do servidor; nenhum foi experimentado com um nó real.
* A porta de rede é HTTP simples: mantenha-a numa rede própria.
* RTSP e ONVIF não foram experimentados com um leitor, gravador ou gestor ONVIF reais; o modelo da câmara na placa decidirá os tamanhos e os pinos.

**Seguinte**

* Compilar o firmware e experimentar os controladores numa placa.
* Acrescentar a ligação com o EH3D-BUNGARD-GUI, com o EH3D-SERVER e com o ecrã, e a vista da câmara.
* Experimentar a saída de proteção ligada a uma linha de paragem numa máquina.

## 🌍 Idiomas

O README e os textos existem em inglês, espanhol, francês, italiano, alemão, chinês, japonês e português europeu.

## 🔗 Projetos relacionados

* [EH3D-BUNGARD-GUI](https://github.com/JuanenRac/EH3D-BUNGARD-GUI): a interface moderna que usa as medições para construir o mapa da mesa
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): o nó da porta série da unidade de controlo que comanda a máquina
* [EH3D-NODE-BUNGARD-HMI](https://github.com/JuanenRac/EH3D-NODE-BUNGARD-HMI): o ecrã tátil do operador
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): o centro da família: levará as respostas do nó a cada cliente
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): as partes comuns da família, entre elas `eh3d-levelling` (o mapa de alturas da mesa)

## 📚 Documentação

* As definições: `head.cfg.example` lista cada chave com o seu valor predefinido e o seu intervalo.
* O hardware: os pinos, a cablagem da saída de proteção e as peças, em `docs/HARDWARE.md`.
* [Registo de alterações](CHANGELOG.md)

## 🤝 Contribuir e comunidade

* Antes de um pull request executa os testes no computador (`./build.sh` ou `build.bat`) e `python -I tools/ci_validate.py`: tudo tem de passar. Vê [CONTRIBUTING.md](CONTRIBUTING.md) e o [código de conduta](CODE_OF_CONDUCT.md).
* O README existe em oito idiomas: uma alteração do texto faz-se nos oito ficheiros, e os dois desenhos saem de `docs/architecture.json` e `docs/structure.json` com `tools/make_architecture.py` e `tools/make_structure.py`.
* Nada que mude a forma como o nó dá uma medição ou abre a saída de proteção muda sem um teste que o cubra e uma nota do que foi observado em hardware.
* Comunica uma vulnerabilidade em privado: [SECURITY.md](SECURITY.md). Perguntas, ideias e relatórios: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENÇA

Software: GPL-3.0-or-later. Ver [LICENSE](LICENSE). Os projetos de hardware da suite usam a CERN OHL-S.
