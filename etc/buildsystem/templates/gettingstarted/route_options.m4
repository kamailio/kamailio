route[OPTIONS_REPLY]
{
	# Only reply to OPTIONS without a username in the RURI, but with one of our IP.
	if (method=="OPTIONS" && @ruri.user=="" && (uri==myself||$t.did)) {
		options_reply();
		exit;
	}
}
