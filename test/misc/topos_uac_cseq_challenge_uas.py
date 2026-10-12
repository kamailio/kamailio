#!/usr/bin/env python3
"""Standalone diagnostic Digest challenge UAS for Kamailio topos/UAC issue #4837.

Run on an isolated loopback port behind Kamailio. This isn't a Kamailio
regression test until paired with a configured originating UAC and a
Kamailio config that enables topos, dialog and uac_auth().
"""
import argparse
import socket
import time


def parse_headers(packet):
    text = packet.decode("utf-8", errors="replace")
    lines = text.split("\r\n")
    headers = {}
    for line in lines[1:]:
        if ":" in line:
            name, value = line.split(":", 1)
            headers.setdefault(name.lower(), []).append(value.strip())
    return lines[0], headers


def head(headers, name, default=""):
    return headers.get(name.lower(), [default])[0]


def reply(headers, code, reason, tag, extra=""):
    via = "".join(f"Via: {v}\r\n" for v in headers.get("via", []))
    frm = head(headers, "from")
    to = head(headers, "to")
    if ";tag=" not in to.lower():
        to += ";tag=" + tag
    return (
        f"SIP/2.0 {code} {reason}\r\n"
        + via
        + f"From: {frm}\r\nTo: {to}\r\n"
        + f"Call-ID: {head(headers, 'call-id')}\r\n"
        + f"CSeq: {head(headers, 'cseq')}\r\n"
        + extra
        + "Content-Length: 0\r\n\r\n"
    ).encode("utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5090)
    parser.add_argument("--timeout", type=float, default=45)
    args = parser.parse_args()

    first = None
    challenged = False
    completed = False
    tag = "toposregress"
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.host, args.port))
        sock.settimeout(args.timeout)
        print(f"Challenge UAS listening at {args.host}:{args.port}", flush=True)
        end = time.monotonic() + args.timeout
        while time.monotonic() < end:
            packet, peer = sock.recvfrom(65535)
            request, headers = parse_headers(packet)
            method = request.split(" ", 1)[0]
            if method == "ACK":
                print("ACK", head(headers, "cseq"))
                continue
            if method != "INVITE":
                continue
            callid = head(headers, "call-id")
            cseq = head(headers, "cseq").split(" ", 1)[0]
            branch = head(headers, "via").split("branch=", 1)[-1].split(";", 1)[0]
            auth = head(headers, "authorization")
            print(f"INVITE cseq={cseq} branch={branch} auth={bool(auth)}", flush=True)

            if not challenged:
                first = (callid, cseq, branch)
                challenged = True
                sock.sendto(reply(headers, 401, "Unauthorized", tag,
                    'WWW-Authenticate: Digest realm="topos-test", '
                    'nonce="b0de123456", algorithm=MD5, qop="auth"\r\n'), peer)
                continue

            if callid != first[0] or int(cseq) <= int(first[1]) or branch == first[2]:
                raise AssertionError("Authenticated INVITE must retain Call-ID, "
                                     "increase CSeq, and use a new Via branch")
            if not auth:
                raise AssertionError("Second INVITE lacks Authorization")
            if completed:
                raise AssertionError("Received duplicate authenticated INVITE; "
                                     "check transaction matching and retransmits")

            for status, reason in ((100, "Trying"), (180, "Ringing"), (200, "OK")):
                extra = f"Contact: <sip:uas@{args.host}:{args.port}>\r\n" if status == 200 else ""
                sock.sendto(reply(headers, status, reason, tag, extra), peer)
            completed = True
            print("PASS: authenticated INVITE accepted and replied 100/180/200. "
                  "Verify originating side receives those responses and Kamailio "
                  "does not retransmit for missing transaction matches.", flush=True)
            return
    raise SystemExit("FAIL: no authenticated INVITE received")


if __name__ == "__main__":
    main()
