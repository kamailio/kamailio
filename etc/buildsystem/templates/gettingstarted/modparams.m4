changequote({{,}})dnl
ANNOTATE({{}},
{{# Flags set be various modules}},
{{}})dnl
ifdef({{GS_CALLFWD}},
{{ANNOTATE({{}},
{{# The flag set when the script has passed through the failure route}},
{{flags
	FLAG_FAILUREROUTE: DEF_FLAG_FAILUREROUTE;
}})
}})dnl
ifdef({{GS_ACC}},
{{
ANNOTATE({{}},
{{# The FLAG_ACC flag is set to mark a message for logging to the database or syslog}},
{{flags
	FLAG_ACC:	DEF_FLAG_ACC;
}})
}})dnl ifdef GS_ACC
ifdef({{GS_NAT}},
{{flags
	FLAG_NAT_UAC:	DEF_FLAG_NAT_UAC,
	FLAG_NAT_UAS:	DEF_FLAG_NAT_UAS;
}})dnl
ifdef({{GS_HELLOWORLD}},{{
ANNOTATE({{The usrloc module is responsible for keeping track of SIP client
        registration locations. In other words, when a SIP client registers
        with the SIP proxy, SER will store the contact information, also known
        as the Address Of Record (AOR), in a table. This table is then queried
        when another SIP client makes a call. The location of this table can
        vary depending on the value of the usrloc db_mode parameter.
PARA Our Hello World SIP proxy sets db_mode to zero to indicate that we do not
        want to persist registration data to a database. Instead we will only
        store this information in memory. Mode 1, write-through, can be used 
	to store data in a database.}},
{{# We set database mode to 0 = do not save to database backend (keep in memory), 1 = write-through to database}},
{{modparam("usrloc", "db_mode", USRLOC_DB_MODE)}})
ANNOTATE({{The ctl module is the management module of SER. It can be used to
        examine the current SIP proxy activity. CTL can also be used if
        you want to inject a new SIP message directly in to the SIP proxy with
        an external application. A good example of this is the sercmd utility
        that is usually located in /usr/local/sbin/. CTL listens on a unix socket, 
	which is represented by a file in the filesystem. You specify that file here and
	must use the same filename for any clients contacting your SER (i.e. sercmd).}},
{{# ctl module params
# by default ctl listens on unixs:/tmp/ser_ctl if no other address is
# specified in modparams; this is also the default for sercmd}},
{{modparam("ctl", "binrpc", "unixs:SER_SOCK_PATH")}})
ANNOTATE({{The fifo parameter specifies the location of the SER FIFO. FIFO was the
	management interface used in previous versions of SER. The serctl script
	used this interface. It is available for backward compatibility, for example
	to use with SEMS, SER Media Server.}},
{{# listen on the "standard" fifo for backward compatibility}},
{{modparam("ctl", "fifo", "fifo:SER_FIFO_PATH")}})
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
{{modparam("rr", "enable_full_lr", ENABLE_FULL_RR)}})
ANNOTATE({{When attributes are put into Record-Route, they are encoded in BASE64. However,
	you can also make SER encrypt the attributes with a secret to avoid somebody inferring
	something about internal workings of your server that they can use for hacking.}},
{{# Encrypt dialog cookies with this secret}},
{{modparam("rr", "cookie_secret", "RR_COOKIE_SECRET")}})
}})dnl ifdef GS_HELLOWORLD
ifdef({{GS_XMLRPC}},
{{ANNOTATE({{The management module can also listen on a TCP/IP network interface. 
	This way you can use sercmd or other management clients from another server.
	This can for example be used to create new users from an external web system.}},
{{# listen on tcp, localhost, 2049 is the ctl and sercmd default port}},
{{modparam("ctl", "binrpc", "SER_XMLRPC")}})
}})dnl
ifdef({{GS_GLOBAL_LOAD}},
{{
ANNOTATE({{}},
{{# Causes global flags and domain attributes to be loaded from database on SER start-up.}},
{{modparam("gflags", "load_global_attrs", 1)
modparam("domain", "load_domain_attrs", 1)}})
}})dnl
ifdef({{GS_ACC_DB}},
{{ANNOTATE({{}},
{{# Set the name of the flag we are going to set when we want to account a message.}},
{{modparam("acc_db", "log_flag", "FLAG_ACC")}})
ANNOTATE({{}},
{{# Set the name of the flag we are going to set when we want to account a missed message.}},
{{modparam("acc_db", "log_missed_flag", "FLAG_ACC_MISSED")}})
}})dnl
ifdef({{GS_ACC_SYSLOG}},
{{ANNOTATE({{}},
{{# Set the name of the flag we are going to set when we want to account a message.}},
{{modparam("acc_syslog", "log_flag", "FLAG_ACC")}})
ANNOTATE({{}},
{{# Set the name of the flag we are going to set when we want to account a missed message.}},
{{modparam("acc_syslog", "log_missed_flag", "FLAG_ACC_MISSED")}})
}})dnl
ifdef({{GS_REGISTRAR}},
{{ifdef({{GS_NAT}},
{{ANNOTATE({{}},
{{# Set the name of the flag which will be saved when contact details are stored when a user agent registers.}},
{{modparam("registrar", "save_nat_flag", "FLAG_NAT_UAC")}})
ANNOTATE({{}},
{{# Set the name of the flag that should be set when NAT info is loaded from the user location.}},
{{modparam("registrar", "load_nat_flag", "FLAG_NAT_UAS")}})
}})dnl ifdef GS_NAT
}})dnl
ifdef({{GS_NAT}},
{{ifdef({{GS_NAT_RTPPROXY}},
{{
ANNOTATE({{}},
{{# Set the number of seconds between each keepalive sent to each user agent behind NAT}},
{{modparam("nathelper", "natping_interval", NATHELPER_NATPING_INTVL)}})
ANNOTATE({{}},
{{# Which method to use when sending a keepalive. OPTIONS will generate a 200 OK from the user agent.}},
{{modparam("nathelper", "natping_method", "NATHELPER_PING_METHOD")}})
ANNOTATE({{}},
{{# Only send keepalives to contacts in user location that is registered as behind NAT?}},
{{modparam("nathelper", "ping_nated_only", NATHELPER_PING_NATED_ONLY)}})
ANNOTATE({{}},
{{# The socket where rtpproxy listens.}},
{{modparam("nathelper", "rtpproxy_sock", "NATHELPER_RTPPROXY_SOCK")}})
}})dnl if GS_NAT_RTPPROXY
}})dnl ifdef GS_NAT
ifdef({{GS_XMLRPC}},
{{
ANNOTATE({{}},
{{}},
{{modparam("xmlrpc", "route", "RPC")}})
}})dnl
ifdef({{GS_AUTH}},
{{ANNOTATE({{}},
{{# The authentication related modules and which database to use.}},
{{modparam("auth_db|usrloc|uri_db", "db_url", "SER_DB_URL")}})
}})dnl
ifdef({{GS_GLOBAL_LOAD}},
{{ANNOTATE({{}},
{{# The modules loading values on SER start-up and the database to use.}},
{{modparam("domain|gflags", "db_url", "SER_DB_URL")}})
}})dnl
ifdef({{GS_ACC_DB}},
{{ANNOTATE({{}},
{{# Accounting to database and the database to use.}},
{{modparam("acc_db", "db_url", "SER_DB_URL")}})
}})dnl
ifdef({{GS_MESSAGE}},
{{ANNOTATE({{}},
{{# The database to use when storing MESSAGEs for offline user agents.}},
{{modparam("msilo", "db_url", "SER_DB_URL")}})
}})dnl
ifdef({{GS_HELLOWORLD}},
{{
ANNOTATE({{SER is not dialog stateful. This means that we don't have any information stored in 
	SER from the INVITE when we receive the BYE (or other messages). We have two means of 
	storing data related to a dialog: either by writing to the database or by setting a 
	dialog cookie in the Record-Route header of the SIP message. Because SIP agents have to
	copy exactly what is in a Record-Route into the Route headers in subsequent messages,
	the cookie we stored will be found in the Route header. The record_route() function sets
	the cookie, while the loose_route() function reads the cookie and sets the corresponding attribute
	back again. The below name of the avpflag is used when we want to mark an attribute for inclusion
	in the dialog cookie.}},
{{# We can store some of our important attributes (inside the messages)}},
{{avpflags
  dialog_cookie;
}})
}})dnl ifdef GS_HELLOWORLD
changequote(`,')dnl
