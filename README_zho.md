<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD-AUX banner" width="100%">
</p>

# 🎛️ EH3D-NODE-BUNGARD-AUX

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | 🇨🇳 <b>中文</b> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### 安装在 Bungard CCD 机床钻头上的小型节点（带有线网口的 ESP32-S3）：发送摄像头视频，用激光测量工作台高度，读取主轴转速和温度，并在主轴过快或过热时断开保护输出。

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.8-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**诚实性核对 - 今天真正能运行的内容：** 节点的规则用 C++17 编写并在电脑上测试：带安全默认值的设置文件、由多次激光读数得出的工作台高度测量（异常值会被丢弃，离散过大的测量会被拒绝）、由传感器脉冲得出的主轴转速、带警告、临界值和回差的温度、保护输出，以及节点在网络上说的话（状态、测量、令牌）。ESP-IDF 固件（有线网络、网页服务器、摄像头、主轴脉冲、保护输出）已写好并能用 ESP-IDF 5.5.5 编译，但从未烧录或在板子上运行；**激光（VL53L1X）、温度传感器（DS18B20、NTC、MLX90614）和摄像头（OV2640 或 OV5640）的驱动已编写，但从未在板子上运行过**，所以从未做过任何测量，也没有连接过任何机床。

---

> **安全（safety）与信息安全状态：** 这不是安全系统。保护输出是给控制单元停机线路的信号，并不能取代它：保护人员和机床的急停是能自行切断驱动的硬件按钮。主轴过快或过热、已启用的温度传感器无响应、或本节点未运行时，输出会断开；没有令牌的节点会拒绝一切测量或查看摄像头的请求。在整个链路于真实机床上试过之前不要依赖它。本项目是独立的：并非由 Bungard Elektronik 制造、认可或支持，出现产品名称只是为了说明它打算配合哪些机床工作。

## 📋 速览

| | |
|---|---|
| 版本 | `0.0.8` |
| 成熟度 | 脚手架 |
| 测试 | 14 个电脑端测试程序 |
| 技术栈 | C++17、ESP-IDF 5.5、ESP32-S3、W5500 网络 |
| 真实硬件 | 否：固件能编译但尚未烧录，也没有连接过任何机床 |

**跳转到：** [概述](#-概述) · [架构](#-架构) · [快速开始](#-快速开始) · [测试](#-测试) · [结构](#-仓库结构) · [相关项目](#-相关项目)

## 🚦 项目状态
每一行是本仓库的一个部分，灯号表示它今天做到了哪一步。内容依据仓库能够展示的证据（测试、构建、运行），而不是计划。

🟢 已完成并验证 · 🟡 可用，有已知限制 · 🟠 部分完成或尚未验证 · 🔴 未完成或不可用

| 领域 | 状态 | 在这里的含义 |
|---|---|---|
| 软件 | 🟠 | ESP-IDF 工程（基于 W5500 的有线网络、网页服务器、摄像头、主轴脉冲、保护输出、独立分区中的设置文件）已编写并能编译，但尚未烧录。 |
| 测试 | 🟡 | 14 个测试程序通过（`ctest`）：设置、激光测量、主轴转速和温度、保护输出、网络报文、视频（RTSP、ONVIF）、钻头设置文档、从 GitHub 更新、时钟、所有节点共有的部分、面板37 种语言的文本以及任务栈。 |
| 激光（工作台高度） | 🟡 | 由多次读数得出的测量（中位数、丢弃异常读数、检查离散度、量程和偏移）已测试；VL53L1X 的驱动已编写但从未运行，因此尚未做过任何测量。 |
| 主轴 | 🟡 | 由传感器脉冲得出的转速（每转任意脉冲数，脉冲停止即视为停转）、带警告、临界值和回差的温度以及保护输出已测试；DS18B20、NTC 和 MLX90614 的驱动已编写但从未运行。 |
| 摄像头 | 🟠 | MJPEG 视频流已按板子的摄像头接口编写，需要在 menuconfig 中填写板子的引脚；尚未运行。 |
| 视频（RTSP、ONVIF） | 🟡 | RTSP 会话（OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、TEARDOWN，走 UDP 或 TCP 连接内，Digest 与 Basic 登录）、JPEG 的 RTP 载荷、ONVIF 的发现与 SOAP 服务（配置文件、带尺寸的编码器、流与快照地址、用户令牌）已在电脑上测试；还没有播放器或录像机从真实板子取过视频。 |
| 真实硬件 | 🔴 | 没有板子运行过这份固件，也没有连接过任何机床。 |
| 文档 | 🟡 | 八种语言的 README、设置参考和硬件说明。 |

## 🎯 概述

* **激光测量**：节点向传感器取多次读数，丢弃异常值（中位数与绝对中位差），拒绝离散度超过 `laser_sigma_max_mm` 或超出量程的测量，并返回到工作台的距离及离散度；桌面程序把它变成工作台高度图上的一个点（`Z x y 高度`，由 `eh3d-levelling` 读取）。
* **主轴**：转速来自传感器的脉冲（霍尔传感器或开槽盘，每转任意脉冲数），脉冲停止时为零；温度有警告、临界值和回差，状态不会来回闪烁；已启用但无响应的传感器视为过热。
* **保护输出**：一个 GPIO，只要主轴必须停止（过快、过热、传感器无响应）或节点未运行，就保持低电平，可通过继电器或光耦接入控制单元的停机线路；可在设置中关闭。
* **protect 输出受到监视：** 写入它的任务已订阅任务看门狗，并列在节点守卫期望的任务中，因此卡死时芯片会重启，引脚恢复为输入，停止线随之断开，而不是让输出一直保持开启。这些都没有在电路板上运行过。
* **摄像头与网络**：摄像头视频以 MJPEG 提供在 `/stream`，读数在 `/api/status`，通过有线网口（SPI 上的 W5500）；令牌保护测量和摄像头。
* **设置**：每个调校值（网络、摄像头、激光、主轴、温度、保护输出）都是节点的设置，保存在闪存中并在网页面板编辑，绝不是代码里的数字；超出范围的值会连同原因一起被拒绝。
* **像 IP 摄像头一样发送视频**：RTSP（`rtsp://地址/stream1`，JPEG over RTP，走 UDP 或 TCP 连接内，Digest 或 Basic 登录）和 ONVIF（WS-Discovery 让录像机和管理软件找到节点，以及 S 配置文件的服务：配置文件、带摄像头可用尺寸的编码器、流与快照地址、用户令牌）；两者都用设置里的用户名和密码，没有密码就不提供任何服务。
* **面板与更新，与其他节点一致：** 节点具有 A.R.M.O.R. 节点和 EH3D-NODE-BUNGARD 的网页面板（分组菜单，底部有语言选择和退出登录；概览页；最多三个备用网络的 Wi-Fi 页，主网络断开时使用；编辑摄像头、视频流、激光和主轴设置的“钻头”页；帮助；关于；用户；日志），支持把一个固件槽复制到另一个（双向），以及从本项目最新的 GitHub 发布版更新（校验发布版公布的 SHA-256，地址必须是项目自己的，且只有管理员才能发起；发布版未签名）。它与 EH3D-NODE-BUNGARD 共用固件基础。

## 🧩 在 EH3D 中的角色

EH3D-NODE-BUNGARD-AUX 是 Bungard CCD 机床钻头上的节点，与 EH3D-NODE-BUNGARD（驱动机床的节点）和 EH3D-NODE-BUNGARD-HMI（屏幕）并列。它装在钻头上，用摄像头看板子，测量钻头下方的工作台并监视主轴。桌面程序（EH3D-BUNGARD-GUI、DRILLPRO、ROUTEPRO）用它的测量结果构建工作台高度图（EH3D-COMMON 中的 `eh3d-levelling`）并修正作业的 Z；EH3D-BUNGARD-GUI 可直接或经 EH3D-SERVER 查询该节点（钻头节点窗口）。

## 🏛️ 架构

<p align="center">
  <img src="images/architecture.svg" alt="EH3D-NODE-BUNGARD-AUX 的架构：谁在使用、包含什么、依赖什么" width="100%">
</p>

本仓库各部分如何配合：谁与它通信、它包含什么、依赖什么。图由 `docs/architecture.json` 生成，并与代码核对。

## 🖼️ 屏幕截图

<p align="center">
  <img src="images/screenshots/answers.png" alt="节点的回答" width="100%">
</p>

<p align="center"><em>节点在网络上的回答：状态（`GET /api/status`）、一次工作台测量（`POST /api/measure`），以及桌面程序读作高度图一个点的那一行。这是测试所检查格式的示意图，不是板子上的截图。</em></p>

<p align="center">
  <img src="images/screenshots/panel-overview.png" alt="panel-overview" width="100%">
</p>

<p align="center"><em>“概览”页面：节点、硬件、时钟、带两个固件槽的闪存和网络。</em></p>

<p align="center">
  <img src="images/screenshots/panel-status.png" alt="panel-status" width="100%">
</p>

<p align="center"><em>“状态”页面，使用与其他 EH3D 程序相同的 Forge 主题。</em></p>

<p align="center">
  <img src="images/screenshots/panel-head.png" alt="panel-head" width="100%">
</p>

<p align="center"><em>“钻头”页面：摄像头、视频流、激光、主轴传感器和保护输出（密码和令牌为只写）。</em></p>

<p align="center">
  <img src="images/screenshots/panel-wifi.png" alt="panel-wifi" width="100%">
</p>

<p align="center"><em>“Wi-Fi”页面：节点加入的网络，以及断开时使用的最多三个备用网络。</em></p>

<p align="center">
  <img src="images/screenshots/panel-update.png" alt="panel-update" width="100%">
</p>

<p align="center"><em>“固件”页面：从 GitHub 更新、上传文件，以及两个槽及其相互复制。</em></p>

<p align="center">
  <img src="images/screenshots/panel-help.png" alt="panel-help" width="100%">
</p>

<p align="center"><em>“帮助”页面：面板每个页面各有一个主题。</em></p>

## ✨ 功能

* **要么测量可信，要么不测量：** 多次读数、丢弃异常值，读数离散过大、超出量程或太少时拒绝（并给出原因）。
* **时刻被监视的主轴：** 由脉冲得出转速，温度带警告、临界值和回差，出问题或节点未运行时保护输出会自行断开。
* **像 IP 摄像头一样的视频：** 带登录的 RTSP 和 ONVIF，任何播放器、录像机或 ONVIF 管理软件都能取用；摄像头还以 MJPEG 提供在需要令牌的 `/stream`。
* **默认安全：** 没有令牌就没有测量和摄像头；每个限值在设置里都有开关，并从保守位置开始。
* **两种摄像头，OV2640 和 OV5640：** 传感器会说明自己是哪一种，节点给出设置允许且该传感器能做到的最大尺寸（OV2640 最大 1600x1200，OV5640 最大 2592x1944），并在 `/api/status`（`camera_model`）和 ONVIF 编码器选项中报告；“钻头”页 中的 `camera_model = auto | ov2640 | ov5640`。Waveshare ESP32-S3-ETH 摄像头接口的引脚是 `menuconfig` 的默认值。
* **三种温度传感器：** 单总线的 DS18B20、接在模拟输入上的 NTC 热敏电阻（“钻头”页 中的 `ntc_series_ohm`、`ntc_r25_ohm`、`ntc_beta`），或不接触主轴就能测温的红外测温仪 MLX90614（接在激光的 I2C 总线上）；校验和与换算已在电脑上测试。
* **摄像头周围的 LED 环形灯：** 一根数据线上的八颗 WS2812 型 RGB LED 照亮钻头下方区域，让摄像头看得清楚。白光是供摄像头使用的光；亮度按人眼感受变化，且不会超过上限；只点亮一侧可得到斜射光；可指定任意颜色，环形灯还用颜色显示检查结果（绿色、红色闪烁、旋转的蓝点、琥珀色、快速红色）。一段时间后会自动熄灭。`GET` 和 `POST /api/light` 以及 `GET /api/snapshot` 为程序提供服务；设置在面板和 `head.cfg`（`light_*`）中，引脚在固件菜单（`AUX_LIGHT_GPIO`）中。
* **会自我判断的节点，以及并不存在的钻头：** 状态里带有一个结论（`selfcheck`：ok、warn、fail）和给出原因的代码（`selfcheck_codes`：摄像头未启动、激光故障或读数过旧、转速脉冲有噪声、主轴偏热或过热、温度传感器无响应、没有令牌等）；其阈值是 `head.cfg` 中的 `selfcheck_*` 设置。`core/head_sim.hpp` 是一个模拟的钻头（不平的工作台、会加速并发热的主轴、可打开的故障），回答方式与固件相同；键和代码列在 [docs/CONTRACT.md](docs/CONTRACT.md)，并有测试保证该页面与应答一致。已在电脑上测试；未在电路板上运行。
* **37 种语言：** 节点的网页面板支持英语、西班牙语、意大利语、德语、法语、葡萄牙语、中文、日语、保加利亚语、克罗地亚语、捷克语、丹麦语、荷兰语、爱沙尼亚语、芬兰语、希腊语、匈牙利语、爱尔兰语、拉脱维亚语、立陶宛语、马耳他语、波兰语、罗马尼亚语、斯洛伐克语、斯洛文尼亚语、瑞典语、挪威语、冰岛语、乌克兰语、俄语、塞尔维亚语、阿尔巴尼亚语、加泰罗尼亚语、加利西亚语、巴斯克语、巴西葡萄牙语和加拿大法语。 前八种之外的 29 种语言由模型翻译，等待母语者审校；某种语言缺少的文本以英文显示。
* **始终可见的保存栏与设置备份：** “保存”和“保存并重启”始终出现在有设置的页面上（没有更改时为灰色并附说明），管理员随时可用“重启”；“系统”页面可把全部设置导出为文件（含 Wi-Fi 密码和令牌：请私密保存），并可导回表单而不发送任何内容，因此每个页面用各自的“保存”来保存；节点的 ID、名称和主机名绝不会被导入更改。“网络”页面还可编辑名称、时间服务器、时区和定期重启，备用 Wi-Fi 网络现在也算作更改，并在保存时发送。为浏览器编写；尚未在电路板上试过。
* **按作业调整的激光：** VL53L1X 有距离模式（短或长）和时间预算，可缩小接收区域以对准小点，滤波器会丢弃偏离中位数过远的读数，每个读数都有状态（有效、信号弱、离散度高、超出范围），并支持在已知距离下标定（偏移和串扰），须先由管理员开启。尚未在传感器上试过。
* **同一根线上的多个 DS18B20：** 节点会在总线上搜索所有传感器（校验每个地址的 CRC），按地址选取主轴的那个（`temp_rom`；只有一个时用它，多个且未指定时用找到的第一个并在状态页提示），设置 9 到 12 位的分辨率（`temp_resolution`），只等待该分辨率所需的转换时间。已在计算机上用模拟总线测试，尚未在真实线路上试过。
* **能区分噪声与转数的转速传感器：** TCRT5000（反射式，标记贴在主轴上）可用数字输出的中断，或用高速 ADC 读取模拟输出并经阈值跟随信号的施密特触发器；比主轴可能转速更快的脉冲、一圈中间的尖峰和丢失的脉冲都会被滤除，状态会说明脉冲是干净还是有噪声。已用 400 Hz 和 1 kHz 的模拟脉冲序列测试；尚未在主轴上试过。
* **在面板中调整的摄像头画面：** 镜像、上下翻转、亮度、对比度、饱和度、自动或手动曝光以及自动白平衡，在摄像头启动时应用到 OV2640 和 OV5640（传感器没有的设置保持不变）。尚未在摄像头上试过。

## 🔒 安全与防护

| 威胁 | 防御 |
|---|---|
| 主轴过快或过热 | 保护输出在超过 `rpm_max` 和达到临界温度时断开，带回差以免闪烁；已启用但无响应的温度传感器视为过热 |
| 节点断电或死机 | 在节点检查完主轴之前输出为低，节点未运行时也保持为低，所以应把它接成会自行断开的停机线路 |
| 网络上有人测量或查看车间 | 没有令牌时节点拒绝测量和摄像头；令牌按恒定时间比较，从不显示也不记录 |
| 把坏的测量当成好的 | 丢弃异常读数；离散度超过 `laser_sigma_max_mm`、超出量程或读数太少时，带原因拒绝测量 |
| 设置里的笔误 | 超出范围的值或未知的键会被报告并保留安全默认值；临界温度低于警告温度或量程颠倒时回到默认值 |
| 固件或仓库里的机密 | 不会编译进任何机密；令牌是其分区中的一项设置 |

**未防御的部分：** 节点不是安全装置。保护输出的可靠性取决于把它接到停机线路的接线，而这里的一切都没有在机床上试过。网络口没有加密（车间网络上的普通 HTTP 端口）：请把它放在独立网络中。

## 🔌 接口

| 接口 | 说明 | 位置 |
|---|---|---|
| 设置 | 闪存中的一个文档，在网页面板的“钻头”页编辑；键名与 `head.cfg.example` 相同 | `core/bungard_config.hpp`、`core/aux_config.hpp` |
| 状态 | `GET /api/status`：节点、摄像头、激光、转速、温度、保护输出和运行时间，一行 JSON | `core/protocol.hpp` |
| 测量 | 带令牌的 `POST /api/measure[?samples=N]`：到工作台的距离、离散度以及测量被拒绝的原因 | `core/laser.hpp`, `main/app_main.cpp` |
| 摄像头 | 带令牌的 `GET /stream`：MJPEG | `main/app_main.cpp` |
| 主轴脉冲 | 一个 GPIO（menuconfig 中的 `AUX_RPM_GPIO`），上升沿 | `main/sensors.cpp` |
| 保护输出 | 一个 GPIO（`AUX_PROTECT_GPIO`），主轴必须停止时为低 | `main/sensors.cpp` |
| 激光与温度 | 激光用 I2C，温度传感器用单总线；驱动已编写但从未运行（红外测温 MLX90614 共用 I2C 总线，DS18B20 用一个引脚，NTC 用一个模拟输入） | `main/sensors.cpp` |
| RTSP | `rtsp://地址:端口/stream1`：OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、GET_PARAMETER、TEARDOWN；JPEG over RTP，走 UDP 或 TCP 交错；Digest 或 Basic 登录 | `core/rtsp.hpp`, `core/rtsp_session.hpp`, `core/rtp_jpeg.hpp`, `main/stream.cpp` |
| ONVIF | 多播地址 `239.255.255.250:3702` 上的 WS-Discovery；`/onvif/device_service` 与 `/onvif/media_service` 上的 SOAP（S 配置文件：能力、服务、配置文件、编码器、流与快照地址）；快照在 `/snapshot.jpg` | `core/onvif.hpp`, `main/stream.cpp` |

## ⚙️ 配置

设置作为一个文档保存在闪存中，在网页面板的“钻头”页编辑（密码和令牌为只写：面板从不显示它们）。键名及其范围与 `head.cfg.example` 所列相同；早期版本的 “钻头”页 文件不再被读取。

| 设置 | 默认值 | 范围 | 作用 |
|---|---|---|---|
| `token` | none | 8 - 64 | 访问令牌；没有它节点会拒绝测量和摄像头 |
| `laser_samples` | `7` | 1 - 31 | 一次测量的读数数（取有效读数的中位数） |
| `laser_offset_mm` | `0` | -50 - 50 | 加到每个距离上：传感器在刀尖上方的位置 |
| `laser_sigma_max_mm` | `0.15` | 0.01 - 5 | 读数离散度超过此值的测量会被拒绝 |
| `rpm_pulses_per_rev` | `1` | 1 - 64 | 主轴每转的传感器脉冲数 |
| `rpm_max` | `65000` | 1000 - 200000 | 超过此转速保护输出断开 |
| `temp_sensor` | none | none ds18b20 ntc mlx90614 | 主轴温度传感器 |
| `temp_warn_c / temp_critical_c` | `55 / 70` | 20 - 200 | 警告值和保护输出断开的临界值 |
| `protect_output` | `on` | on / off | 保护输出工作；仅在测试台上关闭 |
| `stream_user / stream_password` | `viewer / none` | 8 - 64 | RTSP 和 ONVIF 的登录；没有密码就不为任何人服务 |
| `rtsp_port` | `554` | 1 - 65535 | RTSP 服务器端口（网页服务器端口是 `port`） |

## 🚀 快速开始

电脑测试需要 C++17 编译器和 CMake，固件需要 ESP-IDF 5.5.5（目标 ESP32-S3）。

```bash
build.bat            # 电脑上的测试（Windows）  -  Linux 上用 ./build.sh
python -I tools/ci_validate.py
```

固件可用 ESP-IDF 5.5.5 编译：`idf.py set-target esp32s3 && idf.py menuconfig && idf.py build` 生成镜像（在 `EH3D Bungard AUX` 下设置你板子的引脚），节点运行后，在网页面板中进行设置。

## 🧪 测试

电脑测试（`build.bat` 或 `./build.sh`，再 `ctest`）是两个程序，全部通过：`test_aux`（103 项检查）覆盖设置文件（默认值、取值、错误取值、彼此矛盾的限值）、激光测量（中位数、被丢弃的异常读数、偏移、被标为坏的读数、量程、没有读数、过大的离散度）、主轴转速（每转脉冲数、减速的主轴、脉冲停止即停转）、温度（回差、传感器故障）、保护输出，以及网络用语（请求行、路由、头或查询中的令牌、读数个数、状态和测量的 JSON、高度图一个点的那一行）。固件未被覆盖：它能编译，但测试在电脑上运行。 第二个程序 `test_stream`（100 项检查）覆盖视频：登录所用哈希对照公布的向量（MD5、SHA-1、base64 以及 RFC 2617 的 Digest 示例）、RTSP 请求与整个会话（登录、SDP、UDP 与交错传输、play、pause、teardown、错误会话、拒绝多播）、JPEG 的 RTP 载荷（首包中的量化表、顺序、标记位、序号回绕），以及 ONVIF（发现、带标准示例的用户令牌、开放与受保护的调用、各项服务和未实现的调用）。 除这两个程序外，`test_head_settings` 以与面板相同的方式读取、检查和写入设置文档的钻头部分（密码和令牌仍为只写），固件基础的共用测试涵盖从 GitHub 更新、时钟和用户。

探头接上硬件当天的自检：`python tools/selftest.py --host 节点 --token TOKEN` 读取节点（版本、状态、设置导出、备用网络、温度、转速、灯光、摄像头画面、视频流、RTSP、一次激光读数），每步一行写入报告；加上 `--active` 并手动输入确认后，还会点亮灯环两秒再恢复原状。它从不执行激光校准。其测试在 CI 中对一个玩具节点运行（`python -I -m unittest discover -s tools -p "test_*.py"`）。手动步骤见 [docs/HARDWARE_CHECKLIST.md](docs/HARDWARE_CHECKLIST.md)。

## 🏗️ 构建与发布

版本在 `eh3d.project.json` 中，每个版本在 [CHANGELOG.md](CHANGELOG.md) 中都有条目。`build.sh` / `build.bat` 构建并运行电脑测试，`test.sh` / `test.bat` 运行仓库检查。固件用 ESP-IDF 5.5.5 构建（`idf.py build`），尚未进行；还没有发布过版本。

## 🩺 故障排除

| 现象 | 处理办法 |
|---|---|
| 测量被以 `no_readings` 拒绝 | I2C 总线上没有 VL53L1X 响应：请检查 `menuconfig` 中的 SDA 和 SCL 引脚（在 Waveshare ESP32-S3-ETH 上摄像头占用 GPIO 41 和 42）以及接线；回答的其余部分已测试。 |
| 测量被以 `too_noisy` 拒绝 | 读数离散度超过 `laser_sigma_max_mm`：让钻头和工作台稳定下来，或稍微放宽限值；检查有没有东西在抖动。 |
| 节点对测量或摄像头返回 401 | 在 “钻头”页 中设置 `token`（8 到 64 个字符），并放在头 `Authorization: Bearer ...` 或 `?token=` 中发送。 |
| 保护输出始终断开 | 已启用但无响应的温度传感器视为过热：请检查它的接线、`menuconfig` 中的引脚和 “钻头”页 中的 `temp_sensor`，或在测试台上设置 `temp_sensor = none`。 |

## 📂 仓库结构

<p align="center">
  <img src="images/repository-structure.svg" alt="EH3D-NODE-BUNGARD-AUX 的仓库结构" width="100%">
</p>

<details>
<summary>目录树的文本版</summary>

```text
EH3D-NODE-BUNGARD-AUX/
├── core/
│   ├── aux_config.hpp                           设置文件及其安全默认值
│   ├── self_check.hpp                           节点对自身状态的判断及其代码
│   ├── head_sim.hpp                             不存在的钻头模拟，供测试和程序使用
│   ├── bungard_config.hpp                       节点设置和钻头部分
│   ├── laser.hpp                                工作台高度的测量
│   ├── laser_hw.hpp                             激光的寄存器
│   ├── light.hpp                                LED 环形灯：亮度、颜色、信号
│   ├── camera.hpp                               摄像头 OV2640 和 OV5640 及其尺寸
│   ├── spindle.hpp                              转速传感器：脉冲、噪声、施密特
│   ├── thermo.hpp                               温度：DS18B20、NTC、MLX90614
│   ├── onewire.hpp                              1-Wire 总线：搜索、地址、分辨率
│   ├── hash.hpp                                 登录：MD5、SHA-1、base64
│   ├── rtsp.hpp rtsp_session.hpp rtp_jpeg.hpp   RTSP 与 JPEG 的 RTP 载荷
│   ├── onvif.hpp                                ONVIF：发现与 SOAP 服务
│   ├── protocol.hpp                             网络用语：路由、令牌、JSON
│   ├── node_settings.hpp auth.hpp               设置和用户（共享基础）
│   ├── netplan.hpp json.hpp                     网络方案和 JSON（共享基础）
│   └── release_assets.hpp semver.hpp            发布中哪个文件是这块板子的镜像
├── main/
│   ├── app_main.cpp                             启动、设置、网络、面板
│   ├── head.cpp head.hpp                        摄像头、激光、主轴、路由
│   ├── light.cpp light.hpp                      LED 环形灯驱动
│   ├── sensors.cpp sensors.hpp                  引脚：脉冲、保护输出、传感器
│   ├── stream.cpp stream.hpp                    RTSP 服务器、ONVIF 服务与发现
│   ├── idf_component.yml Kconfig.projbuild      组件列表与 menuconfig 引脚
│   ├── web_server.cpp network.cpp               面板服务器、网络（基础）
│   ├── node_store.cpp node_guard.cpp            设置存储、看门狗（基础）
│   └── github_update.cpp entropy.cpp            来自 GitHub 的更新和槽位复制
├── panel/
│   ├── index.html style.css                     页面和侧边菜单布局
│   ├── app.js text.js                           页面和37 种语言的文本
│   └── locales/                                 另外 29 种语言的文字，每种一个文件
├── tests/                                       设置、闸门、语音和界面文本的主机测试
├── docs/                                        硬件说明、与程序的约定和图的源文件
├── tools/                                       CI 检查、README 检查和绘图工具
├── CMakeLists.txt                               ESP-IDF 工程
├── partitions.csv                               带两个槽位的闪存布局
├── head.cfg.example                             每个设置及其默认值和范围
├── sdkconfig.defaults                           两块板共用的 ESP-IDF 设置
├── sdkconfig.board.s3-eth                       有线板设置
├── sdkconfig.board.s3-wifi                      Wi-Fi 板设置
├── dependencies.lock                            组件的确切版本
├── images/                                      横幅图以及架构和仓库结构图片
├── .github/                                     CI 与发布工作流、Issue 和 PR 模板
├── build.sh build.bat                           在实时面板中编译并运行主机测试
├── eh3d.project.json                            清单：版本、角色、成熟度和构建命令
├── CHANGELOG.md                                 每个版本的变更
├── SECURITY.md SUPPORT.md                       如何报告漏洞及在哪里提问
├── CONTRIBUTING.md CODE_OF_CONDUCT.md LICENSE   参与贡献、行为准则和许可证
└── bump_manifest_version.py                     清单的版本工具
```

</details>

## 🗺️ 路线图与已知限制

**当前已知限制**

* 没有在板子上运行过任何东西：固件能编译但尚未烧录或试用，摄像头和网络芯片的引脚必须对照板子的原理图检查。
* 激光（I2C 上的 VL53L1X，使用较少人用的组件 `grrtzm/vl53l1x_library`）、温度传感器和摄像头的驱动已编写但从未运行：板子上第一个响应的传感器可能暴露出错误。
* EH3D-BUNGARD-GUI（钻头节点窗口）和 EH3D-SERVER 会直接或经服务器查询该节点；都还没有对真实节点试过。
* 网络口是普通 HTTP：请放在独立网络中。
* RTSP 和 ONVIF 尚未用真实的播放器、录像机或 ONVIF 管理软件试过；板子上的摄像头型号将决定尺寸和引脚。

**下一步**

* 构建固件，并在板子上试用驱动。
* 加入与 EH3D-BUNGARD-GUI、EH3D-SERVER 和屏幕的连接，以及摄像头视图。
* 在机床上试验接入停机线路的保护输出。

## 🌍 语言

README 和文本提供英语、西班牙语、法语、意大利语、德语、汉语、日语和欧洲葡萄牙语版本。

## 🔗 相关项目

* [EH3D-BUNGARD-GUI](https://github.com/JuanenRac/EH3D-BUNGARD-GUI): 用这些测量构建工作台高度图的现代界面
* [EH3D-NODE-BUNGARD](https://github.com/JuanenRac/EH3D-NODE-BUNGARD): 接在控制单元串口上、驱动机床的节点
* [EH3D-NODE-BUNGARD-HMI](https://github.com/JuanenRac/EH3D-NODE-BUNGARD-HMI): 操作员的触摸屏
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): 家族的中心：它将把节点的回答带给每个客户端
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): 家族的公共部分，包括 `eh3d-levelling`（工作台高度图）

## 📚 文档

* 设置：`head.cfg.example` 列出每个键及其默认值和范围。
* 硬件：引脚、保护输出的接线和零件，见 `docs/HARDWARE.md`。
* [更新日志](CHANGELOG.md)

## 🤝 参与贡献与社区

* 提交 pull request 之前请运行主机测试（`./build.sh` 或 `build.bat`）和 `python -I tools/ci_validate.py`：必须全部通过。参见 [CONTRIBUTING.md](CONTRIBUTING.md) 和[行为准则](CODE_OF_CONDUCT.md)。
* README 有八种语言：文本改动要在八个文件中同时进行，两幅图由 `tools/make_architecture.py` 和 `tools/make_structure.py` 根据 `docs/architecture.json` 和 `docs/structure.json` 绘制。
* 任何会改变节点给出测量或断开保护输出方式的改动，都必须有覆盖它的测试和一条硬件上观察结果的说明。
* 请私下报告漏洞：[SECURITY.md](SECURITY.md)。问题、想法和报告：electrohobby3d@gmail.com（[SUPPORT.md](SUPPORT.md)）。

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 许可证

软件：GPL-3.0-or-later。见 [LICENSE](LICENSE)。套件的硬件设计使用 CERN OHL-S。
