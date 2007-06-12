dnl Use this file to define global macros to be includes across the templates.
dnl These macros are not intended to be used by users.
dnl
define(`SEPARATOR',`ifdef(`INCLUDE_SEPARATORS',`$1',`dnl')')dnl
define(`PKG_INCLUDE',
`ifdef(`GETTINGSTARTED',
`SEPARATOR(`# GETTINGSTARTED ********** $1.m4')
include(`./templates/gettingstarted/$1.m4')dnl
SEPARATOR(`# end GETTINGSTARTED ********** $1.m4')
')dnl
dnl Add more include statements below to create new packages of functionality
dnl you can wrap them in an ifdef statement to control the inclusion from the make file
')dnl !!!!!! NO PACKAGES BELOW HERE
dnl 
dnl All the logging macros are defined as empty here, because if no logging is included, the macros
dnl must expand to empty
define(`NOTICE',`# NOTICE logging is turned off')dnl
define(`INFO',`# INFO logging is turned off')dnl
define(`DEBUG',`# DEBUG logging is turned off')dnl
define(`XNOTICE',`# NOTICE logging is turned off')dnl
define(`XINFO',`# INFO logging is turned off')dnl
define(`XDEBUG',`# DEBUG logging is turned off')dnl
dnl Logging macros
ifdef(`INCLUDE_GETTING_STARTED_LOGGING',
`ifdef(`INCLUDE_NOTICE_LOGGING',
`undefine(`NOTICE')dnl
define(`NOTICE',
`if(is_gflag("GFLAG_NOTICE")){
  log(LOG_NOTICE,$1);
}')')')
ifdef(`INCLUDE_GETTING_STARTED_LOGGING',
`ifdef(`INCLUDE_INFO_LOGGING',
`undefine(`INFO')dnl
define(`INFO',
`if(is_gflag("GFLAG_INFO")){
  log(LOG_INFO,$1);
}')dnl
')dnl
')dnl
ifdef(`INCLUDE_GETTING_STARTED_LOGGING',
`ifdef(`INCLUDE_DEBUG_LOGGING',
`undefine(`DEBUG')dnl
define(`DEBUG',
`if(is_gflag("GFLAG_DEBUG")){
  log(LOG_DEBUG,$1);
}')dnl
')dnl
')dnl
dnl
ifdef(`INCLUDE_GETTING_STARTED_LOGGING',
`ifdef(`INCLUDE_NOTICE_LOGGING',
`define(`undefine(`XNOTICE')dnl
XNOTICE',
`if(is_gflag("GFLAG_NOTICE")){
  xlog("XLOG_NOTICE",$1);
}')')')
ifdef(`INCLUDE_GETTING_STARTED_LOGGING',
`ifdef(`INCLUDE_INFO_LOGGING',
`undefine(`XINFO')dnl
define(`XINFO',
`if(is_gflag("GFLAG_INFO")){
  xlog("XLOG_INFO",$1);
}')dnl
')dnl
')dnl
ifdef(`INCLUDE_GETTING_STARTED_LOGGING',
`ifdef(`INCLUDE_DEBUG_LOGGING',
`undefine(`XDEBUG')dnl
define(`XDEBUG',
`if(is_gflag("GFLAG_DEBUG")){
  xlog("XLOG_DEBUG",$1);
}')dnl
')dnl
')dnl
dnl
dnl If the GETTINGSTARTED macro is defined, we want to include the gettingstarted interals
ifdef(`GETTINGSTARTED',
`dnl Include the gettingstarted internal defines
include(`./templates/gettingstarted/internals.m4')')dnl
