#!/bin/bash
# test basic db related kamailioctl functionality for dbtext

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

cd $CTL_DIR

# setup config file
cp $CTLRC $CTLRC.bak
cp $CTL $CTL.bak
sed -i'' -e "s/# DBENGINE=MYSQL/DBENGINE=DBTEXT/g" $CTLRC
sed -i'' -e "s/TEST=\"false\"/TEST=\"true\"/g" $CTL

./$CTL avp list > /dev/null

ret=$?

if [ "$ret" -eq 0 ] ; then
	./$CTL domain showdb > /dev/null
	ret=$?
fi ;

if [ "$ret" -eq 0 ] ; then
	./$CTL dispatcher show > /dev/null
	ret=$?
fi ;

# cleanup
mv $CTLRC.bak $CTLRC
mv $CTL.bak $CTL

exit $ret
