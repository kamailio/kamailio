#!/bin/bash

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