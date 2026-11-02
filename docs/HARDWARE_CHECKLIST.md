# Hardware checklist - EH3D-NODE-BUNGARD-AUX

Run the automatic part first, then the steps by hand. Write the result next to each step (PASS / FAIL and what you saw) and keep the report file with the notes.

## Automatic part

```
python tools/selftest.py --host NODE --token HEADTOKEN --user admin --password PASS
```

The token and the password can also come from `EH3D_TOKEN` and `EH3D_PASSWORD`; they are never printed. The report `selftest_aux_<time>.txt` lands in the current folder. It reads the version, status, settings and their export (compared with itself, never applied), backup networks, temperature, rpm, light state, a camera picture, the stream, the RTSP answer and one laser reading. Nothing is changed on the node.

```
python tools/selftest.py --host NODE --token HEADTOKEN --active
```

After you type `SI` it switches the light ring on for 2 s and puts it back as it was. The laser calibration is never run by the tool.

## By hand

| Step | What to do | What must happen | Result |
|---|---|---|---|
| 1 | Open the stream address in a player (`/stream`, and the RTSP address of the panel) | A live picture, a few frames per second, no freezing for a minute | |
| 2 | Hold a flat target at a known distance and read the laser in the panel | The reading is within the error of the sensor; with nothing in front the status says out of range | |
| 3 | Run the laser calibration from the panel with the flat target | The offset changes and the reading matches the ruler afterwards | |
| 4 | Turn the spindle by hand or at low speed | The rpm shown matches a hand tachometer | |
| 5 | Warm the sensor with your hand or a hair dryer | The temperature rises; the warning appears at the configured level | |
| 6 | Light: switch on, change the level and the tone, switch off | The ring follows each change and never exceeds the ceiling | |
| 7 | Take a snapshot with the light on and with it off | Both are sharp enough to see a 0.8 mm hole | |
| 8 | Pull the network cable and plug it back | The node comes back and the stream resumes | |
