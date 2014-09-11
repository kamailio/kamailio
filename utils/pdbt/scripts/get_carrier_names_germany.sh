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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

# Small helper script for germany:
# Download the directory of german carrier names and the respective IDs from
# the 'Bundesnetzagentur' and convert this into the format which the pdbt tool
# understands.

url="http://www.bundesnetzagentur.de/cln_1912/DE/Sachgebiete/Telekommunikation/RegulierungTelekommunikation/Nummernverwaltung/TechnischeNummern/Portierungskennung/VerzeichnisPortKenn_Basepage.html"

# fix LOCALE problem during filtering 
export LANG="C"

wget -O - "$url" | recode latin1..utf8 | sed 's/^*.Verzeichnis der Portierungskennungen//' | awk '/<tbody>/, /<\/tbody>/' | tr -d '\r' | tr '\n' '@' | sed 's/<\/table>.*$//' | sed 's/<\/tbody>.*$//'

# probably also possible to use this:
# http://www.bundesnetzagentur.de/cae/servlet/contentblob/156772/publicationFile/8492/KonsolidiertesVerzPortierungsk.zip
# main page (for reference):
# http://www.bundesnetzagentur.de/cln_1932/DE/Sachgebiete/Telekommunikation/RegulierungTelekommunikation/Nummernverwaltung/TechnischeNummern/Portierungskennung/KonsolidiertesVerzPortKenn_Basepage.html?nn=120380