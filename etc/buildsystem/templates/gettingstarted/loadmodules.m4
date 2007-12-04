changequote({{,}})dnl
ifdef({{GS_HELLOWORLD}},
{{ANNOTATE({{Here we have external modules that are necessary for our hello
        world SIP proxy to function. SER modules may be located anywhere on
        the disk system, but /usr/local/lib/ser/modules is the default
        location. Loading a module is as simple as specifying in a loadmodule
        directive as shown.
PARA If you're wondering how you know which module or modules you
        need, well there is no clear cut way to know. In general the modules
        shown here will be needed by every SIP proxy configuration. When you
        need to add additional functionality, such as MySQL connectivity, then
        you will load the appropriate module, in this case the mysql.so
        module.
PARA All SER modules are distributed with the source code and located
        at &lt;ser-source&gt;/modules. Every SER module has documentation created in
	docbook. This documentation is available online at http://iptel.org/. 
	This document makes a valid attempt at explaining the most important 
	SER modules and how to use them, but there is no substitute for reading the 
	actual module documentation.
PARA Load the stateless transaction module (sl_reply etc).}},
{{# Load the stateless transaction module (sl_reply etc)}},
{{loadmodule "SER_LIB_DIR/sl.so"}}) 
ANNOTATE({{Load the transaction stateful module (t_relay() etc).}},
{{# Load the transaction stateful module (t_relay() etc)}},
{{loadmodule "SER_LIB_DIR/tm.so"}})
ANNOTATE({{Load the record routing module. Needed for making sure messages go through our server.}},
{{# Load the record routing module. Needed for making sure messages go through our server}},
{{loadmodule "SER_LIB_DIR/rr.so"}})
ANNOTATE({{Load maxfwd for stopping loops.}},
{{# Load maxfwd for stopping loops}},
{{loadmodule "SER_LIB_DIR/maxfwd.so"}})
ANNOTATE({{Load the user location module to store and retrieve user locations.}},
{{# Load the user location module to store and retrieve user locations}},
{{loadmodule "SER_LIB_DIR/usrloc.so"}})
ANNOTATE({{Load the registrar module to handle registrations.}},
{{# Load the registrar module to handle registrations}},
{{loadmodule "SER_LIB_DIR/registrar.so"}})
ANNOTATE({{Load the xlog module for advanced logging with parameters.}},
{{# Load the xlog module for advanced logging with parameters}},
{{loadmodule "SER_LIB_DIR/xlog.so"}})
ANNOTATE({{Load the ctl for the control socket interface used to send commands to a running SER server.}},
{{# Load the ctl module for the control socket interface.}},
{{loadmodule "SER_LIB_DIR/ctl.so"}})
ANNOTATE({{Load the global flags module used by the logging system to turn dynamically on and off logging on a running SER server.}},
{{# Load the gflags, global flags module.}},
{{loadmodule "SER_LIB_DIR/gflags.so"}})
ANNOTATE({{The textops module is a generix utility module that offers useful functions for searching, substituting, 
	and in other ways manipulating SIP messaging. Simple, but very powerful, and beware! You can quickly break
	an RFC...}},
{{# Load textops used for searching/selecting text in messages and manipulating headers.}},
{{loadmodule "SER_LIB_DIR/textops.so"}})
ANNOTATE({{}},
{{# Load the sanity check module to do integrity checks on incoming messages.}},
{{loadmodule "SER_LIB_DIR/sanity.so"}})
ifdef({{OPTIONS_REPLY_TO_LOCAL}},
{{ANNOTATE({{}},
{{# Load the options module to reply to OPTIONS messages.}},
{{loadmodule "SER_LIB_DIR/options.so"}})
}})dnl
}})dnl GS_HELLOWORLD
ifdef({{USE_MYSQL_DB}},
{{ANNOTATE({{}},
{{# Load the mysql connector module.}},
{{loadmodule "SER_LIB_DIR/mysql.so"}})
}})dnl
ifdef({{GS_AUTH}},
{{
ANNOTATE({{}},
{{# Load the auth module with all the core authentication functions.}},
{{loadmodule "SER_LIB_DIR/auth.so"}})
dnl if USRLOC_DB_MODE is not 0, we need the db functions as well
ifelse({{USRLOC_DB_MODE}},{{0}},{{}},
{{
ANNOTATE({{}},
{{# Load the auth module with the database functions.}},
{{loadmodule "SER_LIB_DIR/auth_db.so"}})
ANNOTATE({{}},
{{# The uri_db module uses database to check the uri.}},
{{loadmodule "SER_LIB_DIR/uri_db.so"}})
ANNOTATE({{}},
{{# The avp_db module allows loading of user attributes from database.}},
{{loadmodule "SER_LIB_DIR/avp_db.so"}})
}})dnl ifelse USRLOC_DB_MODE
ANNOTATE({{}},
{{# Load domain module used to verify the local domains.}},
{{loadmodule "SER_LIB_DIR/domain.so"}})
}})dnl ifdef GS_AUTH
ifdef({{GS_ACC_DB}},
{{ANNOTATE({{}},
{{# The acc_db module allows accounting directly to database.}},
{{loadmodule "SER_LIB_DIR/acc_db.so"}})
}})dnl
ifdef({{GS_ACC_SYSLOG}},
{{ANNOTATE({{}},
{{# The acc_syslog module allows accounting directly to the syslog daemon.}},
{{loadmodule "SER_LIB_DIR/acc_syslog.so"}})
}})dnl
ifdef({{GS_MESSAGE}},
{{ANNOTATE({{}},
{{# Load msilo to store MESSAGE in db if receiver is offline.}},
{{loadmodule "SER_LIB_DIR/msilo.so"}})
}})dnl
ifdef({{GS_XMLRPC}},
{{ANNOTATE({{}},
{{# Load the module implementing the XMLRPC interface into the management of SER.}},
{{loadmodule "SER_LIB_DIR/xmlrpc.so"}})
}})dnl
ifdef({{GS_NAT}},
{{
ifdef({{GS_NAT_RTPPROXY}},
{{
ANNOTATE({{}},
{{# Load the nathelper module, which regularly pings UAs behind NAT and connects to rtpproxy (iptel.org).}},
{{loadmodule "SER_LIB_DIR/nathelper.so"}})
}})dnl
ifdef({{GS_NAT_MEDIAPROXY}},
{{
ANNOTATE({{}},
{{# Load the mediaproxy module, which regularly pings UAs behind NAT and connects to mediaproxy (AG Projects).}},
{{loadmodule "SER_LIB_DIR/mediaproxy.so"}})
}})dnl
}})dnl ifdef GS_NAT
ifdef({{GS_CALLFWD}},
{{
ANNOTATE({{}},
{{# avp module has a long range of functions for attribute manipulation.}},
{{loadmodule "SER_LIB_DIR/avp.so"}})
}})dnl ifdef GS_CALLFWD
changequote(`,')dnl
