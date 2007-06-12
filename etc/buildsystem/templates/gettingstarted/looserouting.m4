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
	# Loose Route Section
	# ------------------------------------------------------------------------
}},
{{	if (loose_route()) {}})
ANNOTATE({{If the test for loose routing returns 'true' then we must relay
        the message without other processing. To do so, we pass execution
        control to the route (ie, route[RELAY]). This route block is
        documented later, but it suffices now to say that it is the default
        message handler.}},
{{}},
{{		route(RELAY);}}) 
ANNOTATE({{Since loose routing was required we cannot process the message
        any further so we must use a break command to exit the main route
        block.}},
{{}},
{{		break;
	};}})
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
		route(RELAY);
		break;
	}
}})dnl
}})dnl
changequote(`,')dnl
