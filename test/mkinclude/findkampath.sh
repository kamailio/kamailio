#!/bin/sh
if test -x /usr/sbin/kamailio
then
	echo "/usr/sbin"
	exit 0
fi
if test -x /usr/local/sbin/kamailio
then
	echo "/usr/local/sbin"
	exit 0
fi
exit 1
