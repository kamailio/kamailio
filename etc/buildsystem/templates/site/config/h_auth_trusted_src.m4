dnl This hook is executed from the AUTHENTICATE route before any authentication takes plase.
dnl If you need to allow messages from a certain source (like a PSTN gateway), you add your
dnl code here and make sure that you do a return; if you detect a trusted source.
dnl Example:
dnl if(src_ip=a.b.c.d) {
dnl   return;
dnl }