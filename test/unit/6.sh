#!/bin/bash
# checks a configuration with 'openser -c' and 'openser -C'

# Copyright (C) 2007 1&1 Internet AG
#
# This file is part of openser, a free SIP server.
#
# openser is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# openser is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

CFG=2.cfg

# start
../openser -c -f $CFG > /dev/null 2>&1
ret=$?

if [ "$ret" -eq 0 ] ; then
	../openser -C -f $CFG > /dev/null 2>&1
	ret=$?
fi ;

exit $ret