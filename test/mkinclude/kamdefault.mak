# Default include file

# Test version
testver=1.0
ifeq ($(config),)
	$error("??? No config set=")
endif
pidfile:=/tmp/${config}.pid

include ../../mkinclude/kambin.mak
include ../../mkinclude/ipaddr.mak
