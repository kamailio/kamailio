INVITE sip:p2@iptel.org SIP/2.0
Via: SIP/2.0/UDP 193.175.133.193
From: "GMD FOKUS iptlab" <sip:jiri@iptel.org>;tag=b96b0300ed30f1286-2f5d
To: <sip:p2@iptel.org>
Call-ID; b96b0300-88d30f-66da-63aa@195.37.78.190
CSeq: 101 INVITE
Expires: 180
User-Agent: Cisco-SIP-IP-Phone/2
Accept: application/sdp
Contact: sip:jiri@195.37.78.190:5060
Content-Type: application/sdp
Content-Length: 225


v=0
o=CiscoSystemsSIP-IPPhone-UserAgent 14474 8233 IN IP4 195.37.78.190
s=SIP Call
c=IN IP4 195.37.78.190
t=0 0
m=audio 18456 RTP/AVP 0 8 18 101
a=rtpmap:0 pcmu/8000
a=rtpmap:101 telephone-event/8000
a=fmtp:101 0-11 
