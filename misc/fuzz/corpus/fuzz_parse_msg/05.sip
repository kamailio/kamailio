MESSAGE sip:alice@atlanta.example.com SIP/2.0
Via: SIP/2.0/UDP x;branch=z9hG4bK2
From: <sip:bob@b.org>;tag=9
To: <sip:alice@a.org>
Call-ID: msg-1@b
CSeq: 1 MESSAGE
Content-Type: multipart/mixed;boundary="BOUND"
Content-Length: 100

--BOUND
Content-Type: text/plain

hello
--BOUND
Content-Type: application/isup;base64=ITU95

AAAA
--BOUND--
