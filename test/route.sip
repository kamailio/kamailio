INVITE sip:janakj@megalith.sh.cvut.cz SIP/2.0
Via: SIP/2.0/UDP 147.32.119.187:5040
From: "Jan Janak" <sip:janakj@megalith.sh.cvut.cz>;tag=b96b0300ed30f1286-2f5d
To: <sip:janakj2@megalith.sh.cvut.cz>
Call-ID: b96b0300-88d30f-66da-63aa@195.37.78.190
CSeq: 101 INVITE
Expires: 180
Route: <sip:aa@mail.ok2rab.nextra.cz:5061>,
       <sip:bb@mail.ok2rab.nextra.cz:5062>
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
