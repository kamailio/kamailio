changequote({{,}})dnl
ANNOTATE({{None of our routes matched (and the routes will have relayed the message if successful).
	This is thus an error situation.}},
{{	# By now, the request uri should be correct, relay}},
XNOTICE("No route matched for the request\nCall-Id: %ci\n");
{{	sl_reply("404", "No route matched");
}})
changequote(`,')dnl
