INVITE sip:jiri@bat.iptel.org SIP/2.0
Max-Forwards: 10
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=1848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=2848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=3848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=8848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=9848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=5848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=6848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=7848eaf6f622ebaf1762f34e5d08ec2e.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=b03d645a795fe2572631dbe15438fabd.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=59c7c421d731de1ce1e1e4c6e83150f2.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=3b33b0d029964ec5330922ff50b34c79.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=781aa36a12b7b99b2316a8ed32123fc9.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=a93680d322702022416a5a661aaf573d.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=5a94858405aeaae679632efed7e1ff0b.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=70509dba8d202a0116d2e51cd474a1d4.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=7056d40a2cfa30ff299705fc791f4303.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=ac7cf460ba831233121eead26af76d5a.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=2c4d2ac0b10115c704d28c2b360bcb6f.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=e0282bc88aed37191911cbb5ded27379.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=c7d6a97d4b6762877410653db5643775.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=a0c4944760b9f9693209f719e7aa177c.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=4147b5297bc5a8e0c3ec58fe6fd9bf6c.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=557cfe12fa9372a0a309ffebcdef3110.836.0
Via: SIP/2.0/UDP 195.37.77.100;branch=ce741cd8b427d1722d4f746922a9d3a2.836.0
Via: SIP/2.0/UDP 195.37.77.101;branch=838cccacb7d33e12c5e097f9090adfe1.836.0
Via: SIP/2.0/UDP 213.20.128.35:9315
From: "jiri" <sip:jiri@iptel.org>;tag=76ff7a07-c091-4192-84a0-d56e91fe104f
To: <sip:jiri@bat.iptel.org>
Call-ID: d10815e0-bf17-4afa-8412-d9130a793d96@213.20.128.35
CSeq: 2 INVITE
Contact: <sip:213.20.128.35:9315>
User-Agent: Windows RTC/1.0
Proxy-Authorization: Digest username="jiri", realm="iptel.org", algorithm="MD5", uri="sip:jiri@bat.iptel.org", nonce="3cef753900000001771328f5ae1b8b7f0d742da1feb5753c", response="53fe98db10e1074b03b3e06438bda70f"
Content-Type: application/sdp
Content-Length: 451

v=0
o=jku2 0 0 IN IP4 213.20.128.35
s=session
c=IN IP4 213.20.128.35
b=CT:1000
t=0 0
m=audio 54742 RTP/AVP 97 111 112 6 0 8 4 5 3 101
a=rtpmap:97 red/8000
a=rtpmap:111 SIREN/16000
a=fmtp:111 bitrate=16000
a=rtpmap:112 G7221/16000
a=fmtp:112 bitrate=24000
a=rtpmap:6 DVI4/16000
a=rtpmap:0 PCMU/8000
a=rtpmap:8 PCMA/8000
a=rtpmap:4 G723/8000
a=rtpmap:5 DVI4/8000
a=rtpmap: 3 GSM/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-16
