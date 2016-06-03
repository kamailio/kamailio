#
#  Find IP address of local system
#  Currently it's either Linux or (default) os/x

UNAME_S:=$(shell uname)
ifeq ($(UNAME_S),Linux)
osystem=LINUX
interface:=eth0
# Needs to be changed to "ip" instead of "ifconfig"
ipaddr:=$(shell  /sbin/ifconfig $(interface) | grep 'inet '|cut -d':' -f2|cut -d' ' -f1)
else
# Assume Darwin OS/X
osystem=DARWIN
interface:=en0
ipaddr:=$(shell /sbin/ifconfig $(interface) | grep 'inet '|cut -d' ' -f2)
endif

ifeq ($(ipaddr),)
$(error Can not find IP address?)
endif
