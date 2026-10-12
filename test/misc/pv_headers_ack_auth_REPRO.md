# pv_headers auto_msg and ACK after 200 OK — issue #4904

This standalone integration harness starts a local UDP UAS at
`127.0.0.1:5092` and sends an INVITE from a disposable caller through a
locally configured Kamailio proxy at `127.0.0.1:5062`. The UAS responds
200 OK with Contact and Record-Route; the caller sends an end-to-end ACK
containing `Proxy-Authorization`. The harness checks whether both INVITE
and ACK were forwarded *without* that header.

Use a disposable Kamailio config (not a production proxy) with:
- `tm`, `sl`, `pv`, `pv_headers`, `rr` loaded.
- `modparam("pv_headers", "auto_msg", 1)` for the first run.
- In INVITE request routing: call `record_route()` and
  `pvh_remove_header("Proxy-Authorization", 0)` before `t_relay()`,
  forwarding the request to `sip:127.0.0.1:5092`.
- In ACK routing: repeat `pvh_remove_header(...)`, call `loose_route()`
  where appropriate and relay the ACK to the test UAS.
- Start the test after Kamailio binds port 5062:
  `python3 test/misc/pv_headers_ack_auth_repro.py`

Repeat with explicit `pvh_apply_headers()` before forwarding, and then
`auto_msg=0` with manual collect/apply. Capture the forwarded packets
and logs to distinguish callback timing from route configuration.

The test uses intentionally fake authorization credentials. It does not
require or test authentication acceptance. A failing ACK route can also
make the test fail: examine SIP traces before attributing failures to
`pv_headers`.

**This is a diagnostic harness only, not a source fix**. The reported
issue's root cause is still under investigation.
