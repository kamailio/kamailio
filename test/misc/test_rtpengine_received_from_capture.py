#!/usr/bin/env python3
"""Self-contained unit tests for the RTPengine NG bencode capture helper."""
import unittest
from rtpengine_received_from_capture import decode


def enc(value):
    if isinstance(value, int):
        return b"i" + str(value).encode() + b"e"
    if isinstance(value, bytes):
        return str(len(value)).encode() + b":" + value
    if isinstance(value, list):
        return b"l" + b"".join(enc(x) for x in value) + b"e"
    if isinstance(value, dict):
        return b"d" + b"".join(enc(k) + enc(v) for k, v in value.items()) + b"e"
    raise TypeError(type(value))


class TestNGCapture(unittest.TestCase):
    def test_ipv4_override(self):
        payload = {b"command": b"offer",
                   b"received-from": [b"IP4", b"198.51.100.7"],
                   b"call-id": b"test-call"}
        result, end = decode(enc(payload))
        self.assertEqual(end, len(enc(payload)))
        self.assertEqual(result[b"received-from"], payload[b"received-from"])

    def test_ipv6_override(self):
        payload = {b"command": b"answer",
                   b"received-from": [b"IP6", b"2001:db8::7"]}
        result, end = decode(enc(payload))
        self.assertEqual(end, len(enc(payload)))
        self.assertEqual(result[b"received-from"], payload[b"received-from"])

    def test_truncated_payload(self):
        with self.assertRaises(ValueError):
            decode(b"9:abc")

    def test_empty_dictionary(self):
        result, end = decode(b"de")
        self.assertEqual(result, {})
        self.assertEqual(end, 2)


if __name__ == "__main__":
    unittest.main()
