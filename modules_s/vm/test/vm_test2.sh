#!/bin/bash

loop=5
max_speed=30
SIP_DST="sip:raf@192.168.0.1"
alarm=0
SER_FIFO="/tmp/ser_fifo"
SIP_FROM="sip:pape@iptel.org"

cat > $SER_FIFO << EOF
:t_uac_dlg:null
INVITE
$SIP_DST
$SIP_DST
Route: <sip:proxy.rafhome.net>; <sip:proxy2.rafhome.net;lr>
Content-Type: application/sdp
Contact: <$SIP_FROM>
CSeq: 100 INVITE
To: <$SIP_DST>
From: "le Pape" <$SIP_FROM>;tag="12345-tag-12345"
.
v=0
o=username 0 0 IN IP4 192.168.0.1
s=session
c=IN IP4 192.168.0.1
t=0 0
m=audio 30000 RTP/AVP 0
a=rtpmap:0 PCMU/8000
.

EOF

