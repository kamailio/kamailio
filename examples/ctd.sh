#!/bin/sh 
#
# $Id$
# 
# (currently lame) click-to-dial example using REFER
#----------------------------------------------------
#
# it half-way works using FIFO/uac; I tested it with
# Cisco 7960 reaching Mitel 5055 and vice versa
#
# bugs: 
# -----
# - to-tag in REFER not ok -- should be gained from initial 
#   conversation (need to take fifo processing from sc) as
#   well as the whole dialog stuff (awk could be a better hit)
# - remove user prompt (act on receipt of a reply)
# - bye is missing after REFER
# - it would be cleaner to send "hold" in initial invite
# - in my test setup, initial ACK is for some bizzar reason
#   not forwarded statelessly by outbound proxy
# - put this example in serdoc

#URI="sip:113311@192.168.2.16"
# address of user wishing to initiate conversation
TARGET="sip:23@192.168.2.16"
FIFO="/tmp/ser_fifo"
# address of controller
FROM="<sip:caller@foo.bar>"
# address of the final destination to which we want to transfer
URI="sip:113311@192.168.2.16"
# initial CSeq and CallId
CSEQ="1"
CALLIDNR=`date '+%s'`$$

#----------------------------------

CALLID="${CALLIDNR}.fifouacctd"

cat > $FIFO <<EOF


:t_uac_dlg:qqq
INVITE 
$URI
.
From: $FROM;tag=$CALLIDNR
To: <$URI>
Call-ID: $CALLID
CSeq: $CSEQ INVITE
Contact: <sip:caller@!!>
Content-Type: application/sdp
.
v=0
o=jku2 0 0 IN IP4 213.20.128.35
s=session
c=IN IP4 213.20.128.35
b=CT:1000
t=0 0
m=audio 54742 RTP/AVP 97 111 112 6 0
a=rtpmap:97 red/8000
a=rtpmap:111 SIREN/16000
a=fmtp:111 bitrate=16000
a=rtpmap:112 G7221/16000
a=fmtp:112 bitrate=24000
a=rtpmap:6 DVI4/16000
a=rtpmap:0 PCMU/8000
.
EOF

read -p "press any key to initiate transfer: "

CSEQ=`expr $CSEQ + 1`

cat > $FIFO <<EOF


:t_uac_dlg:qqq
REFER
$URI
.
From: $FROM;tag=$CALLIDNR
To: <$URI>
Call-ID: $CALLID
CSeq: $CSEQ REFER
Contact: <sip:caller@!!>
Referred-By: $FROM
Refer-To: $TARGET
.
.
EOF
