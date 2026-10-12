#!/usr/bin/env python3
"""End-to-end ACK Proxy-Authorization removal test for pv_headers (#4904).

Requires a local Kamailio proxy listening on 127.0.0.1:5062, configured
to forward INVITE/ACK to this script's UAS at 127.0.0.1:5092 and run
pvh_remove_header("Proxy-Authorization", 0) before t_relay().
No external SIP accounts or real phone numbers are used.
"""
import argparse
import socket
import threading


def parse(data):
    lines = data.decode("utf-8", "replace").split("\r\n")
    headers = {}
    for line in lines[1:]:
        if ":" in line:
            name, value = line.split(":", 1)
            headers.setdefault(name.lower(), []).append(value.strip())
    return lines[0], headers


def first(headers, key, fallback=""):
    return headers.get(key.lower(), [fallback])[0]


def build_ok(headers, host, port):
    via = "".join("Via: " + v + "\r\n" for v in headers.get("via", []))
    return (
        "SIP/2.0 200 OK\r\n" + via
        + "From: " + first(headers, "from") + "\r\n"
        + "To: " + first(headers, "to") + ";tag=accept1\r\n"
        + "Call-ID: " + first(headers, "call-id") + "\r\n"
        + "CSeq: " + first(headers, "cseq") + "\r\n"
        + "".join("Record-Route: " + v + "\r\n"
                  for v in headers.get("record-route", []))
        + f"Contact: <sip:testuas@{host}:{port}>\r\n"
        + "Content-Length: 0\r\n\r\n"
    ).encode()


def uas(bind_host, port, done, seen):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind((bind_host, port))
            sock.settimeout(12)
            while True:
                payload, peer = sock.recvfrom(65535)
                line, hdr = parse(payload)
                method = line.split(" ", 1)[0]
                if method == "INVITE":
                    seen["invite_headers"] = hdr
                    sock.sendto(build_ok(hdr, bind_host, port), peer)
                elif method == "ACK":
                    seen["ack_headers"] = hdr
                    return
    except Exception as exc:
        seen["error"] = repr(exc)
    finally:
        done.set()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--proxy", default="127.0.0.1")
    ap.add_argument("--proxy-port", type=int, default=5062)
    ap.add_argument("--uas-host", default="127.0.0.1")
    ap.add_argument("--uas-port", type=int, default=5092)
    args = ap.parse_args()
    done = threading.Event()
    seen = {}
    thread = threading.Thread(target=uas, args=(args.uas_host, args.uas_port,
                                               done, seen), daemon=True)
    thread.start()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as uac:
        uac.bind(("127.0.0.1", 0))
        uac.settimeout(8)
        port = uac.getsockname()[1]
        callid = "pv-headers-ack-test@localhost"
        source = f"<sip:origin@localhost>;tag=sent1"
        target = "<sip:target@localhost>"
        common = (f"From: {source}\r\nTo: {target}\r\n"
                  f"Call-ID: {callid}\r\nMax-Forwards: 70\r\n"
                  "Proxy-Authorization: Digest username=\"origin\", "
                  "realm=\"example\", nonce=\"test\", "
                  "uri=\"sip:target@localhost\", response=\"1234\"\r\n")
        via1 = f"Via: SIP/2.0/UDP 127.0.0.1:{port};branch=z9hG4bKpvtest1;rport\r\n"
        invite = ("INVITE sip:target@localhost SIP/2.0\r\n" + via1 + common
                  + f"CSeq: 1 INVITE\r\nContact: <sip:origin@127.0.0.1:{port}>\r\n"
                  + "Content-Length: 0\r\n\r\n")
        uac.sendto(invite.encode(), (args.proxy, args.proxy_port))
        while True:
            answer, _ = uac.recvfrom(65535)
            line, hdr = parse(answer)
            if line.startswith("SIP/2.0 200"):
                break
        route = "".join("Route: " + v + "\r\n"
                        for v in reversed(hdr.get("record-route", [])))
        via2 = f"Via: SIP/2.0/UDP 127.0.0.1:{port};branch=z9hG4bKpvtest2;rport\r\n"
        # ACK for a 2xx INVITE must target the remote Contact URI,
        # not the original Request-URI (which may not route to the UAS).
        contact = first(hdr, "contact")
        if not (contact.startswith("<sip:") and ">" in contact):
            raise AssertionError("200 OK missing Contact")
        contact_uri = contact[1:contact.index(">")]
        ack = ("ACK " + contact_uri + " SIP/2.0\r\n" + via2
               + common.replace(f"To: {target}\r\n",
                                "To: " + first(hdr, "to") + "\r\n")
               + route + "CSeq: 1 ACK\r\nContent-Length: 0\r\n\r\n")
        uac.sendto(ack.encode(), (args.proxy, args.proxy_port))

    if not done.wait(12):
        raise SystemExit("FAIL: ACK did not reach mock UAS; check proxy routing")
    if "error" in seen:
        raise SystemExit("FAIL: " + seen["error"])
    for method in ("invite", "ack"):
        if "proxy-authorization" in seen[method + "_headers"]:
            raise SystemExit("FAIL: Proxy-Authorization still present on " + method)
    print("PASS: Proxy-Authorization removed on both relayed INVITE and ACK")


if __name__ == "__main__":
    main()
