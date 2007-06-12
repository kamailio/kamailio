changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE({{The usrloc module is responsible for keeping track of SIP client
        registration locations. In other words, when a SIP client registers
        with the SIP proxy, SER will store the contact information, also known
        as the Address Of Record (AOR), in a table. This table is then queried
        when another SIP client makes a call. The location of this table can
        vary depending on the value of the usrloc db_mode parameter.
PARA Our SIP proxy sets db_mode to zero to indicate that we do not
        want to persist registration data to a database. Instead we will only
        store this information in memory. We will show how to persist AORs in a later example.}},
{{# We set database mode to 0 = do not save to database backend (keep in memory)}},
{{modparam("usrloc", "db_mode", 0)}})
ANNOTATE({{The parameter called enable_full_lr is really a work-around
        for older SIP clients that don't properly handle SIP record-route
        headers. SIP RFC3261 says that specifying loose routing by including a
        ;lr in the record-route header is the correct way to tell other SIP
        proxies that our SIP proxy is a loose router. However, some older
        and/or broken SIP clients incorrectly dropped the ;lr tag because it
        didn't have value associated with it (ie, lr=true). Therefore, by setting 
	enable_full_lr to one, SER will write all
        ;lr tags as ;lr=on to avoid this problem.}},
{{# We turn on full lr (adds lr=on as uri parameter, not lr). 
# This should have higher compatibility with some user agents.}},
{{modparam("rr", "enable_full_lr", 1)}})
ANNOTATE({{ The ctl module is the management module of SER. It can be used to
        examine the current SIP proxy activity. CTL can also be used if
        you want to inject a new SIP message directly in to the SIP proxy with
        an external application. A good example of this is the sercmd utility
        that is usually located in /usr/local/sbin/. CTL listens on a unix socket, 
	which is represented by a file in the filesystem. You specify that file here and
	must use the same filename for any clients contacting your SER (i.e. sercmd).}},
{{# ctl module params
# by default ctl listens on unixs:/tmp/ser_ctl if no other address is
# specified in modparams; this is also the default for sercmd}},
{{modparam("ctl", "binrpc", "unixs:/tmp/ser_ctl")}})
ANNOTATE({{The fifo parameter specifies the location of the SER FIFO. FIFO was the
	management interface used in previous versions of SER. The serctl script
	used this interface. It is available for backward compatibility, for example
	to use with SEMS, SER Media Server.}},
{{# listen on the "standard" fifo for backward compatibility}},
{{modparam("ctl", "fifo", "fifo:/tmp/ser_fifo")}})
ANNOTATE({{The management module can also listen on a TCP/IP network interface. 
	This way you can use sercmd or other management clients from another server.
	This can for example be used to create new users from an external web system.}},
{{# listen on tcp, localhost, 2049 is the ctl and sercmd default port}},
{{modparam("ctl", "binrpc", "tcp:127.0.0.1:2049")}})
}})dnl
changequote(`,')dnl
