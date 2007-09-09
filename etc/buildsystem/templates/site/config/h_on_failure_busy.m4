dnl This hook is executed in onfailure.m4 if a busy (486|600) was received.
dnl You can use this hook to implement voicemail etc.
dnl You must make sure to do exit; after successfully processing the busy to avoid further
dnl processing in failure route.