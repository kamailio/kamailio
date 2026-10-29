INVITE sip:svc@example.com SIP/2.0
Via: SIP/2.0/UDP x;branch=z9hG4bK4
From: <sip:a@a>;tag=1
To: <sip:s@e>
Call-ID: p1
CSeq: 1 INVITE
P-Asserted-Identity: <sip:+1234@e.org>;party=calling;idtype=gon
Privacy: id;header=id
Diversion: <sip:orig@e.org>;reason=unconditional;counter=1
Authorization: Digest username="a", realm="r", nonce="n", uri="sip:e", response="d"
Content-Length: 0

