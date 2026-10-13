#!/usr/bin/env python3
"""Measure SHM growth around locally dropped DMQ requests (Kamailio #5001).

The operator supplies an isolated-test-only command to generate dropped KDMQ
traffic; this script never generates production traffic itself. Requires
kamcmd core.shmmem, and is NOT proof of a leak in isolation.
"""
import argparse
import json
import re
import shlex
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--trigger", required=True,
    help="Command generating N dropped KDMQ requests; include {count}")
parser.add_argument("--count", type=int, default=10000)
parser.add_argument("--warmup", type=int, default=1000)
parser.add_argument("--settle", type=float, default=2.0)
parser.add_argument("--max-growth-per-request", type=float, default=4.0,
    help="Diagnostic threshold; background memory noise varies")
parser.add_argument("--kamcmd", default="kamcmd")
args = parser.parse_args()
if args.count <= 0 or args.warmup < 0 or args.settle < 0:
    parser.error("count must be positive; warmup and settle must not be negative")
if "{count}" not in args.trigger:
    parser.error("--trigger requires literal {count} placeholder")

def snapshot():
    out = subprocess.check_output([args.kamcmd, "core.shmmem"], text=True)
    for key in ("real_used", "used"):
        match = re.search(r"(?m)^\s*" + key + r"\s*[:=]\s*([0-9]+)", out)
        if match:
            return int(match.group(1))
    raise RuntimeError("No used-size field in kamcmd core.shmmem:\n" + out)

def trigger(n):
    command = [part.replace("{count}", str(n))
               for part in shlex.split(args.trigger)]
    if not command:
        raise RuntimeError("empty trigger command")
    print("Running", " ".join(shlex.quote(x) for x in command), flush=True)
    subprocess.run(command, check=True)

if args.warmup:
    trigger(args.warmup)
    time.sleep(args.settle)
baseline = snapshot()
trigger(args.count)
time.sleep(args.settle)
after_first = snapshot()
trigger(args.count)
time.sleep(args.settle)
after_second = snapshot()
growth_1 = after_first - baseline
growth_2 = after_second - after_first
result = {
    "baseline_real_used_bytes": baseline,
    "after_first_real_used_bytes": after_first,
    "after_second_real_used_bytes": after_second,
    "requests_per_round": args.count,
    "growth_bytes_round1": growth_1,
    "growth_bytes_round2": growth_2,
    "growth_per_request_round1": growth_1 / args.count,
    "growth_per_request_round2": growth_2 / args.count,
    "threshold_bytes_per_request": args.max_growth_per_request,
    "pass_heuristic": all(x / args.count <= args.max_growth_per_request
                          for x in (growth_1, growth_2)),
}
print(json.dumps(result, indent=2))
if not result["pass_heuristic"]:
    raise SystemExit(1)
