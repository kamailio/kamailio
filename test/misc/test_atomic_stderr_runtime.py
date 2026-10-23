#!/usr/bin/env python3
"""End-to-end Kamailio multi-worker stderr-pipe regression for issue #4980.

Build src/kamailio first, then run:
    python3 test/misc/test_atomic_stderr_runtime.py ./src/kamailio

The test is intentionally opt-in: it launches a standalone Kamailio with
four SIP workers on 127.0.0.1 and uses a disposable runtime directory.
Never run it against an installed production instance.
"""
import argparse
import concurrent.futures
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path, help="Built src/kamailio binary")
parser.add_argument("--per-worker", type=int, default=500,
                    help="Packets per generator; 4 generators by default")
args = parser.parse_args()
binary = args.binary.resolve()
if not binary.is_file():
    parser.error(f"Kamailio binary does not exist: {binary}")
if args.per_worker < 50 or args.per_worker > 10000:
    parser.error("--per-worker must be 50..10000")
expected = 4 * args.per_worker

# A random loopback port avoids collisions with an installed SIP daemon.
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]

marker = "KSR_ATOMIC_BEGIN_" + "X" * 320 + "_KSR_ATOMIC_END"
marker_bytes = marker.encode("ascii")
with tempfile.TemporaryDirectory(prefix="kamailio-atomic-") as td:
    # Source builds may default to /var/run/kamailio, not user-writable.
    runtime_dir = Path(td) / "run"
    runtime_dir.mkdir(mode=0o700)
    runtime_args = [str(binary), "-Y", str(runtime_dir)]
    cfg = Path(td) / "kamailio.cfg"
    cfg.write_text(
        "#!KAMAILIO\n"
        "debug=3\n"
        "log_stderror=yes\n"
        "fork=yes\n"
        "children=4\n"
        "auto_aliases=no\n"
        "disable_tcp=yes\n"
        f"listen=udp:127.0.0.1:{port}\n"
        "request_route {\n"
        f'    log(1, "{marker}\\n");\n'
        "    exit;\n"
        "}\n",
        encoding="ascii",
    )

    check = subprocess.run(
        [*runtime_args, "-c", "-f", str(cfg)],
        text=True, capture_output=True, timeout=10,
    )
    if check.returncode != 0:
        print("FAIL: Kamailio rejected test config", file=sys.stderr)
        print(check.stdout[-1800:] + check.stderr[-1800:], file=sys.stderr)
        raise SystemExit(1)

    # -DD keeps the parent in foreground while forking SIP workers.
    # PIPE, not a regular file: this is what exposes interleaved write(2)s.
    proc = subprocess.Popen(
        [*runtime_args, "-DD", "-E", "-T", "-n", "4", "-f", str(cfg)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        start_new_session=True,
    )
    collected = []

    def drain():
        try:
            for line in proc.stderr:
                collected.append(line)
        finally:
            proc.stderr.close()

    reader = threading.Thread(target=drain, daemon=True)
    reader.start()

    def sender(worker):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            for n in range(args.per_worker):
                call_id = f"test-{worker}-{n}-{uuid.uuid4().hex}@localhost"
                message = (
                    f"OPTIONS sip:probe@127.0.0.1:{port} SIP/2.0\r\n"
                    f"Via: SIP/2.0/UDP 127.0.0.1:5069;branch=z9hG4bK{uuid.uuid4().hex};rport\r\n"
                    "Max-Forwards: 70\r\n"
                    "From: <sip:test@localhost>;tag=12345\r\n"
                    "To: <sip:probe@localhost>\r\n"
                    f"Call-ID: {call_id}\r\n"
                    "CSeq: 1 OPTIONS\r\n"
                    "Content-Length: 0\r\n\r\n"
                ).encode("ascii")
                sock.sendto(message, ("127.0.0.1", port))
                time.sleep(0.0015)

    try:
        time.sleep(1.0)
        if proc.poll() is not None:
            raise RuntimeError("Kamailio exited before test traffic")
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
            list(executor.map(sender, range(4)))
        time.sleep(1.0)
    finally:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            proc.wait(timeout=6)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait(timeout=4)
        reader.join(timeout=5)

    relevant = [
        line for line in collected
        if b"KSR_ATOMIC_BEGIN_" in line or b"_KSR_ATOMIC_END" in line
    ]
    corrupt = [
        line for line in relevant
        if line.count(b"KSR_ATOMIC_BEGIN_") != 1
        or line.count(b"_KSR_ATOMIC_END") != 1
        or marker_bytes not in line
    ]
    worker_pids = {
        match.group(1)
        for line in relevant
        if (match := re.search(rb"\d+\((\d+)\)", line))
    }
    complete = len(relevant) - len(corrupt)
    print(f"SIP packets sent: {expected}")
    print(f"Complete marker-bearing log records: {complete}")
    print(f"Corrupted/interleaved marker-bearing records: {len(corrupt)}")
    print(f"Distinct worker PIDs in marker logs: {len(worker_pids)}")
    if corrupt:
        print("Example corrupted log record:",
              corrupt[0][:600].decode(errors="replace"))
    # The exact-count assertion prevents a false PASS when UDP traffic or
    # log records are dropped; rerun under lower load if delivery is unstable.
    if complete != expected or corrupt or len(worker_pids) != 4:
        print("FAIL: dropped, duplicated, or interleaved records, "
              "or insufficient multi-worker coverage")
        if complete != expected:
            print("Recent process output:")
            for line in collected[-12:]:
                print(line[:250].decode(errors="replace").rstrip())
        raise SystemExit(1)
    print("PASS: real Kamailio, four workers, complete stderr-pipe records")
