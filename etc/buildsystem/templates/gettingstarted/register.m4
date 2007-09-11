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
		exit;
	}
}})
}})dnl
changequote(`,')dnl
