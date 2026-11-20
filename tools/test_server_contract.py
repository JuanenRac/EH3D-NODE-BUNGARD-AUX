# Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
"""Contract tests of the node against EH3D-SERVER, run without the board: the routes the server calls, the verbs, the words of the ring of light and the range of the readings of a
measure, against the routes of this node, its code and its documentation. The part that reads the server is skipped when EH3D-SERVER is not next to this repository.

Nothing here has been tried on a node on a machine: it compares texts.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SERVER = ROOT.parent / "EH3D-SERVER" / "src"
SERVER_API_DOC = ROOT.parent / "EH3D-SERVER" / "docs" / "API.md"
PROTOCOL = ROOT / "core" / "protocol.hpp"
FIRMWARE = ROOT / "main" / "head.cpp"
LIGHT = ROOT / "core" / "light.hpp"
CONFIG = ROOT / "core" / "aux_config.hpp"
CONTRACT = ROOT / "docs" / "CONTRACT.md"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def node_routes() -> set:
    """(verb, path) pairs of core/protocol.hpp, where the node decides which request is which."""
    routes = set()
    for m in re.finditer(r'r\.path == "(/[a-z_/]+)"\) return (.*?);', read(PROTOCOL)):
        for verb in re.findall(r'r\.method == "([A-Z]+)"', m.group(2)):
            routes.add((verb, m.group(1)))
    return routes


def firmware_routes() -> set:
    return {(verb[len("HTTP_"):], path) for path, verb in re.findall(r'httpd_uri_t \w+ = \{"(/[a-z_/]+)", (HTTP_[A-Z]+),', read(FIRMWARE))}


def server_calls() -> set:
    """(verb, path) pairs that the server asks of the node: the functions `head_*` of api.rs and the `call` of inspect.rs."""
    calls = set()
    api = read(SERVER / "api.rs")
    for body in re.split(r"\n(?:pub )?async fn ", api)[1:]:
        name = body.split("(", 1)[0]
        if not name.startswith("head_"):
            continue
        verb = re.search(r'head::fetch(?:_limit)?\(\s*&address,\s*&token,\s*"(GET|POST)"', body)
        path = re.search(r'"(/api/[a-z_/]+)', body)
        if verb and path:
            calls.add((verb.group(1), path.group(1)))
    for verb, path in re.findall(r'call\(\s*address,\s*token,\s*"(GET|POST)",\s*(?:&format!\(\s*)?"(/api/[a-z_/]+)', read(SERVER / "inspect.rs")):
        calls.add((verb, path))
    return calls


class OwnRoutesTests(unittest.TestCase):
    def test_the_firmware_serves_the_routes_the_node_decides(self):
        self.assertEqual(firmware_routes(), node_routes())

    def test_the_contract_page_says_every_route(self):
        text = read(CONTRACT)
        for _verb, path in node_routes():
            self.assertIn(path, text, path)

    def test_the_page_does_not_promise_what_the_server_does_not_do(self):
        # a sentence of the page that says the server needs some keys is true only when the server reads them
        sentence = re.search(r"The server needs ([^.]*)\.", read(CONTRACT))
        if sentence is None:
            return
        if not SERVER.is_dir():
            self.skipTest("EH3D-SERVER is not next to this repository")
        sources = "".join(read(p) for p in SERVER.glob("*.rs"))
        for key in re.findall(r"`(\w+)`", sentence.group(1)):
            self.assertIn(f'"{key}"', sources, f"the page says the server needs {key} and the server never reads it")


@unittest.skipUnless(SERVER.is_dir(), "EH3D-SERVER is not next to this repository")
class ServerTests(unittest.TestCase):
    def test_the_server_finds_what_it_asks_for(self):
        calls = server_calls()
        self.assertGreaterEqual(len(calls), 5, calls)
        self.assertEqual(calls - node_routes(), set(), "the server asks the node for a route (or a verb) that the node does not have")

    def test_the_server_asks_for_the_routes_it_documents(self):
        calls = server_calls()
        for call in (("GET", "/api/status"), ("POST", "/api/measure"), ("GET", "/api/light"), ("POST", "/api/light"), ("GET", "/api/snapshot")):
            self.assertIn(call, calls)

    def test_the_words_of_the_ring_that_the_server_lets_through_are_read_by_the_node(self):
        words = set(re.findall(r"(\w+)=", re.search(r'light_query_ok\(\s*"([^"]+)"', read(SERVER / "inspect.rs")).group(1)))
        self.assertGreaterEqual(len(words), 7)
        known = set(re.findall(r'value\("(\w+)"\)', read(LIGHT)))
        self.assertEqual(words - known, set())

    def test_the_names_the_server_documents_for_the_ring_are_names_of_the_node(self):
        if not SERVER_API_DOC.is_file():
            self.skipTest("the documentation of EH3D-SERVER is not next to this repository")
        line = next(l for l in read(SERVER_API_DOC).splitlines() if "head/light?on=1" in l)
        light = read(LIGHT) + read(CONFIG)
        for word_kind in ("tone", "sectors", "cue"):
            group = re.search(rf"`{word_kind}` \(([^)]*)\)", line)
            self.assertIsNotNone(group, word_kind)
            names = re.findall(r"`([a-z_]+)`", group.group(1))
            self.assertGreaterEqual(len(names), 3, word_kind)
            for name in names:
                if name == "none" and word_kind == "cue":
                    continue  # no cue at all is the absence of the word
                self.assertTrue(f'"{name}"' in light, f"{word_kind} {name}: documented by the server, not a name of the node")

    def test_the_range_of_the_readings_of_a_measure_is_the_same(self):
        server = re.search(r"\((\d+)\.\.=(\d+)\)\.contains\(&n\)", read(SERVER / "api.rs"))
        node = re.search(r'key == "laser_samples"\) \{\s*if \(num\((\d+), (\d+)\)', read(CONFIG))
        self.assertEqual((int(server.group(1)), int(server.group(2))), (int(node.group(1)), int(node.group(2))))

    def test_an_address_without_a_port_reaches_the_port_of_the_node(self):
        # the server adds :80 to an address written without a port; the node listens on 80 unless its settings say another
        self.assertIn('format!("{a}:80")', read(SERVER / "head.rs"))
        self.assertEqual(int(re.search(r"std::uint16_t port = (\d+);", read(CONFIG)).group(1)), 80)

    def test_the_token_goes_the_way_the_node_reads_it(self):
        self.assertIn("Authorization: Bearer", read(SERVER / "head.rs"))
        self.assertIn("Authorization: Bearer <token>", read(PROTOCOL))

    def test_the_server_does_not_send_a_body_the_node_would_have_to_read(self):
        # the POST of the server has an empty body (Content-Length: 0): the node reads its words from the query
        self.assertIn('Content-Length: 0', read(SERVER / "head.rs"))
        for word in ("on", "level", "tone", "sectors"):
            self.assertIn(f'value("{word}")', read(LIGHT))


if __name__ == "__main__":
    unittest.main()
