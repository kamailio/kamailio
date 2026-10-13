#!/usr/bin/env python3
"""Disposable-lab Diameter CER/DWR probe for Kamailio CDP issue #4981.

Connects three unique Diameter peers, verifies successful CEA and DWR/DWA,
then connects another peer with the same Origin-Host as the first peer.
After duplicate admission/rejection, unrelated peers must still answer DWR.

This is a manual integration test: Kamailio must be configured to accept
the test Diameter peers, and a true EBADF failure may be timing-dependent.
Never point this probe at production Diameter infrastructure.
"""
import argparse
import ipaddress
import socket
import struct
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--host", default="127.0.0.1")
parser.add_argument("--port", type=int, default=3868)
parser.add_argument("--realm", default="lab.invalid")
parser.add_argument("--origin-prefix", default="cdp-probe")
parser.add_argument("--duplicates", type=int, default=10)
parser.add_argument("--timeout", type=float, default=3.0)
parser.add_argument("--allow-remote", action="store_true")
parser.add_argument("--run", action="store_true", help="Required: perform network traffic")
args = parser.parse_args()
if args.duplicates < 1 or args.timeout <= 0 or not 0 < args.port < 65536:
    parser.error("invalid duplicates, timeout or port")
try:
    address = ipaddress.ip_address(args.host)
except ValueError:
    parser.error("--host must be a numeric IP address")
if not address.is_loopback and not args.allow_remote:
    parser.error("non-loopback targets require explicit --allow-remote")
if not args.run:
    parser.error("supply --run on a disposable Kamailio CDP test endpoint")

def avp(code, payload, mandatory=True):
    length = 8 + len(payload)
    raw = struct.pack("!I", code) + bytes([0x40 if mandatory else 0]) + length.to_bytes(3, "big") + payload
    return raw + b"\0" * ((-len(raw)) % 4)

def message(command, request, hop, origin):
    origin_bytes = origin.encode("ascii")
    realm_bytes = args.realm.encode("ascii")
    v = (avp(264, origin_bytes) + avp(296, realm_bytes))
    if command == 257:  # Capabilities-Exchange-Request (CER)
        v += avp(257, b"\x00\x01\x7f\x00\x00\x01")  # Host-IP-Address, IPv4 127.0.0.1
        v += avp(266, struct.pack("!I", 0))
        v += avp(269, b"kamailio-cdp-integration-probe")
        v += avp(258, struct.pack("!I", 0))
    length = 20 + len(v)
    flags = 0x80 if request else 0
    header = b"\x01" + length.to_bytes(3, "big") + bytes([flags]) + command.to_bytes(3, "big")
    header += struct.pack("!III", 0, hop, hop ^ 0x1F00)
    return header + v

def recv_exact(sock, n):
    out = bytearray()
    while len(out) < n:
        part = sock.recv(n - len(out))
        if not part:
            raise ConnectionError("Diameter TCP connection closed")
        out.extend(part)
    return bytes(out)

def recv_frame(sock):
    hdr = recv_exact(sock, 20)
    if hdr[0] != 1:
        raise ValueError("not Diameter v1")
    size = int.from_bytes(hdr[1:4], "big")
    if not 20 <= size <= 65536:
        raise ValueError(f"invalid Diameter length: {size}")
    return hdr + recv_exact(sock, size - 20)

def result_code(packet):
    body = packet[20:]
    idx = 0
    while idx + 8 <= len(body):
        code = struct.unpack_from("!I", body, idx)[0]
        flags = body[idx + 4]
        length = int.from_bytes(body[idx + 5:idx + 8], "big")
        header_len = 12 if flags & 0x80 else 8
        if length < header_len or idx + length > len(body):
            raise ValueError("invalid AVP size")
        if code == 268 and length >= header_len + 4:
            return struct.unpack_from("!I", body, idx + header_len)[0]
        idx += (length + 3) & ~3
    return None

serial = 10000

def roundtrip(sock, command, origin):
    global serial
    serial += 1
    hop = serial
    sock.sendall(message(command, True, hop, origin))
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        sock.settimeout(max(0.1, deadline - time.monotonic()))
        packet = recv_frame(sock)
        reply_command = int.from_bytes(packet[5:8], "big")
        reply_hop = struct.unpack_from("!I", packet, 12)[0]
        if not (packet[4] & 0x80) and reply_command == command and reply_hop == hop:
            return packet
    raise TimeoutError(f"no matching Diameter response for command {command}")

peers = []
try:
    for n in range(3):
        origin = f"{args.origin_prefix}-{n}.{args.realm}"
        conn = socket.create_connection((args.host, args.port), args.timeout)
        peers.append((conn, origin))
        cea = roundtrip(conn, 257, origin)
        if result_code(cea) != 2001:
            raise RuntimeError(f"precondition: CEA for {origin} returned {result_code(cea)}, expected 2001")
    for conn, origin in peers:
        roundtrip(conn, 280, origin)
    print("BASELINE OK: three healthy peers exchanged CER and DWR", flush=True)

    for n in range(args.duplicates):
        dup = socket.create_connection((args.host, args.port), args.timeout)
        try:
            dup.settimeout(args.timeout)
            dup.sendall(message(257, True, 200000 + n, peers[0][1]))
            try:
                recv_frame(dup)
            except (ConnectionError, OSError, TimeoutError):
                pass  # duplicate may be rejected or dropped
        finally:
            dup.close()
        for conn, origin in peers[1:]:
            roundtrip(conn, 280, origin)
    print(f"PASS: healthy Diameter peers survived {args.duplicates} duplicate-Origin-Host attempts")
finally:
    for conn, _ in peers:
        conn.close()
