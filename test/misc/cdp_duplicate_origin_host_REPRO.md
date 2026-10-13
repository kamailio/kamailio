# CDP duplicate Origin-Host / peer isolation (#4981)

This is an **opt-in integration probe**, not a unit test. It creates three
Diameter peers with distinct Origin-Host FQDNs, sends CER and watchdog DWR,
then repeatedly connects a fourth peer using the first peer's Origin-Host.
The two unrelated peers must continue responding with DWA after each
duplicate identity attempt.

## Prerequisites

- Disposable Kamailio CDP instance configured to accept the test Origin-Hosts
  and Realm, and to process watchdogs. Default example realm is lab.invalid.
- The test address is 127.0.0.1:3868. Remote targets are blocked unless the
  operator explicitly sets --allow-remote.
- Run with verbose CDP logs and compare against **unpatched master**.
- Real-world fault may be timing-dependent; a passing run does not prove
  that all stale-descriptor EBADF paths are covered.

## Run

    python3 test/misc/cdp_duplicate_origin_host_probe.py \
      --host 127.0.0.1 --realm lab.invalid --duplicates 20 --run

The probe refuses to send any traffic without --run. It verifies a 2001
Diameter success code on each healthy CEA as a prerequisite. If the
test server refuses the CER or ignores DWR, update its configuration before
drawing conclusions about the fix.

## Additional regression matrix

1. Healthy connection close and normal peer deletion.
2. Duplicate Origin-Host CER while two unrelated peers remain healthy.
3. Stale TCP FD (close/EBADF) while healthy peers are active.
4. Interrupted select() (EINTR) without disconnecting peers.
5. mkfifo/open failures: no dangling serviced peer, leaked FIFO, or double-close.
6. TLS-enabled CDP build and shutdown on supported versions.

Capture baseline and patched peer states, logs, and sanitizer findings. No
daemon or live Diameter integration run has been claimed for this branch.
