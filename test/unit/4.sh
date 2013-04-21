#!/bin/bash
# test basic fifo functionality

# Copyright (C) 2007 1&1 Internet AG
#
# This file is part of Kamailio, a free SIP server.
#
# Kamailio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# Kamailio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

source include/common
source include/require

CFG=4.cfg

if ! (check_kamailio); then
	exit 0
fi ;

# setup config
echo -e "loadmodule \"$SR_DIR/modules/mi_fifo/mi_fifo.so\"" > $CFG
echo -e "loadmodule \"$SR_DIR/modules/kex/kex.so\"" >> $CFG
echo -e "modparam(\"mi_fifo\", \"fifo_name\", \"/tmp/kamailio_fifo\")" >> $CFG


        
$BIN -w . -f $CFG > /dev/null
ret=$?

if [ "$ret" -eq 0 ] ; then
	sleep 1
	$CTL version > /dev/null
	ret=$?
fi ;

$KILL

rm -f $CFG

exit $ret