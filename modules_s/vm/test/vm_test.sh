#!/bin/bash

loop=5
max_speed=30
SIP_DST="sip:raf@192.168.0.1"
alarm=0
SER_FIFO="/tmp/ser_fifo"
SIP_FROM="sip:c@192.168.0.1"

cat > $SER_FIFO << EOF
:vm_uac_dlg:null
INVITE
$SIP_DST
$SIP_DST
<$SIP_DST>

<$SIP_FROM>
12345-tag-12345
100
1234576098621
Content-Type: application/sdp
Contact: <sip:c@192.168.0.11:5060>

v=0
o=username 0 0 IN IP4 192.168.0.1
s=session
c=IN IP4 192.168.0.1
t=0 0
m=audio 30000 RTP/AVP 0
a=rtpmap:0 PCMU/8000
.

EOF

