# received-from flag ordering regression

Issue: https://github.com/kamailio/kamailio/issues/4983

The runtime parsing bug has an upstream candidate fix by maintainer
`c1a29f31d8dc5dc41b02c96c4fd5930c7131ff99` (branch `vseva/4983`).
**This contribution is only a regression capture tool, not a duplicate fix.**

## Setup

Configure Kamailio `rtpengine_sock` to `udp:127.0.0.1:2223`.
Run `test/misc/rtpengine_received_from_capture.py` and send an INVITE
with valid SDP so that a disposable route invokes `rtpengine_offer()`.
The harness captures and decodes one NG message and asserts the
`received-from` list; it replies with an NG error, so it is not a media server.

Run four combinations in separate sessions:

| Flags | Harness options |
|---|---|
| `received-from=198.51.100.7 replace-origin` | `--expect-family IP4 --expect-ip 198.51.100.7` |
| `replace-origin received-from=198.51.100.7` | `--expect-family IP4 --expect-ip 198.51.100.7` |
| `received-from=2001:db8::7 replace-origin` | `--expect-family IP6 --expect-ip 2001:db8::7` |
| `replace-origin received-from=2001:db8::7` | `--expect-family IP6 --expect-ip 2001:db8::7` |

Expected: pre-fix master fails the non-final flag cases. The maintainer's fix
should make the results equal across flag orderings.

Before proposing this test upstream, integrate it into the Kamailio test
framework and run it against the patch branch. It is currently a manual tool.
