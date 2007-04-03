#!/bin/sh
echo $1
cvs status $1 | sed -n 's/^[ \]*Working revision:[ \t]*\([0-9][0-9\.]*\).*/\1/p'

