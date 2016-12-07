#!/bin/bash


which ragel >/dev/null
if [ $? -ne 0 ] ; then
  echo "ERROR. Ragel not installed, cannot compile the Ragel grammar." >&2
  exit 1
else
  ragel -v
  echo
fi


set -e

RAGEL_FILE=rfc1918_parser
echo ">>> Compiling Ragel grammar $RAGEL_FILE.rl ..."
ragel -G2 -C $RAGEL_FILE.rl
echo
echo "<<< OK: $RAGEL_FILE.c generated"
echo
