MESSAGE sip:bob@example.invalid SIP/2.0
From: sip:alice@example.invalid;tag=45dfdf49
To: sip:bob@example.invalid
Call-ID: 1172299593a
CSeq: 1 MESSAGE
Content-Type: application/sdp
Max-Forwards: 2
X-Case: 60-test9
X-Info: sdpops remove_line_by_prefix test9 - filter video attribute

v=0
o=FreeSWITCH 1447318678 1447318679 IN IP4 10.0.0.145
s=FreeSWITCH
c=IN IP4 10.0.0.145
t=0 0
m=audio 28504 RTP/AVP 96 101
a=rtpmap:96 opus/48000/2
a=fmtp:96 useinbandfec=1; maxaveragebitrate=14400; maxplaybackrate=8000
a=rtpmap:101 telephone-event/48000
a=fmtp:101 0-16
a=ptime:20
a=sendrecv
a=rtcp:28505 IN IP4 10.0.0.145
a=rtcp-fb:101 nack
m=video 21992 RTP/SAVPF 100
a=rtpmap:100 VP8/90000
a=rtcp:21992 IN IP4 10.0.0.150
a=rtcp-fb:100 ccm fir
a=rtcp-fb:100 nack
a=rtcp-fb:100 nack pli

