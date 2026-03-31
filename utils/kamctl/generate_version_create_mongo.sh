#!/usr/bin/env sh
#
# This file is part of Kamailio, a free SIP server.
#
# SPDX-License-Identifier: GPL-2.0-or-later
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

OUTPUT_FILE="$1"
JSON_DIR="$2"

# Create the version-create.mongo file
echo "use kamailio;" > "$OUTPUT_FILE"
echo "db.createCollection(\"version\");" >> "$OUTPUT_FILE"

for FILE in "$JSON_DIR"/*.json; do
  if [ -f "$FILE" ]; then
    if [ "$FILE" != "$JSON_DIR/version.json" ]; then
      VN=$(grep '"version":' "$FILE" | grep -o -E '[0-9]+')
      FN=$(basename "$FILE" .json)
      echo "db.getCollection(\"version\").insert({ table_name: \"$FN\", table_version: NumberInt($VN) });" >> "$OUTPUT_FILE"
    fi
  fi
done
