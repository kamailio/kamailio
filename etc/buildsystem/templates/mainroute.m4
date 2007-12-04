ifdef(`INCLUDE_SEPARATORS',
`# Main route, executed for all messages
')dnl
route {
sinclude(CFGPREFIX/common/mainroute.m4)dnl
sinclude(CFGPREFIX/CFGNAME/mainroute.m4)dnl
dnl
dnl TO ADD NEW ROUTE CALLS IN THE MAIN ROUTE with each route in a separate file.
dnl -------------
dnl Add a new file 'name.m4' and a new file route_'name'.m4
dnl Add a PKG_INCLUDE(`name')dnl to mainroute.m4 (template/) and a PKG_INCLUDE(`route_name')dnl
dnl to ser.cfg.m4
dnl The name.m4 should contain a call to the route, see gettingstarted/natdetect.m4 as an example
dnl NOTE that doing this will include the two files from ALL feature packages.
dnl 
PKG_INCLUDE(`sanitychecks')dnl

PKG_INCLUDE(`forallmessages')dnl

PKG_INCLUDE(`looserouting')dnl

PKG_INCLUDE(`nonlocal')dnl

PKG_INCLUDE(`natdetect')dnl

PKG_INCLUDE(`register')dnl

PKG_INCLUDE(`pre-authentication')dnl

PKG_INCLUDE(`authentication')dnl

PKG_INCLUDE(`typeprocessing')dnl

PKG_INCLUDE(`catchall')dnl

} 
ifdef(`INCLUDE_SEPARATORS',
`# end main route
')dnl
