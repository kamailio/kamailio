changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{ANNOTATE({{The main route block is where all received SIP messages are
        sent. From the main route block you can call other route blocks, test
        the message for certain conditions, reject the message, relay the
        message, and basically do anything you need to fit your business
        needs.}})dnl
ANNOTATE({{       Here is a main overview of what happens:</para>

        <orderedlist>
          <listitem>
            <para>A message enters the main route and we do some checks</para>
          </listitem>

          <listitem>
            <para>We determine if the message is for us, if not we just send
            it to where it belongs (route[RELAY])</para>
          </listitem>

          <listitem>
            <para>If its for us, we explicitly handle REGISTER messages (a
            message from a phone asking to register itself). Route[REGISTER] handles
            REGISTERs by saving where we can reach the phone, while route[1]
            is a default handler for all other messages.</para>
          </listitem>
        </orderedlist>

        <para>NOTE: It may appear odd to present such simple functionality
        this way, but we will use this structure as the basis for a much more
        complex ser.cfg which is presented step-by-step in following
        sections.</para>

        <para>The details are below.}})dnl
ANNOTATE({{It always make sense to stop invalid SIP messages as early as possible.
	This way you will prevent some (badly made) Denial of Service attacks, as well
	as avoid spending server resources on messages that will be denied somewhere or
	may cause problems. The type of checks done can be controlled by the sanity module's
	'default_checks' parameter. In the Getting Started configurations, this parameter
	is not explicitly set (defaults to all), so by adding this configuration to your
	common/local modparams.m4 file, you can change the behavior of sanity_check() below.}},
{{	# ------------------------------------------------------------------------
	# Sanity Check Section
	# ------------------------------------------------------------------------}},
{{	sanity_check();}})
ANNOTATE({{mf_process_maxfwd_header is a safety check that you should
        always include as the first line of your main route block. This
        function is exposed in the mf.so module and is used to keep track of
        how many times a SIP message has passed through SER. Erroneous ser.cfg
        files can send a SIP message to the wrong location and cause looping
        conditions. Also other SIP proxies that SER interacts with can do the
        same thing. PARA The basic rule here is that if this function ever returns 'true'
        then you need to stop processing the problematic message to avoid
        endless looping.}},
{{	# Check whether we might be in a loop.}},
{{	if (!mf_process_maxfwd_header("10")) {
}})dnl
ANNOTATE({{If a looping situation is detected then SER needs a way to tell
        the SIP client that an error has occurred. The sl_send_reply()
        function performs this job. sl_send_reply() is exposed in the sl.so
        module and all it does is sends a stateless message to the SIP client.
        This means that SER will send the message and forget about it. It will
        not try to resend if it does not reach the recipient or expect a
        reply. PARA You can specify an appropriate error message as shown. The error
        message can only selected by the defined SIP error codes and messages.
        Many IP phones will show the text message to the user.}},,
{{		sl_reply("483", "Too Many Hops");
}})dnl
ANNOTATE({{The exit statement tells SER to stop processing the SIP message
        and exit from the route block that it is currently executing and 
	completely stop processing the current message.}},
,
{{		exit;
	};
}})dnl
ANNOTATE({{msg:len is a core SER function that returns the length in bytes
        of the current SIP message. This, like mf_process_maxfwd_header()
        should be called at the beginning of the main route block in all
        ser.cfg files. PARA This statement simply tests the length of the SIP message
        against the maximum length allowed. If the SIP message is too large,
        then we stop processing because a potential buffer overflow has been
        detected.}},
{{}},
{{	if (msg:len > max_len) {
		sl_reply("513", "Message Overflow");
		exit;
	};
}})dnl
}})dnl
changequote(`,')dnl
