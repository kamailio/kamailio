config:=curlapi.cfg
KAMBIN:=/usr/local/sbin

# Find the IP address
include ../../mkinclude/ipaddr.mak


# Log the version
include ../../mkinclude/kamversion.mak

all:
	@echo Our IP address $(ipaddr) on interface $(interface)  - Redo to fly Kamailio?
	@/usr/local/sbin/kamailio -f $(config)  -l udp:$(ipaddr):5060 -D 1 -E -M 2000

debug:
	@/usr/local/sbin/kamailio -f $(config)  -l udp:$(ipaddr):5060 -D 1 -E > /tmp/kamcurl.$$$$ 2>&1
	grep curl /tmp/kamcurl.$$$$

