#!/bin/bash
# runs ../kamailio with all command line arguments.
# ommited options are -h -v -I -c -D -V

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

if ! (check_kamailio); then
	exit 0
fi ;

# the config file
CFG=18.cfg

# setup config
echo -e "debug=3\nrequest_route {\n ;\n}" > $CFG

# start:
$BIN -f ./$CFG -l 127.0.0.1 -n 0 -r -R -E -d -e -K -T -N 0 -b 23 -m 42 -w . -u $(id -u) -g $(id -g) -P ./pid.out -G ./pgid.out -a no -A TESTDEF > /dev/null 2>&1

ret=$?

sleep 1

# clean up:
$KILL

rm $CFG
rm -f ./pid.out
rm -f ./pgid.out 

exit $ret
