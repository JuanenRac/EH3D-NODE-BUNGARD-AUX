# EH3D-NODE-BUNGARD-AUX - what each FreeRTOS task of this node needs at least (the check in check_task_safety.py reads it; a task that is not here fails the check).
# Every number is the size it has now or more: shrinking one has to be a decision, not an accident.
# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
NEEDS = {
    "github-check": (12288, "TLS and the release JSON"),
    "github-ota": (12288, "TLS, the download and the signature check"),
    "rtsp": (8192, "one RTSP connection"),
    "mjpeg": (6144, "the MJPEG stream of the camera"),
    "net-rescue": (6144, "brings the network back"),
    "slot-copy": (6144, "copies a firmware slot"),
    "improv": (5120, "the Wi-Fi setup over USB"),
    "light": (4096, "the light of the head"),
    "node-guard": (4096, "watches the other tasks"),
    "onvif-disc": (6144, "ONVIF discovery"),
    "rpm_adc": (4096, "reads the rpm sensor"),
    "rtsp-srv": (4096, "the RTSP server"),
    "temp": (4096, "reads the temperature"),
    "watch": (4096, "watches the spindle"),
    "boot-button": (3072, "the button of the board"),
    "clock-sync": (3072, "asks the time servers"),
    "confirm": (3072, "the confirm button"),
    "mem-watch": (3072, "watches the memory"),
    "setup-hint": (3072, "the lamp of the setup mode"),
}
