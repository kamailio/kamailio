# DMQ local-request-drop SHM regression (#5001)

This **manual integration harness** measures allocator growth, not raw packet counts.
It does not generate KDMQ messages itself. Prepare a disposable two-node DMQ
deployment with a local-request route dropping only the test KDMQ
requests, plus a sender command that reliably produces N such requests.

## Steps

1. Build patched Kamailio (dmq, tm, ctl) with SHM debug. Verify normal
   KDMQ requests complete and are cleaned up without double-free.
2. Configure a generator that creates exactly N locally dropped KDMQ requests.
   **Do not run against production or uncontrolled peers.**
3. Execute:

       python3 test/misc/dmq_drop_shm_probe.py \
         --trigger './send_test_kdmq.sh --count {count}' \
         --count 10000 --warmup 1000 --max-growth-per-request 4

4. Compare JSON output against the **same test on unpatched master**.
   Log kamcmd core.shmmem before and after, check free SHM, allocator stats,
   worker restart count, and DMQ/TM errors.
5. Test normal request completion, local drop and TM error cleanup under
   AddressSanitizer/valgrind where supported.

The threshold is a heuristic, not a proof: background traffic, caches, and
fragmentation can alter memory between snapshots. Repeat matched patched vs
unpatched tests. Manual execution is needed because this repository does not
include a hermetic two-node DMQ environment.
