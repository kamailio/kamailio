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
# SER with FIFO server turned on and TM module loaded
#
# Limitations: 
# ------------
# it only works with UACs supporting REFER; it has been tested 
# with Cisco 7960 and Mitel 5055; Windows Messenger does not
# support REFER; dialog parser over-simplified (see inline) but 
# quite functional (if there is something to be fixed, it is
# richness of SIP syntax); an awk-only rewrite would be esthetically
# nicer, imho 
#
# History:
# --------
# 2003-02-27 dialog support completed (jiri)

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
	URI="sip:113311@192.168.2.16"
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
BEGIN { IGNORECASE=1; rri=0; line=0; ret=1;eoh=0 }
END { # print dialog information a la RFC3261, S. 12.2.1.1
	# calculate route set 
	if (rri>0) { # route set not empty
		# next-hop loose router?
		if (match(rr[1], ";lr")) {
			ruri=rcontact
			nexthop=rr[1]
			rrb=1 # begin from first
		} else { # next-hop strict router
			ruri=rr[1]
			rrb=2 # skip first
			rri++
			rr[rri]=rcontact
			nexthop="." # t_uac_dlg value for "use ruri"
		}
	} else { # no record-routing
			ruri=rcontact
			nexthop="."
			rrb=1 # do not enter the loop
	}
	# print the FIFO request header
	print ruri
	print nexthop
	print to
	for(i=rrb; i<=rri; i++ ) {
		if (i==rrb) printf "Route: "; else printf ", "
		printf("%s", rr[i])
		if (i==rri) printf("\n")
		
	}
	exit ret
}

# set true (0) to return value if transaction completed succesfully
{line++; }
line==1 && /^2[0-9][0-9] / { ret=0; next; }
line==1 && /^[3-6][0-9][0-9] / { print; next; }
line==1 { print "reply error"; print; next; } 

# skip body
/^$/ { eoh=1 }
eoh==1 { next }

# collect dialog state: contact, rr, to
/^(Contact|m):/ { 
	# contact is <>-ed; fails if < within quotes
	if (match($0, "^(Contact|m):[^<]*<([^>]*)>", arr)) {
		rcontact=arr[2]
		next
	# contact without any extras and without <>, just uri
	} else if (match($0, "^(Contact|m):[ \t]*([^ ;\t]*)", arr)) {
		rcontact=arr[2]
		next
	} else {
		# contact parsing error
		ret=1
	}
}

/^Record-Route:/ {
	# rr is always <>-ed; fails if <> within quotes
	srch=$0
	while (match(srch, "[^<]*<([^>]*)>", arr )) {
		rri++
		rr[rri]=arr[1]
		srch=substr(srch,RLENGTH)
	}
}

/^(To|t):/ {
	to=$0;
}

{next} # do not print uninteresting header fields
	' # end of awk script
} # end of filter_fl

#---------------------------
# main

# set up exit cleaner
trap "rm -f $dlg $fifo_reply; exit" 0

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
ret="$?"

if [ "$ret" -ne "0" ] ; then
	echo "refer failed"
	exit 1
fi

echo "refer succeeded"

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
