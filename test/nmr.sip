INVITE sip:**@backup01.sipphone.com SIP/2.0
Record-Route: <sip:**@195.37.77.101;ftag=2726874358;lr=on>
Via: SIP/2.0/UDP 195.37.77.101;branch=z9hG4bK2e69.ceb85f87.0
Via: SIP/2.0/UDP 192.168.0.177:5060;received=66.27.63.253;rport=10344;branch=z9hG4bK98761FBE466C4882B9A816285384E1EA
From: 8..123 <sip:876543210123@iptel.org>;tag=2726874358
To: <sip:**@backup01.sipphone.com>
Contact: <sip:876543210123@66.27.63.253:10344>
Call-ID: 7937AAC5-AE9E-41BA-9918-EF0C9DD96B37@192.168.0.177
CSeq: 38975 INVITE
Proxy-Authorization: Digest username="876543210123",realm="iptel.org",nonce="3fb9dd3e335d491d6d08f57b8b1a0a0bb83443a4",response="87da01419bc07a9ba1a76d63b993ae5d",uri="sip:**@backup01.sipphone.com"
Max-Forwards: 69
Content-Type: application/sdp
User-Agent: X-Lite build 1084
Content-Length: 301
P-hint: OUTBOUND
P-Behind-NAT: Yes

v=0
o=876543210123 1531591932 1531591932 IN IP4 192.168.0.177
s=X-Lite
c=IN IP4 195.37.77.101
t=0 0
m=audio 35274 RTP/AVP 0 8 3 98 101
a=rtpmap:0 pcmu/8000
a=rtpmap:8 pcma/8000
a=rtpmap:3 gsm/8000
a=rtpmap:98 iLBC/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-15
a=direction:active
