changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},
{{ANNOTATE({{As part of SER - Getting Started, we have included a log system.
Using the m4 make system to generating your ser.cfg, you can turn on and off, as well as
control how much SER should log (without restarting SER). 

PARA It is very useful to see when and which incoming
messages get processed, especially to identify missing ACKs in a transaction or for looping messages.  Thus, here
we log all incoming messages. We log the From, To, and Call-Id headers. These three together always identify a dialog (i.e. call). 
Subsequent log messages will always include the call-id. Thus, you can grep your log using the Call-Id to get all
the log messages pertaining to one call (but might be multiple dialogs).

PARA Notice the format of the log statement. The XNOTICE macro is defined in the m4 SER build system
and wraps the actual logging into several checks used to turn logging on and off.  The actual log string is sent 
to xlog, so the format must be suitable for xlog.  The corresponding NOTICE macro will send the string to the log
function (built-in, so you don't need to load the xlog module, but you cannot use variables to represent parts of the
messages). If you use the SER m4 build system, you can use the following log macros in your own code: XNOTICE, 
XINFO, XDEBUG, NOTICE, INFO, DEBUG.}},
{{# Log all incoming messages to see start of processing}},
{{XNOTICE("INCOMING - %rm\nFrom: %is(%fu)\nTo: %ru\nCall-ID: %ci\n")}})
}})dnl GS_HELLOWORLD
ifdef({{GS_ACC}},
{{
ANNOTATE({{}},
{{	# We only set the flag for initial INVITEs (i.e. without the to tag)
	# We then set a record-route cookie in the INVITE to make sure that all messages are accounted.}},
{{	if (method=="INVITE" && @to.tag=="") {
		setflag(FLAG_ACC);
	}
}})
dnl Execute the hook for setting the accounting flag
sinclude(CFGPREFIX/common/h_set_acc_flag.m4)dnl
sinclude(CFGPREFIX/CFGNAME/h_set_acc_flag.m4)dnl
}})dnl
changequote(`,')dnl
