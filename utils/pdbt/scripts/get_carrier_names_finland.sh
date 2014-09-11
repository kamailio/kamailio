#!/bin/bash

# Copyright (C) 2010 Mikko Lehto, mikko dot lehto at setera dot fi
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

# Small helper script for Finland:
# Download the directory of finish carrier names and the respective IDs from
# the regulation authorities and convert this into the format which the pdbt tool
# understands.

wget -O -  "http://www2.ficora.fi/numerointi/nure_numbering.asp?nums=tot&amp;lang=en" | awk '/<tbody>/, /<\/tbody>/' | awk -F"</td" -v RS="</tr" '{ gsub(/.*>/,"",$1) gsub(/.*>/,"",$2); gsub(/&auml;/,"ä",$2); gsub(/&Aring;/,"Å",$2); gsub(/&ouml;/,"ö",$2); if ( $2 != "") { printf "D%.3d %s\n",$1,$2 } }'
