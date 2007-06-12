changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE({{REGISTER messages are explicitly handled, so we need to find out
        if we are processing a registration request.}},
{{	# ------------------------------------------------------------------------
	# Message Type Processing Section
	# NOTE!!! It is assumed that all messages reaching this point are destined
	# for the local server (REGISTER) or for UAs registered locally. (see nonlocal.m4)
	# ------------------------------------------------------------------------}},
{{	if (method=="REGISTER") {}})
ANNOTATE({{All registration requests are passed to route[REGISTER] for processing.
        We do this to somewhat segregate the workload of the main route block.
        By doing so we keep our ser.cfg from becoming unmanageable.}},
{{}},
{{		route(REGISTER);
		break;
	};}})
ANNOTATE({{When we reach this part of the main route, we know the message is for our server,
	it is not part of an on-going dialog (handled by loose route), it's not an ACK, and finally,
	it's not a REGISTER message. 
PARA We assume all remaining messages (INVITE, MESSAGE, NOTIFY, etc) should be handled in the
	same way and send them to the NEWDIALOG route.}},
{{	# All local, non-ACKs, non-REGISTER are assumed to be part of a potential
	# new dialog (MESSAGE and NOTIFYs without dialog-creation will ex. also reach here)}},
{{	route(NEWDIALOG);
}})dnl
}})dnl
changequote(`,')dnl
