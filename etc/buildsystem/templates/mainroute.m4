ifdef(`INCLUDE_SEPARATORS',
`# Main route, executed for all messages
')dnl
route {
sinclude(CFGPREFIX/common/mainroute.m4)dnl
sinclude(CFGPREFIX/CFGNAME/mainroute.m4)dnl
PKG_INCLUDE(`sanitychecks')dnl

PKG_INCLUDE(`forallmessages')dnl

PKG_INCLUDE(`looserouting')dnl

PKG_INCLUDE(`nonlocal')dnl

PKG_INCLUDE(`typeprocessing')dnl

PKG_INCLUDE(`catchall')dnl

} 
ifdef(`INCLUDE_SEPARATORS',
`# end main route
')dnl
