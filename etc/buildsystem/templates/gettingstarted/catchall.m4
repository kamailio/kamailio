changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE({{When we return from the NEWDIALOG route, we assume that our processing logic
	has determined where to send the message, so we want to relay the message.}},
{{	# By now, the request uri should be correct, relay}},
{{	route(RELAY);
}})dnl
}})dnl
changequote(`,')dnl
