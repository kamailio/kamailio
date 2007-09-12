dnl Non-local messages should NOT be left at the end of this part.
dnl Use uri!=myself or !is_uri_local() etc to determine whether this is a non-local
dnl call and send to another route (ex. SEND/t_relay) and exit; afterwards.
changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ifdef({{GS_AUTH}},
{{
ANNOTATE({{}},
{{	# check if the caller is from a local domain (f=from, d=domain, $f will be set)}},
{{	lookup_domain("$fd", "@from.uri.host");
}})
ANNOTATE({{}},
{{	# check if the callee is at a local domain (t=to, d=domain, $t will be set)
	# If we have no To tag (dialog creating messages), we use request uri, but
	# but if this is an in-dialog message (like BYE with last hop before UA here)
	# we use the To header.}},
{{	if(@to.tag=="") {
		lookup_domain("$td", "@ruri.host");
	} else {
		lookup_domain("$td", "@to.uri.host");
	}
}})
ANNOTATE({{}},
{{	# we dont know the domain of the caller and also not
	# the domain of the callee -> someone uses our proxy as
	# a relay
}},
{{	if (!$t.did && !$f.did) {
		sl_reply("403", "Relaying Forbidden");
		exit;
	}
}})	
}},
{{ANNOTATE({{Up to now we have treated messages for our local server (i.e. destined for a local
	subscriber) and another server (ex. a gateway to the public telephony network) in the
	same way.  Now we need to split the processing.  All messages not for myself should
	just be relayed. In the hello-world script, we assume that SER's relaying knows where
	to send the message (normally this means looking up the domain part of the sip address
	found in the message using Domain Name Service, DNS). 
PARA The keyword 'uri' is defined in the SER core and is a synonym
        for the request URI or R-URI for short.
PARA myself is also defined in SER core
	and it will test the request URI against all listen addresses and aliases that have
	been defined at the top of ser.cfg (among the directives).
PARA So, this line tests to see if the R-URI is the same as the SIP
        proxy itself. Another way to look at it is by saying that if this
        statement is TRUE then the SIP message is not being sent to our SIP
        proxy, but rather to some other destination, such as a third party SIP
        phone.}},
{{	# Make sure that non-local messages are just relayed. 
	# myself will test against listen addresses and aliases defined in directives.m4}},
{{	if (uri!=myself) {
		route(RELAY);
		exit;
	};
}})
}})dnl ifdef GS_AUTH
}})dnl ifdef GS_HELLOWORLD
changequote(`,')dnl
