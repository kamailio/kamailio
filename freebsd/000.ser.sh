#!/bin/sh
#
# Start/stop/restart SIP EXpress Router (SER)
#
# Version: 1.0 - Paul Belanger <pabelanger@gmail.com>
#
# Directions:
# copy ser script to /usr/local/etc/rc.d/
# edit /etc/rc.conf and add the following:
# ser_enable="YES"
#
#
# 05.05.2005 - Initial Version

. /etc/rc.subr

name="ser"
rcvar="`set_rcvar`"
command="/usr/local/sbin/${name}"
pidfile="/var/run/${name}.pid"

load_rc_config $name
ser_flags="$cron_flags -P $pidfile"
run_rc_command "$1"
