#!/bin/sh
#
# $Id$
#
# collect SIP message dump statistics
#
# call it without parameters to harvest the youngest
# log file or with "all" parameter to harvest all
# 
# (you need to capture SIP messages first; you
# may for example run an init.d job such as
# ngrep -t port 5060 2>&1 | rotatelogs /var/log/sip 86400&
#


LOGDIR=/var/log

#####################

if [ "$1" = "all" ] ; then
	CURRENT=`ls -t $LOGDIR/sip.*`
else
	CURRENT=`ls -t $LOGDIR/sip.* | tail -1`
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
    rpl500=0;rpl5xx=0;
    rpl6xx=0;


    cancel=0;invite=0;ack=0; info=0;register=0;bye=0;
    options=0;
    message=0; subscribe=0; notify=0;
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
/SIP\/2\.0 5[0-9][0-9]/ {
    print
    rpl5xx++
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
    print "100: " rpl100 " 180: " rpl100 " 1xx: " rpl1xx
    print "200: " rpl200 " 202: " rpl202 " 2xx: " rpl2xx
    print "300: " rpl300 " 302: " rpl302 " 3xx: " rpl3xx
    print "400: " rpl400 " 401: " rpl401 " 403: " rpl403 
		" 404: " rpl404 " 405: " rpl405
        " 407: " rpl407 " 408: " rpl408  " 410: " rpl410
        " 481: " rpl481 " 486: " rpl486 " 487: " rpl487
        " 4xx: " rpl4xx  " 3xx: " rpl3xx
    print "500: " rpl500 " 5xx: " rpl5xx
    print "6xx: " rpl6xx
    print "INVITE: " invite " CANCEL: " cancel " ACK: " ack
    print "BYE: " bye " OPTIONS: " options " INFO: " info
    print "MESSAGE: " message " SUBSCRIBE: " subscribe " NOTIFY: " notify
}
'


cat $CURRENT | awk "$AWK_PG"
