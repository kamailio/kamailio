changequote({{,}})dnl
route[REGISTER] {
	# ------------------------------------------------------------------------
	# REGISTER Message Handler
	# ------------------------------------------------------------------------
ifdef({{GS_HELLOWORLD}},
{{ANNOTATE(
{{All REGISTER messages are happily honoured because we have not
        yet implemented authorization checks. The save() function is
        responsible for storing SIP client registration information in the
        location table. However recall that the usrloc db_mode has been set
        to zero on line 5.1. This means that the save() function will only
        save the registration information to memory rather than to
        disk.}},
{{}},
{{}})
}})dnl ifdef GS_HELLOWORLD
ifdef({{GS_AUTH}},
{{
ANNOTATE({{}},
{{	# From lookup_domain, $t.did should be set if we found the domain as ours.}},
{{	if (!$t.did) {
		XNOTICE("%rm  From <%fu>  Attempted REGISTER from non-local domain\nCall-ID: %ci\n");
		sl_reply("403", "Attempting to register with unknown domain");
		exit;
	}
}})	
ANNOTATE({{}},
{{	# We check that Authorization header in the message contains valid credentials found in the
	# table with the same name.}},
{{	if (!www_authenticate("$fd.digest_realm", "REGISTER_CREDENTIALS_TABLE")) {
}})	
ANNOTATE({{}},
{{		# $? represents the return error code from www_authenticate()}},
{{		if ($? == -2) {
			XDEBUG("    REGISTER - %rm  From <%fu>  500 Internal Server Error\nCall-ID: %ci\n");
			sl_reply("500", "Internal Server Error");
		} else if ($? == -3) {
			XDEBUG("    REGISTER - %rm  From <%fu>  400 Bad Request\n");
			sl_reply("400", "Bad Request");
		} else {
}})
ANNOTATE({{}},
{{			# After www_authenticate(), $digest_challenge should be set if no credentials were found.
			# We append the challenge to reply and send 401 Unauthorized.}},
{{			if ($digest_challenge) {
				append_to_reply("%$digest_challenge");
			}
			sl_reply("401", "Unauthorized");
		}
}})	
ANNOTATE({{}},
{{		# We didn't have a valid set of credentials, so we exit.}},
{{		exit;
	}
}})
ANNOTATE({{}},
{{	# www_authenticate set $uid, so let's store it.}},
{{	$authuid=$uid;
}})
ifdef({{REGISTER_CHECK_TO_EQUALS_AUTH}},
{{
ANNOTATE({{}},
{{	# Check if the authenticated user is the same as the target user.
	# We use the uri in the To field to lookup the user's uid. If none is found, the To user is unknown.}},
{{	if (!lookup_user("$tu.uid", "@to.uri")) {
		sl_reply("404", "Trying to register unknon user (To)");
		exit;
	}
}})
ANNOTATE({{}},
{{	# $f.uid set by www_authenticate() should match the uid we should looked up}},
{{	if ($authuid != $tu.uid) {
		sl_reply("403", "Authentication and To-Header mismatch");
		exit;
	}
}})	
}})dnl ifdef REGISTER_CHECK_TO_EQUALS_AUTH
ifdef({{REGISTER_CHECK_FROM_EQUALS_AUTH}},
{{
ANNOTATE({{}},
{{	# We use the uri in From to look up the uid.}},
{{	if (!lookup_user("$fu.uid", "@from.uri")) {
		sl_reply("404", "Expect a known user in From");
		exit;
	}
}})
ANNOTATE({{}},
{{	# The looked up $fu.uid is checked against the Authentication user}},
{{	if ($authuid != $fu.uid) {
		sl_reply("403", "Authentication and From-Header mismatch");
		exit;
	}
}})
}})dnl
}})dnl ifdef GS_AUTH
	if (!save_contacts("location")) {
ANNOTATE({{If SER is unable save the contact information then we just
        return the error to the client and return to the main route
        block.}},
{{}},
{{		sl_reply("400", "Invalid REGISTER Request");
		exit;
	};
}})
ifdef({{GS_MESSAGE}},
{{
ANNOTATE({{}},
{{	# Check to see if the user has any stored messages received while offline}},
{{	m_dump("");
}})
}})dnl ifdef GS_MESSAGE
	exit;
} # End route[REGISTER]
changequote(`,')dnl
