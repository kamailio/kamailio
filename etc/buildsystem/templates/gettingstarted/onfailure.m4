changequote({{,}})dnl
ifdef({{GS_CALLFWD}},
{{
failure_route[FAILURE_ROUTE]
{
ANNOTATE({{}},
{{	# We set the FLAG_FAILUREROUTE, so that we know in other routes that we have been through a failure.}},
{{	setflag(FLAG_FAILUREROUTE);
}})
ANNOTATE({{}},
{{	# If we receive Busy here or Busy everywhere, we want to forward.}},
{{	if (t_check_status("486|600")) {
}})
dnl Include the hook used for handling busy.
sinclude(CFGPREFIX/common/h_on_failure_busy.m4)dnl
sinclude(CFGPREFIX/CFGNAME/h_on_failure_busy.m4)dnl
ANNOTATE({{}},
{{		# The loaded user attributes from when the INVITE was processed, are still set.
		# We load the fwd_busy_target attribute (should be SIP uri) and relay.}},
{{		if ($tu.fwd_busy_target) {
			attr2uri("$tu.fwd_busy_target");
			route(RELAY);
			exit;
		}
	}
}})
ANNOTATE({{}},
{{	# No answer was received, we may have another forwarding.}},
{{	else if (t_check_status("408|480")) {
}})
dnl No answer hook
sinclude(CFGPREFIX/common/h_on_failure_noanswer.m4)dnl
sinclude(CFGPREFIX/CFGNAME/h_on_failure_noanswer.m4)dnl
ANNOTATE({{}},
{{		# Use the user's fwd_noanswer_target }},
{{		if ($tu.fwd_noanswer_target) {
			attr2uri("$tu.fwd_noanswer_target");
			route(RELAY);
			exit;
		}
	}
}})
} # End failure_route[FAILURE_ROUTE]
}})dnl ifdef GS_CALLFWD

ifdef({{GS_MESSAGE}},
{{
failure_route[MESSAGE]
{
ANNOTATE({{}},
{{	# Skip non-MESSAGE messages}},
{{	if( method != "MESSAGE" ) {
		exit;
	}
}})
ANNOTATE({{}},
{{	# Try to store the MESSAGE}},
{{
	if (m_store("1", "")) {
		XDEBUG("    MSILO: offline message stored\nCall-Id: %ci\n");
		t_reply("202", "Accepted"); 
	}
}})	
ANNOTATE({{}},
{{	# Storage failed, tell the sender}},
{{	else {
		XDEBUG("    MSILO: offline message NOT stored\nCall-Id: %ci\n");
		t_reply("503", "Service Unavailable");
	}
}})
}
}})dnl
changequote(`,')dnl
