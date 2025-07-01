#!/usr/bin/env bash

# This script is executed by CMake after db_berkeley and db_text schema files are generated.
# It appends the last line of each generated file (except 'version')
# to the 'version' file and truncates each generated file to the n line.

# Get the number of lines to move to version file from the first argument
# Get the nubmer of lines to keep in the file from the second argument

TAIL_NUMBER="$1"
HEAD_NUMBER="$2"

# echo " Tail number: $TAIL_NUMBER"
# echo " Head number: $HEAD_NUMBER"

# Loop through files, sorted alphabetically
for FILE in $(ls * | sort); do
    # Check if it's a regular file and not the version file
    if [ -f "$FILE" ] && [ "$FILE" != "version" ]; then
        # echo "  Processing: $FILE"

        # Check if file has at least 1 line before tail
        if [ -s "$FILE" ]; then # -s checks if file is not empty
            # Append the last line to the version file
            # Using "printf" to ensure a newline is added after the tail output
            tail -n "$TAIL_NUMBER" "$FILE" >> version
            if [ ${PIPESTATUS[0]} -ne 0 ]; then # Check tail command result
                echo "Warning: tail command failed for $FILE"
            fi

            # Get the first line and overwrite the original file
            head -n "$HEAD_NUMBER" "$FILE" > "$$FILE".tmp
            if [ $? -ne 0 ]; then
                 echo "Warning: head command failed for $FILE"
            else
                mv "$$FILE".tmp "$FILE"
                if [ $? -ne 0 ]; then
                     echo "Warning: mv command failed for $FILE"
                fi
            fi
        else
            echo "Warning: File $FILE is empty, skipping processing."
        fi
    fi
done

echo "Finished processing schema files in $PWD"

exit 0 # Indicate success
