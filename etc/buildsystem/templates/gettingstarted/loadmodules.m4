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
}})dnl
changequote(`,')dnl
