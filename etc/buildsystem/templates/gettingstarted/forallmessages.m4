changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},
{{ANNOTATE({{Here we look to see if the SIP message that was received is a
        REGISTER message. If it is not a register message then we must
        record-route the message to ensure that upstream and/or downstream SIP
        proxies that we may interact with keep our SIP proxy informed of all
        SIP state changes. By doing so, we can be certain that SER has a
        chance to process all SIP messages for the conversation. PARA 
	The keyword 'method' is provided by the SER core and allows you
        to find out what type of SIP message you are dealing with.}},
{{	# ------------------------------------------------------------------------
	# Record Route Section
	# ------------------------------------------------------------------------}},
{{	if (method!="REGISTER") {}})
ANNOTATE({{The record_route() function simply adds a Record-Route header
        field to the current SIP message. The inserted field will be inserted
        before any other Record-Route headers that may already be present in
        the SIP message. Other SIP servers or clients will use this header to
        know where to send an answer or a new message in a SIP dialog.}},
{{}},
{{		record_route();
	};
}})dnl 
ANNOTATE({{As part of SER - Getting Started, we have included a log system.
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
changequote(`,')dnl
