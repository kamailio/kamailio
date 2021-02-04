#!/bin/bash

if [ "$1" = "" ]; then
	CMD="kamailio -m 64 -D -dd -f /etc/kamailio.cfg"
else
	CMD="$*"
fi

echo "Running ${CMD}"
exec ${CMD}

