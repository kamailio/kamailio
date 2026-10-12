# topos + uac_auth + dialog CSeq mismatch reproducer (#4837)

Run `topos_uac_cseq_challenge_uas.py` as a disposable upstream UAS on
`127.0.0.1:5090`. It issues a 401 Digest challenge on the first INVITE,
then checks the authenticated INVITE has the **same Call-ID**, **higher CSeq**,
**new Via branch** and an Authorization header, before replying 100/180/200.

Configure a local Kamailio SIP proxy with `tm`, `topos`, `uac`, and
`dialog`, `track_cseq_updates=1`, plus a `failure_route` that invokes
`uac_auth()` then `t_relay()`. Forward to the test UAS. Use credentials
matching the test challenge: realm `topos-test` and a disposable username/
password in the `uac` module configuration.

Run two setups, keeping all other options fixed:
1. Module load order: `topos`, `uac`, `dialog`.
2. Module load order: `dialog`, `uac`, `topos`.

Capture both sides with `sngrep` or `tcpdump`, plus debug=3 Kamailio logs.
The Python UAS validates only the upstream request sequence: **a PASS from this
script does NOT prove that downstream 100/180/200 match a Kamailio TM
transaction**. You must also verify the originating caller receives them,
`t_check_trans()` matches, and no spurious INVITE retransmits occur.

Root cause is not established. This change introduces only a diagnostic UAS,
not a change to the topos/CSeq state machine. It is not merge-ready without
automated end-to-end assertions in the Kamailio test suite.
