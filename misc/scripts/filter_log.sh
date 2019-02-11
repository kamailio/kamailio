#!/bin/sh
#
# $Id$
#
# tool for filtering SIP messages from log by a RegExp
#
# Example of use: ./filter_msg.sh /var/log/sip/sip.1056844800 'CallId: abc'
#


#####################

usage()
{
	echo "Usage: $0 <filename> <RegExp>"
}

if [ "$#" -ne 2 ] ; then
	usage
	exit
fi

AWK_PG='
BEGIN {
	IGNORECASE=1;
	line=0;
	msg_match=0;
}

/^#$/ {
	line=0
	msg_match=0
	next
}

msg_match==1 {
	print
	next
}

{ 
	if (match($0, RE)) {
		msg_match=1;
		# dump all accumulated lines here
		for (i=1; i<=line; i++) print buffer[i];
		print
		next
	}
	# there are still chances for a match in following lines;
	# keep buffering this request
	line++
	buffer[line]=$0
}

'


cat $1 | awk "$AWK_PG" RE="$2"
