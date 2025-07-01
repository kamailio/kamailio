
#ifndef __ctl_defaults_h
#define __ctl_defaults_h
/*listen by default on: */

#define DEFAULT_CTL_SOCKET_PROTO "unixs:"

#ifdef SRNAME
/* this is used when compiling sercmd tool */
#define DEFAULT_CTL_SOCKET_NAME SRNAME "_ctl"
#else
/* this is used when compiling sip server */
#define DEFAULT_CTL_SOCKET_NAME NAME "_ctl"
#endif

#define DEFAULT_CTL_SOCKET \
	DEFAULT_CTL_SOCKET_PROTO RUN_DIR "/" DEFAULT_CTL_SOCKET_NAME

/* port used by default for tcp/udp if no port is explicitely specified */
#define DEFAULT_CTL_PORT 2049

#define PROC_CTL -32

#endif
