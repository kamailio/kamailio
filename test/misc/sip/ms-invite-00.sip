OPTIONS sip:jirim@bat.iptel.org SIP/2.0
Max-Forwards: 10
Via: SIP/2.0/UDP 195.37.77.100:9;branch=0e413351aee2246f9f1edac77ccd39bc.e1.0
From: "jiri" <sip:jiri@iptel.org>;tag=ae2c8aec-fd00-4f0c-98b9-6d40b72c950c
To: <sip:jirim@bat.iptel.org>
Call-ID: 83cd6524-386b-4e78-8975-ab96a0f78590@212.202.173.36
CSeq: 2 OPTIONS
Contact: <sip:212.202.173.36:9310>
User-Agent: Windows RTC/1.0
Content-Type: application/sdp
Content-Length: 453

v=0
o=jku2 0 0 IN IP4 212.202.173.36
s=session
c=IN IP4 212.202.173.36
b=CT:1000
t=0 0
m=audio 16090 RTP/AVP 97 111 112 6 0 8 4 5 3 101
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
a=rtpmap:3 GSM/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-16
