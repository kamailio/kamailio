dnl This hook is executed in onfailure.m4 if a no answer (408|480) was received.
dnl You can use this hook to implement voicemail etc.
dnl You must make sure to do exit; after successfully processing the busy to avoid further
dnl processing in failure route.
dnl You can also do more advanced checking using tm functions for whether a 1xx response was received
dnl or not, as well as choose error code to return based on which parallel branch failing.