#!/usr/bin/env python3
"""Hardware-day self-test of the EH3D-NODE-BUNGARD-AUX node (standard library only, Windows and Linux).

Read-only by default: version, state, settings and their export (compared with itself, never applied), backup networks, OTA state, the camera picture and
stream, the RTSP answer, the laser reading, the rpm sensor, the temperature and the state of the light. Nothing is changed on the node.

With --active (and a typed SI, or --yes) it also switches the light of the ring on for two seconds and puts it back as it was, checking the result.
The laser calibration is never run by this tool: it changes what every reading means.

    python tools/selftest.py --host 192.168.0.60 --token HEADTOKEN --user admin --password PASS
    python tools/selftest.py --host 192.168.0.60 --token HEADTOKEN --active

The report selftest_aux_<time>.txt is written to the current folder: one line per step (OK, FALLO, OMITIDO) with the measured value.
"""
import argparse
import http.client
import json
import os
import socket
import ssl
import sys
import time

NODE = "aux"

# ---- COMMON BEGIN (identical in every node repo; do not edit one copy alone) ----
OK = "OK"
FAIL = "FALLO"
SKIP = "OMITIDO"


class Fail(Exception):
    """A step that did not pass."""


class Skip(Exception):
    """A step that could not run (and why)."""


class Report:
    """One line per step. Every text that goes out is cleaned of the secrets the run knows."""

    def __init__(self, node, host):
        self.node, self.host = node, host
        self.secrets = []
        self.lines = []
        self.counts = {OK: 0, FAIL: 0, SKIP: 0}
        self.started = time.strftime("%Y-%m-%d %H:%M:%S")

    def hide(self, *values):
        for v in values:
            if isinstance(v, str) and len(v) >= 4 and v not in self.secrets:
                self.secrets.append(v)

    def clean(self, text):
        text = str(text)
        for s in sorted(self.secrets, key=len, reverse=True):
            text = text.replace(s, "***")
        return text

    def add(self, status, name, detail=""):
        line = "%-9s%s" % (status, name) + (" | " + detail if detail else "")
        line = self.clean(line)
        try:
            print(line, flush=True)
        except (OSError, UnicodeError):
            pass   # a closed pipe or a console that cannot show a character must not stop the run: the report file has the line
        self.lines.append(line)
        self.counts[status] += 1

    def step(self, name, fn):
        """Runs fn; it returns the measured value (text), or raises Fail / Skip."""
        try:
            self.add(OK, name, fn() or "")
        except Skip as e:
            self.add(SKIP, name, str(e))
        except Fail as e:
            self.add(FAIL, name, str(e))
        except Exception as e:  # a broken answer or a dead node is a failed step, never a crash
            self.add(FAIL, name, "%s: %s" % (type(e).__name__, e))

    def text(self):
        head = ["selftest %s" % self.node, "host: %s" % self.host, "started: %s" % self.started, ""]
        tail = ["", "OK=%d FALLO=%d OMITIDO=%d" % (self.counts[OK], self.counts[FAIL], self.counts[SKIP])]
        return self.clean("\n".join(head + self.lines + tail)) + "\n"

    def write(self, out_dir):
        path = os.path.join(out_dir, "%s_%s_%s.txt" % (getattr(self, "prefix", "selftest"), self.node, time.strftime("%Y%m%d_%H%M%S")))
        with open(path, "w", encoding="utf-8") as f:
            f.write(self.text())
        return path


def confirm(prompt, yes=False, ask=input):
    """The interactive gate of every active test: the person types SI. --yes (asked for expressly) skips the question; no answer is a no."""
    print(prompt, flush=True)
    if yes:
        print("(--yes: confirmed by whoever runs this)", flush=True)
        return True
    try:
        return ask("Escribe SI para seguir: ").strip() == "SI"
    except (EOFError, KeyboardInterrupt):
        return False


def secret_values(doc):
    """Every string kept under a key that names a password or a token, so that no report can print one."""
    found = []
    if isinstance(doc, dict):
        for k, v in doc.items():
            if isinstance(v, str) and v and any(w in k.lower() for w in ("password", "token", "secret", "psk", "key")):
                found.append(v)
            else:
                found.extend(secret_values(v))
    elif isinstance(doc, list):
        for v in doc:
            found.extend(secret_values(v))
    return found


class Http:
    """A small HTTP client. It writes nothing to the node unless allow_write is set (the login is the only exception: it opens a session)."""

    def __init__(self, host, port=80, scheme="http", insecure=False, timeout=5.0, bearer=""):
        self.host, self.port, self.scheme, self.insecure, self.timeout, self.bearer = host, int(port), scheme, insecure, timeout, bearer
        self.cookie = ""
        self.allow_write = False

    def request(self, method, path, body=None, headers=None, login=False):
        if method not in ("GET", "HEAD") and not self.allow_write and not login:
            raise RuntimeError("write refused: %s %s needs --active" % (method, path))
        if self.scheme == "https":
            ctx = ssl.create_default_context()
            if self.insecure:
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
            conn = http.client.HTTPSConnection(self.host, self.port, timeout=self.timeout, context=ctx)
        else:
            conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        hdr = {"X-Requested-With": "eh3d"}
        if self.bearer:
            hdr["Authorization"] = "Bearer " + self.bearer
        if self.cookie:
            hdr["Cookie"] = self.cookie
        if headers:
            hdr.update(headers)
        data = None
        if body is not None:
            data = body if isinstance(body, bytes) else json.dumps(body).encode()
            hdr.setdefault("Content-Type", "application/json")
        try:
            conn.request(method, path, body=data, headers=hdr)
            resp = conn.getresponse()
            payload = resp.read()
            for k, v in resp.getheaders():
                if k.lower() == "set-cookie" and "=" in v and "expires=thu, 01 jan 1970" not in v.lower():
                    self.cookie = v.split(";", 1)[0]
            return resp.status, payload
        finally:
            conn.close()

    def get_json(self, path):
        status, payload = self.request("GET", path)
        if status != 200:
            raise Fail("GET %s answered HTTP %d" % (path, status))
        try:
            doc = json.loads(payload.decode("utf-8"))
        except ValueError:
            raise Fail("GET %s did not answer valid JSON" % path)
        if not isinstance(doc, dict):
            raise Fail("GET %s did not answer a JSON object" % path)
        return doc

    def login(self, user, password):
        status, _ = self.request("POST", "/api/v1/login", {"user": user, "password": password}, login=True)
        if status != 200:
            raise Fail("login refused (HTTP %d)" % status)
        if not self.cookie:
            raise Fail("login gave no session")


def check_panel(rep, http, user, password):
    """The read-only battery of the panel API shared by the nodes: version, state, settings, export, backup networks, OTA. Returns the status document or None."""
    box = {}

    def session():
        d = http.get_json("/api/v1/session")
        box["session"] = d
        return "node_id=%s, keys=%d" % (d.get("node_id", "?"), len(d))

    rep.step("panel: session (no login)", session)
    if not user or not password:
        for n in ("panel: login", "panel: status", "panel: settings", "panel: settings export is repeatable", "panel: backup Wi-Fi networks", "panel: OTA state"):
            rep.add(SKIP, n, "give --user and --password (or EH3D_USER / EH3D_PASSWORD)")
        return None

    rep.step("panel: login", lambda: http.login(user, password) or "session opened")
    if not http.cookie:
        return None

    def status():
        d = http.get_json("/api/v1/status")
        box["status"] = d
        if not d.get("version"):
            raise Fail("the status has no version")
        return "version=%s uptime_s=%s ip=%s" % (d.get("version"), d.get("uptime_s", "?"), d.get("ip", "?"))

    rep.step("panel: status", status)

    def settings():
        d = http.get_json("/api/v1/config")
        return "%d sections: %s" % (len(d), ",".join(sorted(d)[:12]))

    rep.step("panel: settings", settings)

    def export():
        a = http.get_json("/api/v1/config/export")
        rep.hide(*secret_values(a))
        b = http.get_json("/api/v1/config/export")
        if a != b:
            raise Fail("two exports in a row differ")
        # what the import would send back: the same document, written and read again, must be the same (nothing is applied here)
        if json.loads(json.dumps(a, sort_keys=True)) != a:
            raise Fail("the export does not survive a write/read round trip")
        box["export"] = a
        return "identical twice, round trip equal, %d sections, nothing applied" % len(a)

    rep.step("panel: settings export is repeatable", export)

    def backup():
        doc = box.get("export")
        if doc is None:
            raise Skip("no export to read")
        sta = doc.get("sta")
        if not isinstance(sta, dict):
            raise Fail("the export has no sta section")
        nets = sta.get("backup", [])
        if not isinstance(nets, list) or any(not isinstance(n, dict) or not n.get("ssid") for n in nets):
            raise Fail("the backup network list is malformed")
        return "%d backup networks saved" % len(nets)

    rep.step("panel: backup Wi-Fi networks", backup)

    def ota():
        d = http.get_json("/api/v1/ota/progress")
        return "state=%s" % d.get("state", "?")

    rep.step("panel: OTA state", ota)
    return box.get("status")


def add_common_args(p, default_port):
    p.add_argument("--host", required=True, help="address of the node")
    p.add_argument("--port", type=int, default=default_port, help="HTTP port of the node (default %d)" % default_port)
    p.add_argument("--https", action="store_true", help="use HTTPS (add --insecure for the node's own certificate)")
    p.add_argument("--insecure", action="store_true", help="do not verify the certificate")
    p.add_argument("--token", default=os.environ.get("EH3D_TOKEN", ""), help="the node's access token (or the variable EH3D_TOKEN); never printed")
    p.add_argument("--user", default=os.environ.get("EH3D_USER", ""), help="panel user (or EH3D_USER)")
    p.add_argument("--password", default=os.environ.get("EH3D_PASSWORD", ""), help="panel password (or EH3D_PASSWORD); never printed")
    p.add_argument("--timeout", type=float, default=5.0, help="seconds to wait for one answer")
    p.add_argument("--out-dir", default=".", help="where the report goes (default: the current folder)")
    p.add_argument("--active", action="store_true", help="also run the tests that move things or start the spindle (asks for confirmation)")
    p.add_argument("--yes", action="store_true", help="answer the confirmation of --active yes (only when you expressly want it)")
# ---- COMMON END ----



def tokened(args):
    return Http(args.host, args.port, "https" if args.https else "http", args.insecure, args.timeout, bearer=args.token)


def stream_bytes(args, path, limit=60000, seconds=3.0):
    """Reads the start of an endless answer (the stream) and returns (status, content type, bytes read)."""
    if args.https:
        ctx = ssl.create_default_context()
        if args.insecure:
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
        conn = http.client.HTTPSConnection(args.host, args.port, timeout=args.timeout, context=ctx)
    else:
        conn = http.client.HTTPConnection(args.host, args.port, timeout=args.timeout)
    try:
        conn.request("GET", path, headers={"Authorization": "Bearer " + args.token})
        resp = conn.getresponse()
        got = b""
        end = time.time() + seconds
        while len(got) < limit and time.time() < end:
            chunk = resp.read(4096)
            if not chunk:
                break
            got += chunk
        return resp.status, resp.getheader("Content-Type", ""), got
    finally:
        conn.close()


def check_head(args, rep, http_, st):
    """The read-only checks of the head: camera, stream, RTSP, laser, rpm, temperature, light."""
    live = st or {}

    def status():
        d = http_.get_json("/api/status")
        live.update(d)
        if not d.get("version"):
            raise Fail("the status has no version")
        return "version=%s node=%s uptime_s=%s camera=%s(%s) laser=%s rpm_sensor=%s" % (d.get("version"), d.get("node", "?"), d.get("uptime_s", "?"), d.get("camera"), d.get("camera_model", ""), d.get("laser"), d.get("rpm_sensor"))

    rep.step("head: status (no token needed)", status)

    def own_check():
        # the node judges itself (docs/CONTRACT.md): a fail is a failed step, a warn is shown; an older firmware has no such key
        verdict = live.get("selfcheck")
        if verdict is None:
            raise Skip("this firmware has no self-check")
        if verdict == "fail":
            raise Fail("the node reports a fault: %s" % (live.get("selfcheck_codes") or "no code"))
        return "selfcheck=%s codes=%s" % (verdict, live.get("selfcheck_codes") or "none")

    rep.step("head: the node's own check", own_check)

    def temp():
        t = live.get("temp_c")
        if t is None:
            raise Skip("no temperature sensor configured or no reading yet")
        if not -20 <= float(t) <= 120:
            raise Fail("temperature out of any believable range: %s" % t)
        return "temp_c=%s heat=%s" % (t, live.get("heat", "?"))

    rep.step("head: temperature", temp)

    def rpm():
        if not live.get("rpm_sensor"):
            raise Skip("the rpm sensor is not enabled in the settings")
        return "rpm=%s (the spindle turns? this is what the sensor sees now)" % live.get("rpm")

    rep.step("head: rpm sensor", rpm)

    def light_state():
        if not args.token:
            raise Skip("give --token (or EH3D_TOKEN)")
        d = http_.get_json("/api/light")
        live["_light"] = d
        return "light=%s" % json.dumps(d, sort_keys=True)[:160]

    rep.step("head: light state", light_state)

    def snapshot():
        if not args.token:
            raise Skip("give --token (or EH3D_TOKEN)")
        if live.get("camera") is False:
            raise Skip("no camera in the settings")
        t0 = time.time()
        code, payload = http_.request("GET", "/api/snapshot")
        if code != 200:
            raise Fail("snapshot answered HTTP %d" % code)
        if payload[:2] != b"\xff\xd8":
            raise Fail("the snapshot is not a JPEG")
        return "JPEG %d bytes in %.0f ms" % (len(payload), (time.time() - t0) * 1000)

    rep.step("head: camera snapshot", snapshot)

    def stream():
        if not args.token:
            raise Skip("give --token (or EH3D_TOKEN)")
        if live.get("camera") is False:
            raise Skip("no camera in the settings")
        t0 = time.time()
        code, ctype, data = stream_bytes(args, "/stream")
        if code != 200:
            raise Fail("stream answered HTTP %d" % code)
        if b"\xff\xd8" not in data:
            raise Fail("no JPEG frame in the first %d bytes of the stream" % len(data))
        return "%s, %d bytes in %.1f s" % (ctype.split(";")[0], len(data), time.time() - t0)

    rep.step("head: MJPEG stream", stream)

    def rtsp():
        if live.get("camera") is False:
            raise Skip("no camera in the settings")
        try:
            s = socket.create_connection((args.host, args.rtsp_port), timeout=args.timeout)
        except OSError as e:
            raise Skip("RTSP port %d not reachable (switched off in the settings?): %s" % (args.rtsp_port, e))
        try:
            s.sendall(("OPTIONS rtsp://%s:%d/ RTSP/1.0\r\nCSeq: 1\r\n\r\n" % (args.host, args.rtsp_port)).encode())
            s.settimeout(args.timeout)
            answer = s.recv(512).decode("ascii", "replace")
        finally:
            s.close()
        if not answer.startswith("RTSP/1.0 200"):
            raise Fail("RTSP OPTIONS answered %r" % answer.split("\r\n")[0])
        return "RTSP OPTIONS 200"

    rep.step("head: RTSP", rtsp)

    def measure():
        if not args.token:
            raise Skip("give --token (or EH3D_TOKEN)")
        if not live.get("laser"):
            raise Skip("the laser is not enabled in the settings")
        code, payload = http_.request("POST", "/api/measure?samples=3", login=True)  # a reading: it changes nothing on the node
        if code != 200:
            raise Fail("measure answered HTTP %d" % code)
        try:
            d = json.loads(payload.decode())
        except ValueError:
            raise Fail("measure did not answer valid JSON")
        return "reading=%s" % json.dumps(d, sort_keys=True)[:160]

    rep.step("head: laser reading", measure)
    rep.add(SKIP, "head: laser calibration", "never run by the tool (it changes what every reading means); do it from the panel with a flat target")


def run_active(args, rep, http_, ask=input):
    if not args.token:
        rep.add(SKIP, "active tests", "give --token (or EH3D_TOKEN)")
        return
    if not confirm("Voy a ENCENDER el aro de luz de la cabeza al nivel %d durante 2 s y dejarlo como estaba. No se mueve nada. Escribe SI para seguir." % args.level, args.yes, ask):
        rep.add(SKIP, "active tests", "confirmation not given: nothing active was run")
        return
    http_.allow_write = True
    before = {}

    def light():
        try:
            before.update(http_.get_json("/api/light"))
        except Fail:
            raise
        if not before:
            raise Fail("no light state")
        if before.get("light") is False:
            raise Skip("the light ring is not enabled in the settings")
        code, payload = http_.request("POST", "/api/light?on=1&level=%d" % args.level)
        if code != 200:
            raise Fail("light on answered HTTP %d" % code)
        time.sleep(2.0)
        now = http_.get_json("/api/light")
        if not now.get("on", now.get("light_on")):
            raise Fail("the light did not report being on: %s" % json.dumps(now, sort_keys=True)[:120])
        return "light on at level %s for 2 s" % now.get("level", now.get("light_level", "?"))

    try:
        rep.step("active: light on for 2 s", light)
    finally:
        # back to the state it had: off unless it was on
        was_on = bool(before.get("on", before.get("light_on", False)))
        try:
            if not before or before.get("light") is False:
                pass   # nothing was touched
            elif was_on:
                http_.request("POST", "/api/light?on=1&level=%s" % before.get("level", before.get("light_level", 60)))
            else:
                http_.request("POST", "/api/light?on=0")
        except Exception:
            pass

    def restored():
        if not before or before.get("light") is False:
            raise Skip("the light was not touched")
        now = http_.get_json("/api/light")
        is_on = bool(now.get("on", now.get("light_on", False)))
        was_on = bool(before.get("on", before.get("light_on", False)))
        if is_on != was_on:
            raise Fail("NOT RESTORED: the light is %s, it was %s" % ("on" if is_on else "off", "on" if was_on else "off"))
        return "light back to %s" % ("on" if is_on else "off")

    rep.step("active: light restored", restored)


def main(argv=None, ask=input):
    p = argparse.ArgumentParser(description="Self-test of the EH3D-NODE-BUNGARD-AUX node (read-only unless --active).")
    add_common_args(p, 80)
    p.add_argument("--rtsp-port", type=int, default=554, help="RTSP port (default 554)")
    p.add_argument("--level", type=int, default=20, help="light level of the active test, 1..100 (default 20)")
    args = p.parse_args(argv)
    rep = Report(NODE, args.host)
    rep.hide(args.token, args.password)
    http_ = tokened(args)
    status = check_panel(rep, http_, args.user, args.password)

    def head_panel():
        if not http_.cookie:
            raise Skip("needs the panel login")
        d = http_.get_json("/api/v1/head")
        s = http_.get_json("/api/v1/head/status")
        return "head settings=%d keys, status=%d keys" % (len(d), len(s))

    rep.step("panel: head settings and status", head_panel)
    check_head(args, rep, http_, {})
    if args.active:
        run_active(args, rep, http_, ask)
    else:
        rep.add(SKIP, "active tests", "not requested (add --active)")
    path = rep.write(args.out_dir)
    print("report: " + path)
    return 1 if rep.counts[FAIL] else 0


if __name__ == "__main__":
    sys.exit(main())
