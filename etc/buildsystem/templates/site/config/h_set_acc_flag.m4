dnl This hook is executed from forallmessages.m4, right after the FLAG_ACC flag has been tested and set.
dnl If you have special logic to determine when to account, you put it here.
dnl This is the standard way to test whether to set the flag:
dnl if (method=="INVITE" && @to.tag=="") {
dnl		setflag(FLAG_ACC);
dnl	}
dnl 
dnl You can add code that uses unsetflag(FLAG_ACC) or setflag(FLAG_ACC)
