changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE({{When we reach this part of the main route, we know the message is for our server to handle,
	it is not part of an on-going dialog (handled by loose route), it's not an ACK, and finally,
	it's not a REGISTER message. 
PARA We assume all remaining messages (INVITE, MESSAGE, NOTIFY, etc) should be handled in the
	same way and send them to the NEWDIALOG route.}},
{{	# All local, non-ACKs, non-REGISTER are assumed to be part of a potential
	# new dialog (standalone MESSAGE and NOTIFYs without dialog-creation will ex. also reach here)}},
{{	route(NEWDIALOG);
}})dnl
}})dnl
changequote(`,')dnl
