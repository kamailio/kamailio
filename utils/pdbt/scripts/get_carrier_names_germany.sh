#!/bin/bash

# Copyright (C) 2009 1&1 Internet AG
#
# This file is part of sip-router, a free SIP server.
#
# sip-router is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# sip-router is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software 
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Small helper script for germany:
# Download the directory of german carrier names and the respective IDs from
# the 'Bundesnetzagentur' and convert this into the format which the pdbt tool
# understands.

url="http://www.bundesnetzagentur.de/enid/Portierungskennung/Verzeichnis_1ct.html"

# fix LOCALE problem during filtering 
export LANG="C"

wget -O - "$url" | recode latin1..utf8 | tr -d '\r' | tr '\n' '@' | sed 's/^.*Firma//' | sed 's/<\/table>.*$//' | tr '@' '\n' | sed 's/<\/p>/@/' | sed 's/<\/td>/@/' | egrep -v "^ *<" | tr -d '\n' | sed 's/@ *@/@/g' | tr '@' '\n' | sed 's/  */ /g' | sed 's/^ *//' | tr '\n' '@' | sed 's/\([^@]*\)@\(D[0-9][0-9][0-9]\)[^@]*@/\2 \1@/g' | tr '@' '\n' | sed 's/\&nbsp\;/ /g' | sed 's/\&amp\;/\&/g' | sed 's/  */ /g' | egrep -v '^$'
