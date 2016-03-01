MESSAGE sip:bob@example.invalid SIP/2.0
From: sip:alice@example.invalid;tag=45dfdf49
To: sip:bob@example.invalid
Call-ID: 1172299593a
CSeq: 1 MESSAGE
Content-Type: application/sdp
Max-Forwards: 2
X-Case: 60-test7
X-Info: sdpops remove_line_by_prefix() test7 - change s -line

v=0
o=- 370010 0 IN IP4 192.168.13.31
s=MGW
c=IN IP4 192.168.13.31
t=0 0
m=audio 22616 RTP/AVP 0 96 100
a=rtpmap:96 telephone-event/8000
a=rtpmap:100 X-NSE/8000

