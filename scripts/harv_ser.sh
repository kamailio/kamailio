#!/bin/sh
#
# $Id$
#
# tool for post-processesing captured SIP messages 
#
# call it without parameters to harvest the youngest
# log file or with "all" parameter to harvest all
# 
# you need to capture SIP messages first; you
# may for example run an init.d job such as
# ngrep -t port 5060 2>&1 | rotatelogs /var/log/sip 86400&
# caution: if you do that you best set up a crond daemon
# which deletes the files too -- they become huge
# very quickly
#
# note that the tool has no notion of messages and transactions
# yet; a consuquence of the former is that number of clients
# which do not identify themselves using User-Agent HF is 
# unknown (only lines which include it are processed);
# a consequence is also that relayed messages are
# counted twice (incoming, outgoing), and INVITEs are not
# correlated with BYEs
#


LOGDIR=/var/log

#####################

if [ "$1" = "all" ] ; then
	CURRENT=`ls -t $LOGDIR/sip.*`
else
	CURRENT=`ls -t $LOGDIR/sip.* | head -1`
fi
echo "Log: $CURRENT"

#cat $CURRENT | ./ser_harvest.awk 

AWK_PG='
BEGIN {
    rpl100=0; rpl180=0; rpl183=0; rpl1xx=0;
    rpl200=0; rpl202=0; rpl2xx=0;
    rpl300=0; rpl302=0; rpl3xx=0;
    rpl400=0; rpl401=0; rpl403=0; rpl404=0; rpl405=0;
        rpl407=0;rpl408=0;rpl410=0;
        rpl481=0;rpl486=0;rpl487=0;
        rpl4xx=0;
    rpl500=0;rpl501=0;rpl5xx=0;
    rpl603=0;rpl6xx=0;

	hint_imgw=0;
	hint_voicemail=0;
	hint_battest=0;
	hint_usrloc=0;
	hint_outbound=0;
	hint_sms=0;
	hint_gw=0;
	hint_off_voicemail=0;


    cancel=0;invite=0;ack=0; info=0;register=0;bye=0;
    options=0;
    message=0; subscribe=0; notify=0;

	ua_snom=0;
	ua_msn=0;
	ua_mitel=0;
	ua_pingtel=0;
	ua_ser=0;
	ua_osip=0;
	ua_linphone=0;
	ua_ubiquity=0;
	ua_3com=0;
	ua_ipdialog=0;
	ua_epygi=0;
	ua_jasomi=0;
	ua_cisco=0;
	ua_insipid=0;
	ua_hostip=0;
	ua_xx=0;

	ua=0;
}

{ua=0}

ua==0 && /User-Agent:.*snom/ {
	ua_snom++
	ua=1
}
ua==0 && /User-Agent:.*Windows RTC/ {
	ua_msn++
	ua=1
}
ua==0 && /User-Agent:.*Mitel/ {
	ua_mitel++
	ua=1
}
ua==0 && /User-Agent:.*Pingtel/ {
	ua_pingtel++
	ua=1
}
ua==0 && /User-Agent:.*Sip EXpress/ {
	ua_ser++
	ua=1
}
ua==0 && /User-Agent:.*oSIP-ua/ {
	ua_osip++
	ua=1
}
ua==0 && /User-Agent:.*oSIP\/Linphone/ {
	ua_linphone++
	ua=1
}
ua==0 && /User-Agent:.*3Com/ {
	ua_3com++
	ua=1
}
ua==0 && /User-Agent:.*ipDialog/ {
	ua_ipdialog++
	ua=1
}
ua==0 && /User-Agent:.*UbiquityUserAgent/ {
	ua_ubiquity++
	ua=1
}
ua==0 && /User-Agent:.*EPYGI/ {
	ua_epygi++
	ua=1
}
ua==0 && /User-Agent:.*Jasomi/ {
	ua_jasomi++
	ua=1
}
ua==0 && /User-Agent:.*Cisco/ {
	ua_cisco++
	ua=1
}
ua==0 && /User-Agent:.*Insipid/ {
	ua_insipid++
	ua=1
}
ua==0 && /User-Agent:.*Hotsip/ {
	ua_hotsip++
	ua=1
}

 { comment="hack to deal with old version of ngrep (breaking in columns)"
		 c="skip lines which words which frequently appeared on broken "
		 c="columns. should not affect non-broken logs"
 }


ua==0 && /(CANCEL|REGISTER|SUBSCRIBE|ACK|BYE|INVITE|REFER|OPTIONS|NOTIFY|sip-cc).*User-Agent:/ {
	ua=1
}

ua==0 && /User-Agent:/ {
	ua_xx++
	print
}



/P-hint: IMGW/ {
	hint_imgw++
}
/P-hint: VOICEMAIL/ {
	hint_voicemail++
}
/P-hint: BATTEST/ {
	hint_battest++
}
/P-hint: USRLOC/ {
	hint_usrloc++
}
/P-hint: OUTBOUND/ {
	hint_outbound++
}
/P-hint: SMS/ {
	hint_sms++
}
/P-hint: GATEWAY/ {
	hint_gw++
}
/P-hint: OFFLINE-VOICEMAIL/ {
	hint_off_voicemail++
}


/SIP\/2\.0 100/ {
    rpl100++
    next
}
/SIP\/2\.0 180/ {
    rpl180++
    next
}
/SIP\/2\.0 183/ {
    rpl183++
    next
}
/SIP\/2\.0 1[0-9][0-9]/ {
    print
    rpl1xx=0
    next
}


/SIP\/2\.0 200/ {
    rpl200++
    next
}
/SIP\/2\.0 202/ {
    rpl202++
    next
}
/SIP\/2\.0 2[0-9][0-9]/ {
    print
    rpl2xx++
    next
}

/SIP\/2\.0 300/ {
    rpl300++
    next
}
/SIP\/2\.0 302/ {
    rpl302++
    next
}
/SIP\/2\.0 3[0-9][0-9]/ {
    print
    rpl3xx++
    next
}

/SIP\/2\.0 400/ {
    rpl400++
    next
}
/SIP\/2\.0 401/ {
    rpl401++
    next
}
/SIP\/2\.0 403/ {
    rpl403++
    next
}
/SIP\/2\.0 404/ {
    rpl404++
    next
}
/SIP\/2\.0 405/ {
    rpl405++
    next
}
/SIP\/2\.0 407/ {
    rpl407++
    next
}
/SIP\/2\.0 408/ {
    rpl408++
    next
}
/SIP\/2\.0 410/ {
    rpl410++
    next
}
/SIP\/2\.0 481/ {
    rpl481++
    next
}
/SIP\/2\.0 486/ {
    rpl486++
    next
}
/SIP\/2\.0 487/ {
    rpl487++
    next
}
/SIP\/2\.0 4[0-9][0-9]/ {
    print
    rpl4xx++
    next
}


/SIP\/2\.0 500/ {
    rpl500++
    next
}
/SIP\/2\.0 501/ {
    rpl501++
    next
}
/SIP\/2\.0 5[0-9][0-9]/ {
    print
    rpl5xx++
    next
}

/SIP\/2\.0 603/{
	rpl603++
	next
}
/SIP\/2\.0 6[0=9][0-9]/ {
    print
    rpl6xx++
    next
}


/CANCEL sip/ {
    cancel++
    next
}
/INVITE sip/ {
    invite++
    next
}
/ACK sip/ {
    ack++
    next
}
/BYE sip/ {
    bye++
    next
}
/OPTIONS sip/ {
    options++
    next
}
/INFO sip/ {
    info++
    next
}
/MESSAGE sip/ {
    message++
    next
}
/SUBSCRIBE sip/ {
    subscribe++
    next
}
/NOTIFY sip/ {
    notify++
    next
}

END {
	print "## Reply Codes"
    print "100: " rpl100 " 180: " rpl180 " 183: " rpl183 " 1xx: " rpl1xx
    print "200: " rpl200 " 202: " rpl202 " 2xx: " rpl2xx
    print "300: " rpl300 " 302: " rpl302 " 3xx: " rpl3xx
    print "400: " rpl400 " 401: " rpl401 " 403: " rpl403 
		" 404: " rpl404 " 405: " rpl405
        " 407: " rpl407 " 408: " rpl408  " 410: " rpl410
        " 481: " rpl481 " 486: " rpl486 " 487: " rpl487
        " 4xx: " rpl4xx  " 3xx: " rpl3xx
    print "500: " rpl500 " 501: " rpl501 " 5xx: " rpl5xx
    print "603: " rpl603 " 6xx: " rpl6xx
	print "## Request Methods"
    print "INVITE: " invite " CANCEL: " cancel " ACK: " ack
    print "BYE: " bye " OPTIONS: " options " INFO: " info
    print "MESSAGE: " message " SUBSCRIBE: " subscribe " NOTIFY: " notify
	print "## Outbound Routes"
	print "To imgw: " hint_imgw " To voicemail: " hint_voicemail
	print "To bat: " hint_battest " To UsrLoc: " hint_usrloc
	print "Outbound: " hint_outbound " To SMS: " hint_sms
	print "To PSTN: " hint_gw " To: VM on off-line" hint_off_voicemail
	print "## User Agents"
	print "Snom: " ua_snom " MSN: " ua_msn " Mitel: " ua_mitel
	print "Pingtel: " ua_pingtel " SER: " ua_ser " osip: " ua_osip
	print "linphone: " ua_linphone " ubiquity: " ua_ubiquity
	print "3com: " ua_3com " IPDialog: " ua_ipdialog " Epygi: " ua_epygi
	print "Jasomi: " ua_jasomi " Cisco: " ua_cisco " insipid: " ua_insipid
	print "Hotsip: " ua_hotsip " UFO: " ua_xx
}
'


cat $CURRENT | awk "$AWK_PG"
