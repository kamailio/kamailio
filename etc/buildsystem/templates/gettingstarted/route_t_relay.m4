changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
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
}})dnl
}})dnl
changequote(`,')dnl
