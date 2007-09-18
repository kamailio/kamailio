dnl Note that this route is called newdialog because dialog-creating messages will go through 
dnl here (INVITE, MESSAGE, early CANCELs, NOTIFY, INFO). 
dnl
dnl This route should return to caller after doing request uri changes
changequote({{,}})dnl
ANNOTATE(
{{The NEWDIALOG route is responsible for all messages that start a new dialog, as well
	as those NOT starting a dialog, but which are not part of one (NOTIFY, OPTIONS, 
	etc)}},
{{}}, 
{{route[NEWDIALOG] {
}})
ifdef({{GS_OUTBOUND}},
{{
ANNOTATE({{}},
{{	# If we found the domain of the From user, but not the To user, this is an outgoing Internet call.}},
{{	if ($f.did && ! $t.did) {
		route(RELAY);
		exit;
	}
}})
}})dnl ifdef GS_OUTBOUND
ifdef({{GS_AUTH}},
{{ANNOTATE({{}},
{{	# If we don't know the user, it is not for a local user.}},
{{	if (!lookup_user("$tu.uid", "@ruri")) {
ifdef({{GS_PSTN}},
{{ANNOTATE({{}},
{{		# If we have a PSTN gateway, the call is from a local domain and the dial plan matches...}},
{{		if ($f.did && $gw_ip && uri=~"PSTN_DIAL_PLAN") {}}) 
ifdef({{PSTN_ASSERTED_ID_ATTR}},
{{ANNOTATE({{}},
{{			# If the user's $asserted_id is set, add a Remote-Party-ID header}},
{{			if (PSTN_ASSERTED_ID_ATTR) {}})
ANNOTATE({{}},
{{				# Compose the header}},
{{				xlset_attr("$rpidheader", "<sip:%PSTN_ASSERTED_ID_ATTR@%@ruri.host>;screen=yes");
}})
ANNOTATE({{}},
{{				# Add the header field, replace if it already exists}},
{{				replace_attr_hf("Remote-Party-ID", "$rpidheader");
			}
}})
}})dnl ifdef PSTN_ASSERTED_ID_ATTR
ANNOTATE({{}},
{{			#Convert the $gw_ip into a domain part of a uri and set request uri, then relay to gateway.}},
{{			attr2uri("$gw_ip", "domain");
			route(RELAY);
			exit;
		}
}})
}})dnl ifdef GS_PSTN
ANNOTATE({{}},
{{		# User is not known and request uri does not match PSTN dialplan.}},
{{		return;
}})

	}
}})
ifdef({{GS_CALLFWD}},
{{
ANNOTATE({{}},
{{	# Use the looked up uid and load the user attributes.}},
{{	load_attrs("$tu", "$t.uid");}})
ANNOTATE({{}},
{{	# The attribute fwd_always_target contains sip forwarding info.
	# If it happens to be our SIP, it will be sent in loop back for new processing.}},
{{	if ($tu.fwd_always_target) {
		attr2uri("$tu.fwd_always_target");
		route(RELAY);
		exit;
	}
}})
}})dnl ifdef GS_CALLFWD
}})dnl ifdef GS_AUTH
ANNOTATE(
{{lookup_contacts("location") attempts to retrieve the AOR for the Requested
        URI. In other words it tries to find out where the person you are
        calling is physically located. It does this by searching the location
        table that the save() function updates. If an AOR is
        located, we can complete the call, otherwise we must return an error
        to the caller to indicate such a condition. And since the user cannot 
	be found we stop processing.}},
{{}},
{{	if (!lookup_contacts("location")) {
}})dnl
ifdef({{GS_MESSAGE}},
{{ANNOTATE({{}},
{{		# User is offline, store MESSAGEs}},
{{		if(method == "MESSAGE") {
			route(MESSAGE);
			exit;
		}
	}})
}})dnl
ANNOTATE({{If the party being called cannot be located then SER will reply
        with a 404 User Not Found error. This is done in a stateless
        fashion.}},
{{}},
{{		XINFO("    NEWDIALOG - 480 User Offline\nCall-Id: %ci\n");
		sl_reply("480", "User Offline");
		exit;
	}
ifdef({{GS_NAT}},
{{ANNOTATE({{}},
{{	# We have also loaded the NAT_UAS flag, now set the attribute we use throughout the config.}},
{{	if (isflagset("FLAG_NAT_UAS")) {
		XINFO("    NEWDIALOG - callee is behind NAT\nCall-Id:%ci\n");
		$nated_callee = true;
	}
}})
}})dnl
ifdef({{GS_CALLFWD}},
{{
ANNOTATE({{}},
{{	# If the fr_inv_timer attribute (in ms) was set for callee (i.e. loaded from database),
	# we want to set the total time before we time out an INVITE answered with a 1xx reply
	# This is the ringing time, the default is 120,000 ms}},
{{	if ($t.fr_inv_timer) {
}})
ANNOTATE({{}},
{{		# If also the callee has set fr_timer, we also want to set the time-out (in ms)
		# we want to wait if the user agent has not answered at all with 1xx (maybe it is offline)
		# Default is 30,000ms}},
{{		if ($t.fr_timer) {
			t_set_fr("$t.fr_inv_timer", "$t.fr_timer");
}})
ANNOTATE({{}},
{{		# We only have the INVITE timer}},
{{		} else {
			t_set_fr("$t.fr_inv_timer");
		}
	}
}})
}})dnl ifdef GS_CALLFWD
ANNOTATE({{}},
{{	# Relay it to the location(s) of the found user}},
{{	route(RELAY);
	exit;
}})

} # End route[NEWDIALOG]
}})dnl
changequote(`,')dnl
