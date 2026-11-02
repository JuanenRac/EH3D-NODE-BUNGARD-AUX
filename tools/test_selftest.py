"""Tests of tools/selftest.py against a toy head node (a thread with an HTTP server and an RTSP answer)."""
import contextlib
import http.server
import io
import json
import os
import socket
import tempfile
import threading
import unittest

import selftest as st

TOKEN = "toy-head-token-0123"
PASSWORD = "toy-password-xyz"
WIFI_SECRET = "wifi-secret-zzz"
JPEG = b"\xff\xd8\xff\xe0" + b"\x00" * 64 + b"\xff\xd9"


class ToyRtsp:
    def __init__(self, answer=b"RTSP/1.0 200 OK\r\nCSeq: 1\r\n\r\n"):
        self.sock = socket.socket()
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(2)
        self.port = self.sock.getsockname()[1]
        self.answer = answer
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        while True:
            try:
                c, _ = self.sock.accept()
            except OSError:
                return
            try:
                c.recv(512)
                c.sendall(self.answer)
            finally:
                c.close()

    def close(self):
        self.sock.close()


class ToyHead:
    """The head's routes. mode: ok, error (HTTP 500 everywhere), broken (not JSON), nojpeg (the picture is not a JPEG)."""

    def __init__(self, mode="ok", light_enabled=True):
        self.mode, self.writes = mode, []
        self.light = {"light": light_enabled, "on": False, "level": 0, "max_level": 80}
        self.light_calls = []
        node = self

        class H(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *a):
                pass

            def _send(self, code, doc, ctype="application/json", cookie=False):
                body = doc if isinstance(doc, bytes) else json.dumps(doc).encode()
                self.send_response(code)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(body)))
                if cookie:
                    self.send_header("Set-Cookie", "eh3d_session=abc123; Path=/")
                self.end_headers()
                self.wfile.write(body)

            def _token(self):
                return (self.headers.get("Authorization") or "") == "Bearer " + TOKEN

            def do_GET(self):
                path = self.path.split("?")[0]
                if path == "/api/v1/session":
                    return self._send(200, {"node_id": "toy-aux"})
                if node.mode == "error":
                    return self._send(500, {"error": "boom"})
                if node.mode == "broken":
                    return self._send(200, b"{nope")
                if path == "/api/status":
                    return self._send(200, {"node": "toy", "version": "0.0.2", "camera": True, "camera_model": "ov5640", "laser": True, "rpm_sensor": True, "rpm": 0, "temp_c": 31.5, "heat": "ok", "uptime_s": 5})
                if path in ("/api/light", "/api/snapshot", "/stream"):
                    if not self._token():
                        return self._send(401, {"error": "unauthorized"})
                    if path == "/api/light":
                        return self._send(200, node.light)
                    jpg = b"not a picture" if node.mode == "nojpeg" else JPEG
                    if path == "/api/snapshot":
                        return self._send(200, jpg, "image/jpeg")
                    return self._send(200, b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + jpg + b"\r\n--frame\r\n", "multipart/x-mixed-replace; boundary=frame")
                if "eh3d_session" not in (self.headers.get("Cookie") or ""):
                    return self._send(401, {"error": "login"})
                docs = {
                    "/api/v1/status": {"version": "0.0.2", "uptime_s": 5, "ip": "127.0.0.1"},
                    "/api/v1/config": {"node": {"name": "toy"}},
                    "/api/v1/config/export": {"node": {"name": "toy"}, "sta": {"password": WIFI_SECRET, "backup": []}},
                    "/api/v1/head": {"camera": True},
                    "/api/v1/head/status": {"ok": True},
                    "/api/v1/ota/progress": {"state": "idle"},
                }
                if path in docs:
                    return self._send(200, docs[path])
                self._send(404, {"error": "not_found"})

            def do_POST(self):
                n = int(self.headers.get("Content-Length") or 0)
                body = self.rfile.read(n)
                path, _, query = self.path.partition("?")
                node.writes.append(self.path)
                if path == "/api/v1/login":
                    if json.loads(body).get("password") == PASSWORD:
                        return self._send(200, {"ok": True}, cookie=True)
                    return self._send(401, {"error": "wrong_credentials"})
                if not self._token():
                    return self._send(401, {"error": "unauthorized"})
                if path == "/api/measure":
                    return self._send(200, {"distance_mm": 12.3, "status": "valid"})
                if path == "/api/light":
                    node.light_calls.append(query)
                    if "on=1" in query:
                        node.light.update(on=True, level=20)
                    else:
                        node.light.update(on=False, level=0)
                    return self._send(200, node.light)
                self._send(404, {"error": "not_found"})

        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
        self.httpd.daemon_threads = True
        self.port = self.httpd.server_address[1]
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()

    def close(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def run(argv, ask=lambda _: "SI"):
    out = io.StringIO()
    with tempfile.TemporaryDirectory() as d, contextlib.redirect_stdout(out):
        code = st.main(argv + ["--out-dir", d], ask=ask)
        files = [f for f in os.listdir(d) if f.startswith("selftest_aux_")]
        report = ""
        if files:
            with open(os.path.join(d, files[0]), encoding="utf-8") as f:
                report = f.read()
    return code, out.getvalue(), report


def base(node, rtsp, extra=()):
    return ["--host", "127.0.0.1", "--port", str(node.port), "--token", TOKEN, "--user", "admin", "--password", PASSWORD, "--timeout", "1", "--rtsp-port", str(rtsp.port)] + list(extra)


class SelfTest(unittest.TestCase):
    def setUp(self):
        self.node, self.rtsp = ToyHead(), ToyRtsp()

    def tearDown(self):
        self.node.close()
        self.rtsp.close()

    def test_read_only_pass_writes_nothing_but_the_login_and_a_reading(self):
        code, out, report = run(base(self.node, self.rtsp))
        self.assertEqual(code, 0, report)
        for text in ("head: camera snapshot | JPEG", "head: MJPEG stream", "head: RTSP | RTSP OPTIONS 200", "head: laser reading", "head: temperature | temp_c=31.5", "active tests | not requested"):
            self.assertIn(text, report)
        self.assertEqual(sorted(self.node.writes), ["/api/measure?samples=3", "/api/v1/login"])
        self.assertEqual(self.node.light_calls, [])

    def test_secrets_are_never_printed(self):
        code, out, report = run(base(self.node, self.rtsp))
        for secret in (TOKEN, PASSWORD, WIFI_SECRET):
            self.assertNotIn(secret, out)
            self.assertNotIn(secret, report)

    def test_node_that_does_not_answer(self):
        s = socket.socket(); s.bind(("127.0.0.1", 0)); port = s.getsockname()[1]; s.close()
        code, out, report = run(["--host", "127.0.0.1", "--port", str(port), "--token", TOKEN, "--timeout", "0.5", "--rtsp-port", str(port)])
        self.assertEqual(code, 1)
        self.assertIn("FALLO    head: status (no token needed)", report)

    def test_http_error_and_broken_json(self):
        for mode, word in (("error", "HTTP 500"), ("broken", "valid JSON")):
            node = ToyHead(mode)
            try:
                code, out, report = run(base(node, self.rtsp))
                self.assertEqual(code, 1, mode)
                self.assertIn(word, report)
            finally:
                node.close()

    def test_picture_that_is_not_a_jpeg(self):
        node = ToyHead("nojpeg")
        try:
            code, out, report = run(base(node, self.rtsp))
            self.assertEqual(code, 1)
            self.assertIn("FALLO    head: camera snapshot | the snapshot is not a JPEG", report)
        finally:
            node.close()

    def test_rtsp_refused_is_skipped_and_wrong_answer_fails(self):
        self.rtsp.close()
        code, out, report = run(base(self.node, self.rtsp))
        self.assertIn("OMITIDO  head: RTSP", report)
        bad = ToyRtsp(b"RTSP/1.0 500 no\r\n\r\n")
        try:
            code, out, report = run(base(self.node, bad))
            self.assertEqual(code, 1)
            self.assertIn("FALLO    head: RTSP", report)
        finally:
            bad.close()

    def test_wrong_token_is_a_failure(self):
        code, out, report = run(base(self.node, self.rtsp, ["--token", "wrong-token-1234"]))
        self.assertEqual(code, 1)
        self.assertIn("FALLO    head: light state | GET /api/light answered HTTP 401", report)

    def test_write_without_active_is_refused(self):
        h = st.Http("127.0.0.1", self.node.port, bearer=TOKEN)
        with self.assertRaises(RuntimeError):
            h.request("POST", "/api/light?on=1")
        self.assertEqual(self.node.light_calls, [])

    def test_denied_confirmation_changes_nothing(self):
        code, out, report = run(base(self.node, self.rtsp, ["--active"]), ask=lambda _: "no")
        self.assertEqual(code, 0, report)
        self.assertIn("confirmation not given", report)
        self.assertEqual(self.node.light_calls, [])

    def test_active_light_is_restored(self):
        code, out, report = run(base(self.node, self.rtsp, ["--active"]))
        self.assertEqual(code, 0, report)
        self.assertIn("OK       active: light on for 2 s", report)
        self.assertIn("OK       active: light restored | light back to off", report)
        self.assertFalse(self.node.light["on"])

    def test_active_failure_still_puts_the_light_back(self):
        self.node.light.update(on=True, level=50)

        original = st.time.sleep

        def boom(_):
            raise RuntimeError("interrupted")

        st.time.sleep = boom
        try:
            code, out, report = run(base(self.node, self.rtsp, ["--active"]))
        finally:
            st.time.sleep = original
        self.assertEqual(code, 1)
        self.assertIn("FALLO    active: light on for 2 s", report)
        self.assertIn("light back to on", report)          # it was on before and it is on again
        self.assertTrue(self.node.light["on"])

    def test_light_ring_not_enabled_is_skipped(self):
        node = ToyHead(light_enabled=False)
        try:
            code, out, report = run(base(node, self.rtsp, ["--active"]))
            self.assertIn("OMITIDO  active: light on for 2 s", report)
            self.assertEqual(node.light_calls, [])
        finally:
            node.close()


if __name__ == "__main__":
    unittest.main()
