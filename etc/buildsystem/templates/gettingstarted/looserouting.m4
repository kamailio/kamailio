changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE({{loose_route() tests to see if the current SIP message should be
        loose routed or not. If the message should be loose routed then SER
        should simply relay the message to the next destination as specified
        in the top-most Record-Route header field.</para>

        <para>In all ser.cfg files you should call loose_route() after the
        record_route() function.</para>

        <para>This is the basic definition of loose routing. Refer to RFC3261
        for a complete explanation of loose routing.</para>

        <para>Note for interested readers:</para>

        <para># RFC3261 states that a SIP proxy is said to be "loose routing"
        if it</para>

        <para># follows the procedures defined in this specification for
        processing of</para>

        <para># the Route header field. These procedures separate the
        destination of the</para>

        <para># request (present in the Request-URI) from the set of proxies
        that need to</para>

        <para># be visited along the way (present in the Route header field).
        A proxy</para>

        <para># compliant to these mechanisms is also known as a "loose
        router".</para>

        <para>#</para>

        <para># SER is a loose router, therefore you do not need to do
        anything aside</para>

        <para># from calling loose_route() in order to implement this
        functionality.</para>

        <para>#</para>

        <para># A note on SIP message dialogs (contributed by Jan
        Janak):</para>

        <para>#When it comes to SIP messages, you can classify them as those
        that</para>

        <para>#create a dialog and those that are within a dialog. A message
        within</para>

        <para>#a dialog (ACK, BYE, NOTIFY) typically requires no processing on
        the</para>

        <para>#server and thus should be relayed at the beginning of [your
        ser.cfg]</para>

        <para>#right after loose_route function.</para>

        <para>#</para>

        <para>#Messages creating dialogs can be further classified as those
        that</para>

        <para>#belong to the server (they have the domain or IP of the server
        in</para>

        <para>#the Request-URI) and those that do not. Messages that do not
        have a</para>

        <para>#domain or IP of the proxy in the Request-URI should be
        immediately</para>

        <para>#relayed or blocked.}},
{{	# ------------------------------------------------------------------------
	# Handling messages with Route headers (aka loose routing)
	# ------------------------------------------------------------------------
}},
{{	if (loose_route()) {}})
		XDEBUG("    loose routed - %rm\nFrom: %is(%fu)\nTo: %ru\nCall-ID: %ci\n")
ifdef({{GS_NATHANDLING}},
{{
ANNOTATE({{}},
{{# This attribute is set so we can check in NAT handling whether we are routing based on Route headers.}},
{{		$loose_route = true;
}})
}})dnl ifdef GS_NATHANDLING
ifdef({{GS_ACC}},
{{
ANNOTATE({{}},
{{		# After loose_route(), any attributes found in the dialog cookie have been set.
		# Check if this dialog is accounted.}},
{{		if ($accounting) {
			setflag(FLAG_ACC);
		}
}})
}})dnl
ANNOTATE({{If the test for loose routing returns 'true' then we must relay
        the message without other processing. To do so, we pass execution
        control to the route (ie, route[RELAY]). This route block is
        documented later, but it suffices now to say that it is the default
        message handler.}},
{{}},
{{		route(RELAY);}}) 
ANNOTATE({{Since loose routing was required we cannot process the message
        any further so we must use a command to exit the main route
        block.}},
{{}},
{{		exit;
	}
}})
ANNOTATE({{Here we look to see if the SIP message that was received is a
        REGISTER message. If it is not a register message then we must
        record-route the message to ensure that upstream and/or downstream SIP
        proxies that we may interact with keep our SIP proxy informed of all
        SIP state changes. By doing so, we can be certain that SER has a
        chance to process all SIP messages for the conversation. PARA 
	The keyword 'method' is provided by the SER core and allows you
        to find out what type of SIP message you are dealing with.}},
{{	# ------------------------------------------------------------------------
	# Record Route Section
	# ------------------------------------------------------------------------}},
{{	else if (method!="REGISTER") {}})
ifdef({{GS_ACC}},
{{
ANNOTATE({{}},
{{		# If the accounting flag is set, we want to store that info in a dialog cookie
		# This way we can quickly check if we should account.}},
{{		if (isflagset(FLAG_ACC)) {
			$accounting = true;
			setavpflag($accounting, "dialog_cookie");
		}
}})
}})dnl ifdef GS_ACC
ANNOTATE({{The record_route() function simply adds a Record-Route header
        field to the current SIP message. The inserted field will be inserted
        before any other Record-Route headers that may already be present in
        the SIP message. Other SIP servers or clients will use this header to
        know where to send an answer or a new message in a SIP dialog.}},
{{}},
{{		record_route();
	};
}})
ANNOTATE({{ACK is a special type of message. It is part of a transaction, ex. INVITE - 200 OK - ACK.
	The INVITE will enter the main route and handled further below, while the OK will enter
	our script in the onreply_route (see subsequent chapters) and NOT the main route. The ACK
	will enter the main route (as it is not a reply, but in fact a confirmation message in its
	own right), but will normally be handled by the loose route test above.  However, in some
	cases, when our server is the last step before the destination, the ACK will not be
	handled by loose route.  In this case, the ACK header (request-URI) will contain the 
	destination and we can just relay the message.}},
{{	# If the ACK has more hops before reaching the user agent, it will have a Route header 
	# and be forwarded above. If next hop is a user agent, the Request URI will be the ip:port
	# found in Contact of the original OK from the user agent, but will not be loose routed.
	# Just relay the ACK to the user agent.}},
{{	if (method=="ACK") {
}})
XDEBUG("    ACK relayed using ruri - %rm\nFrom: %is(%fu)\nTo: %ru\nCall-ID: %ci\n")
ANNOTATE({{}},
{{}},
{{
		route(RELAY);
		exit;
	}
}})
}})dnl ifdef GS_HELLOWORLD
changequote(`,')dnl
