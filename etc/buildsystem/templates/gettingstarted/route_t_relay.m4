changequote({{,}})dnl
ANNOTATE({{Route[RELAY] is the default message handler that is used throughout
        the ser.cfg file to relay messages. Notice how the route block
        definition uses square brackets, but to call a route block you use
        parenthesis.}},
{{}},
{{route[RELAY] {
}})
ANNOTATE(
{{As for incoming messages, we want to log all the outgoing messages as well. This
way we know when the message is leaving our server.}},
{{# Log all outgoing messages}},
{{XNOTICE("OUTGOING - %rm\nFrom: %is(%fu)\nTo: %ru\nCall-ID: %ci\n")}})dnl
ifdef({{GS_NAT}},
{{ANNOTATE({{}},
{{	# Send to NAT fixing}},
{{	route(NAT_MANGLE);}})
ANNOTATE({{}},
{{	# And make sure that the replies goes through the reply route for NAT fixing}},
{{	t_on_reply("NAT_MANGLE");}})
}})dnl ifdef GS_NAT
ifdef({{GS_CALLFWD}},
{{ANNOTATE({{}},
{{	# If we have been in the failure route, this is a forwarding after a non-answered attempt.
	# We need to add a branch.}},
{{	if (isflagset(FLAG_FAILUREROUTE)) {
		append_branch();
	}
}})
ANNOTATE({{}},
{{	# We want all failure responses to go through our failure route.}},
{{	t_on_failure("ROUTE_FAILURE_ROUTE");
}})
}})dnl ifdef GS_CALLFWD
ifdef({{GS_MESSAGE}},
{{
ANNOTATE({{}},
{{	# If we are sending a MESSAGE, we want to store it if it fails, so use another failure route.}},
{{	if (method=="MESSAGE") {
		t_on_failure("MESSAGE");
	}
}})
}})dnl ifdef GS_MESSAGEifdef({{GS_HELLOWORLD}},{{
ANNOTATE(
{{t_relay() is a function exposed by the tm.so module and is
        perhaps one of the most important functions in any ser.cfg file.
        t_relay() is responsible for sending a message to it's destination and
        keep track of any re-sends or replies. If the message cannot be sent
        successfully, then t_relay() will return an error condition.

PARA    See section 1 for more in-depth background information on
        transactions.}},
{{	# ------------------------------------------------------------------------
	# Default Message Handler
	# ------------------------------------------------------------------------
	# t_relay will act based on the current request URI, as well as any
	# overruling received parameters (i.e. for NATed user agents)
	# UDP to TCP and vice versa works as well.
}},
{{	if (!t_relay()) {
}})dnl
ANNOTATE({{If t_relay() cannot send the SIP message then
        sl_reply_error() will send the error back to the SIP client to inform
        it that a server error has occurred.
PARA	If everything is ok, we are done and exit will end execution of ser.cfg
	for this message.}},
{{}},
{{	      	sl_reply_error();
	};
	exit;
}
}})dnl ifdef GS_HELLOWORLD
changequote(`,')dnl
