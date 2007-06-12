changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},
{{ANNOTATE(
{{Route[REGISTER] is the registration message handler. This is where we
        store user contact data.}},
{{}},
{{ 
route[REGISTER] {
}})dnl
ANNOTATE(
{{All REGISTER messages are happily honoured because we have not
        yet implemented authorization checks. The save() function is
        responsible for storing SIP client registration information in the
        location table. However recall that the usrloc db_mode has been set
        to zero on line 5.1. This means that the save() function will only
        save the registration information to memory rather than to
        disk.}},
{{	# ------------------------------------------------------------------------
	# REGISTER Message Handler
	# ------------------------------------------------------------------------
}},
{{	if (!save_contacts("location")) {
}})dnl
ANNOTATE({{If SER is unable save the contact information then we just
        return the error to the client and return to the main route
        block.}},
{{}},
{{		sl_reply_error();
	};
}
}})dnl
}})dnl
changequote(`,')dnl
