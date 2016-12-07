#!/bin/sh
#
# $Id$
#
# Usage: ctd.sh $FROM $TARGET
# 
# click-to-dial example using REFER
#----------------------------------
#
# About:
# ------
# this script initiates a call from SIP user $FROM to SIP
# user $TARGET; it works as follows: a dummy user invites
# $FROM to a dummy "call on hold"; as soon as it is set up, the
# dummy user transfers $FROM to $TARGET  (REFER transaction)
# and terminates the dummy session established previously
# (BYE transaction). Note: the "dummy call" is used to
# make $FROM accept $REFER -- most of SIP phones do not
# accept REFER if no call has not been established yet.
#
# Requirements: 
# -------------
# - SER with FIFO server turned on and TM module loaded
#
# Limitations: 
# ------------
# it only works with UAs supporting REFER; it has been tested 
# with Cisco 7960, Mitel 5055, Grandstream and Pingtel; Windows 
# Messenger does not support REFER. Never tested on solaris. 
# Some cisco 7960 images don't work (in particular, POS30202
# doesnt, POS3-03-8-21 does)
#
# History:
# --------
# 2003-03-01 bug_fix: route set reversed
# 2003-02-27 dialog support completed (jiri)
# 2003-04-28 dialog info precomputed in SER (jiri)

#--------------------------------
# config: who with whom
# address of the final destination to which we want to transfer
# initial CSeq and CallId
if [ -z "$2" ]; then
	TARGET="sip:23@192.168.2.16"
	echo "destination unspecified -- taking default value $TARGET"
else
	TARGET="$2"
fi
# address of user wishing to initiate conversation
if [ -z "$1" ] ; then
	URI="sip:44@192.168.2.16"
	echo "caller unspecified -- taking default value $URI"
else
	URI="$1"
fi

#---------------------------------
# fixed config data
FIFO="/tmp/ser_fifo"
# address of controller
FROM="<sip:controller@foo.bar>"
CSEQ="1"
CALLIDNR=`date '+%s'`$$
CALLID="${CALLIDNR}.fifouacctd"
name="ctd_fifo_$$"
fifo_reply="/tmp/$name"
dlg="/tmp/$CALLID.dlg"
FIXED_DLG=`printf "From: $FROM;tag=$CALLIDNR\nCall-ID: $CALLID\nContact: <sip:caller@!!>"`
#----------------------------------

# generate parts of FIFO-request essential to forming
# subsequent in-dialog reuqests
# 
# limitations: parsing broken if <> in display names or
# line-folding used
filter_fl()
{

awk -F ' ' '
BEGIN { IGNORECASE=1; line=0; eoh=0;ret=1 }
END { exit ret; }

{line++; }

# line 1: status code
line==1 && /^2[0-9][0-9] / { ret=0;next; }
line==1 && /^[3-6][0-9][0-9] / { print; print $0 > "/dev/stderr"; next; }
line==1 { print "reply error"; print; next; } 

# skip body
/^$/ { eoh=1 }
eoh==1 { next }

# uri and outbound uri at line 2,3: copy and paste
line==2 || line==3 { print $0; next; }
# line 4: Route; empty if ".", copy and paste otherwise
line==4 && /^\.$/ { next; }
# if non-empty, copy and paste it
line==4 { print $0; next; }
# filter out to header field for use in next requests
/^(To|t):/ { print $0; next; }
# anything else will be ignored
{next} 
	' # end of awk script
} # end of filter_fl

#---------------------------
# main

# set up exit cleaner
trap "rm -f $dlg $fifo_reply; exit 1" 0

# set up FIFO communication

if [ ! -w $FIFO ] ; then # can I write to FIFO server?
	echo "Error opening ser's FIFO $FIFO"
	exit 1
fi
mkfifo $fifo_reply # create a reply FIFO
if [ $? -ne 0 ] ; then
	echo "error opening reply fifo $fifo_reply"
	exit 1
fi
chmod a+w $fifo_reply
# start reader now so that it is ready for replies
# immediately after a request is out
cat < $fifo_reply | filter_fl > $dlg  &
fifo_job="$!"

# initiate dummy INVITE with pre-3261 "on-hold"
# (note the dots -- they mean in order of appearance:
# outbound uri, end of headers, end of body; eventualy
# the FIFO request must be terminated with an empty line)
cat > $FIFO <<EOF

:t_uac_dlg:$name
INVITE 
$URI
.
$FIXED_DLG
To: <$URI>
CSeq: $CSEQ INVITE
Content-Type: application/sdp
.
v=0
o=click-to-dial 0 0 IN IP4 0.0.0.0
s=session
c=IN IP4 0.0.0.0
b=CT:1000
t=0 0
m=audio 9 RTP/AVP 0
a=rtpmap:0 PCMU/8000
.

EOF

# wait for reply 
wait $fifo_job # returns completion status of filter_fl
if [ "$?" -ne "0" ] ; then
	echo "invitation failed"
	exit 1
fi

echo "invitation succeeded"

# proceed to REFER now
if [ \! -r $dlg ] ; then
	echo "dialog broken"
	exit 1
fi
CSEQ=`expr $CSEQ + 1`

# start reader now so that it is ready for replies
# immediately after a request is out
cat < $fifo_reply | filter_fl > /dev/null  &
fifo_job="$!"

# dump the REFER request to FIFO server 
cat > $FIFO <<EOF

:t_uac_dlg:$name
REFER
`cat $dlg`
$FIXED_DLG
CSeq: $CSEQ REFER
Referred-By: $FROM
Refer-To: $TARGET
.
.

EOF

# report REFER status
wait $fifo_job
ref_ret="$?"

if [ "$ref_ret" -ne "0" ] ; then
	echo "refer failed"
else
	echo "refer succeeded"
fi


# well, URI is trying to call TARGET but still maintains the
# dummy call we established with previous INVITE transaction:
# tear it down


# dump the BYE request to FIFO server 
CSEQ=`expr $CSEQ + 1`
cat < $fifo_reply | filter_fl > /dev/null  &
fifo_job="$!"
cat > $FIFO <<EOF

:t_uac_dlg:$name
BYE
`cat $dlg`
$FIXED_DLG
CSeq: $CSEQ BYE
.
.

EOF

# report BYE status
wait $fifo_job
ret="$?"

if [ "$ret" -ne "0" ] ; then
	echo "bye failed"
	exit 1
fi
echo "bye succeeded"

# clean-up
trap 0
rm -f $dlg $fifo_reply
exit $ref_ret
