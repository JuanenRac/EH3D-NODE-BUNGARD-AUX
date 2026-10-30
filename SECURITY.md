# Security policy

EH3D-NODE-BUNGARD-AUX is firmware for a node on the head of a machine: it measures the height of the table, watches the speed and the temperature of a spindle, shows the workshop with a camera and drives a protect output. It is not a safety system. The stop that protects people and the machine is a hardware button that cuts the drive by itself; the protect output is a signal for the stop line, not a replacement for it.

What the firmware does, and what is tested on a computer:

* A measure comes from several readings: the odd ones are thrown away, and a spread above the limit, a range or too few readings refuse it with the reason.
* The protect output opens when the spindle is too fast or too hot, when a temperature sensor that is switched on does not answer and, by its wiring, when the node is not running.
* Without a token the node refuses the measure and the camera; with one, the token is compared in constant time and is never shown or logged. Nothing secret is compiled in.
* A value out of range in `head.cfg` or an unknown key is reported and the safe default stays.

What it does **not** do: the firmware has not been built or run on a board, the drivers of the laser and of the temperature sensors are not written, and the network port is plain HTTP with no encryption: keep it on a network of its own.

## Reporting a problem

Please report a vulnerability privately to **electrohobby3d@gmail.com**, with the steps that show it. Do not open a public issue for it. You will get an answer as soon as possible; this is a one-person project.
