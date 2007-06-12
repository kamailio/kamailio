changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},
{{ANNOTATEHEAD({{SER has debug information that can be enabled or suppressed using the
        debug directive. A nominal value of 3 is usually specified to obtain
        enough debug information when errors occur. The debug directive
        specifies how much information to write to syslog. The higher the
        number, the more verbose SER becomes. The most verbose debug level is
        9. When the debug level is set higher than 3 SER becomes very verbose
        and the start up time can take a bit longer.}},
{{# Debug level on logging}},
{{debug=DEBUG_LEVEL}})
ANNOTATE({{The fork directive tells the SER daemon to run in the foreground
        or the background. When you are ready to operate SER as a system
        service, you must set this directive to yes. For now we will just
        operate SER as a foreground process. 
	NOTE: See the appendix for a SER init.d start script.}},
{{# Fork and create children (no=run single process for debugging purposes)}},
{{fork=DEBUG_FORK}})
ANNOTATE({{Since we are running SER as a foreground process we must set the
        log_stderror directive equal to yes in order to see the output.}},
{{# Log to stderr (only use this of fork=no)}},
{{log_stderror=DEBUG_LOG_STDERROR}})
ANNOTATE({{}},
{{# Log to the defined syslog facility}},
{{log_facility=SYSLOG_FACILITY}})
ANNOTATE({{The listen directive instructs SER to listen for SIP traffic on
        a specific IP address. Your server must physically listen on the IP
        address you enter here. If you omit this directive then SER will
        listen on all interfaces. NOTE: When you start SER, it will report 
	all the interfaces that it is listening on.}},
{{# IP address to listen to}},
{{listen=SER_LISTEN_IP}})
ANNOTATE({{In addition to specifying the IP address to listen on you can
        also specify a port. The default port used on most SIP routers is
        5060. If you omit this directive then SER assumes port 5060.}},
{{# Port to listen on}},
{{port=SER_LISTEN_PORT}})
ANNOTATE({{The alias directive is used by SER to determine if an incoming 
	message is destined for itself. The 'myself' command uses the alias
	(see further below}},
{{# Aliases used by uri!=myself checks to identity locally destined messages}},
{{alias=SER_ALIAS}})
ANNOTATE({{The children directive tells SER how many processes to spawn
        upon server start up. A good number here is 4, however, in a
        production environment you may need to increase this number.}},
{{# Number of children to fork per port per protocol}},
{{children=NR_OF_CHILDREN}})
ANNOTATE({{These lines are really to prevent SER from attempting to lookup
        its IP addresses in DNS. By adding these two lines to ser.cfg we
        suppress any warnings if your IP is not in your DNS server.}},
{{# Should dns resolving be used?}},
{{dns=SER_DNS_ON}})
ANNOTATE({{}},
{{# Should reverse dns resolving be used?}},
{{rev_dns=SER_REVERSE_DNS_ON}})
}})dnl
changequote(`,')dnl
