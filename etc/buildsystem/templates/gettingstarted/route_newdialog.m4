dnl Note that this route is called newdialog because dialog-creating messages will go through 
dnl here (INVITE). However, any message that is not in-dialog will go here (however,
dnl "route_notindialog" is a weird name...)
dnl
dnl This route should return to caller after doing request uri changes
changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE(
{{The NEWDIALOG route is responsible for all messages that start a new dialog, as well
	as those NOT starting a dialog, but which are not part of one (NOTIFY, OPTIONS, 
	etc)}},
{{}}, 
{{route[NEWDIALOG] {
}})dnl
ANNOTATE({{lookup_contacts("aliases") attempts to retrieve any aliases for the
        Requested URI. An alias is just another way to refer to a SIP
        destination. For example, if you have a SIP user 4075559938 and a toll
        free number 8885551192 that rings to 4075559938, then you can create
        the toll free number as an alias for 4075559938. When someone dials
        the toll free number the SIP client configured as 4075559938 will
        ring.
PARA    The lookup(aliases) function is not strictly needed for this
        example, however, serctl, which is a command line utility included
        with SER requires this line for it to operate properly.}},
{{}},
{{	lookup_contacts("aliases");
}})dnl
ANNOTATE(
{{If an alias was found and the Requested URI is no longer a
        locally served destination, then we relay the SIP message using our
        default message handler route[RELAY]. After the message is relayed we stop
        processing by means of the exit command.}},
{{	# We here have to test again for "localness", as an alias may change
	# the uri to a non-local one.
}},
{{        if (uri!=myself) {
                route(RELAY);
                exit;
	}
}})dnl
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
ANNOTATE({{If the party being called cannot be located then SER will reply
        with a 404 User Not Found error. This is done in a stateless
        fashion.}},
{{}},
{{		sl_send_reply("404", "User Not Found");
		break;
	};
}
}})dnl
}})dnl
changequote(`,')dnl
