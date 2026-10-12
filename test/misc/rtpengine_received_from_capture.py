#!/usr/bin/env python3
"""Capture an RTPengine NG request and assert received-from address/family.

An integration harness for kamailio/kamailio#4983, to be run from a
disposable Kamailio config using rtpengine_sock=udp:127.0.0.1:2223.

Run one flag order per invocation. Example:
    python3 test/misc/rtpengine_received_from_capture.py \
      --expect-family IP4 --expect-ip 198.51.100.7
Then send an INVITE with SDP that triggers:
    rtpengine_offer("received-from=198.51.100.7 replace-origin")

The production fix is being developed upstream on branch vseva/4983.
"""
import argparse
import socket


def decode(blob, offset=0):
    if offset >= len(blob):
        raise ValueError("unexpected end of bencode")
    token = blob[offset:offset + 1]
    if token == b"i":
        stop = blob.index(b"e", offset)
        return int(blob[offset + 1:stop]), stop + 1
    if token in (b"d", b"l"):
        offset += 1
        items = []
        while blob[offset:offset + 1] != b"e":
            item, offset = decode(blob, offset)
            items.append(item)
        if token == b"l":
            return items, offset + 1
        if len(items) % 2:
            raise ValueError("odd bencoded dict")
        return dict(zip(items[::2], items[1::2])), offset + 1
    colon = blob.index(b":", offset)
    size = int(blob[offset:colon])
    end = colon + 1 + size
    if end > len(blob):
        raise ValueError("truncated bencode string")
    return blob[colon + 1:end], end


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2223)
    parser.add_argument("--expect-ip", required=True)
    parser.add_argument("--expect-family", choices=("IP4", "IP6"), required=True)
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.host, args.port))
        sock.settimeout(args.timeout)
        print("Waiting for a Kamailio RTPengine NG command...", flush=True)
        data, sender = sock.recvfrom(65535)
        cookie, space, encoded = data.partition(b" ")
        if not space:
            raise AssertionError("missing NG cookie")
        decoded, end = decode(encoded)
        if end != len(encoded) or not isinstance(decoded, dict):
            raise AssertionError("bad NG payload")
        # Respond to the request before asserting so the caller doesn't hang.
        sock.sendto(cookie + b" d6:result5:errore", sender)
        got = decoded.get(b"received-from")
        expected = [args.expect_family.encode(), args.expect_ip.encode()]
        if got != expected:
            raise AssertionError(f"received-from mismatch: {got!r} != {expected!r}")
        print(f"PASS: {args.expect_family} {args.expect_ip}")


if __name__ == "__main__":
    main()
