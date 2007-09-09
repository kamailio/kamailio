changequote({{,}})dnl
ifdef({{GS_AUTH}},
{{
route[AUTHENTICATE]
{
ANNOTATE({{}},
{{	# RFC3261 says you are not allowed to challenge these methods}},
{{	if (method=="CANCEL" || method=="ACK") {
		return;
	}
}})
ifdef({{GS_INBOUND}},
{{
ANNOTATE({{}},
{{	# If the domain in From is unknown, this is an incoming call from the Internet, let it pass.}},
{{	if (! $f.did) {
		return;
	}
}})
}})dnl ifdef GS_INBOUND
dnl Execute the hook for trusted sources (which may do a return to avoid authentication)
sinclude(CFGPREFIX/common/h_auth_trusted_src.m4)dnl
sinclude(CFGPREFIX/CFGNAME/h_auth_trusted_src.m4)dnl
ANNOTATE({{}},
{{	# We check that Authorization header in the message contains valid credentials found in the
	# table with the same name.}},
{{	if (!proxy_authenticate("$fd.digest_realm", "AUTH_CREDENTIALS_TABLE")) {
}})	
ANNOTATE({{}},
{{		# $? represents the return error code from proxy_authenticate()}},
{{		if ($? == -2) {
			XDEBUG("    AUTHENTICATE - %rm  From <%fu>  500 Internal Server Error\nCall-ID: %ci\n");
			sl_reply("500", "Internal Server Error");
		} else if ($? == -3) {
			XDEBUG("    AUTHENTICATE - %rm  From <%fu>  400 Bad Request\n");
			sl_reply("400", "Bad Request");
		} else {
}})
ANNOTATE({{}},
{{			# After www_authenticate(), $digest_challenge should be set if no credentials were found.
			# We append the challenge to reply and send 407 Proxy Authentication Required.}},
{{			if ($digest_challenge) {
				append_to_reply("%$digest_challenge");
			}
			sl_reply("407", "Proxy Authentication Required");
		}
}})	
ANNOTATE({{}},
{{		# We didn't have a valid set of credentials, so we exit.}},
{{		exit;
	}
}})
ANNOTATE({{}},
{{	# proxy_authenticate set $uid, so let's store it.}},
{{	$authuid=$uid;
}})
ifdef({{AUTH_CHECK_FROM_EQUALS_AUTH}},
{{
ANNOTATE({{}},
{{	# Use the uri in the From header and look up the uid. If not found, delete the uri attribute.}},
{{	if (!lookup_user("$fu.uid", "@from.uri")) {
		del_attr("$uid");
	}
}})
ANNOTATE({{}},
{{	# Check the found uid against the authenticated uid.}},
{{	if ($fu.uid != $authuid) {
		sl_reply("403", "Fake Identity");
		exit;
	}
}})
}})dnl ifdef AUTH_CHECK_FROM_EQUALS_AUTH
ANNOTATE({{}},
{{	# load the user attributes (preferences) of the caller. $fu.yourattr should now be set if set in the
	# user preferences table.}},
{{	load_attrs("$fu", "$f.uid");
}})
}
}})dnl ifdef GS_AUTH
changequote(`,')dnl
