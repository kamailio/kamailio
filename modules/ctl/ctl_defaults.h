
#ifndef __ctl_defaults_h
#define __ctl_defaults_h
/*listen by default on: */
#ifdef SRNAME
/* this is used when compiling sercmd tool */
#define DEFAULT_CTL_SOCKET  "unixs:" RUN_DIR "/" SRNAME "_ctl"
#else
/* this is used when compiling sip server */
#define DEFAULT_CTL_SOCKET  "unixs:" RUN_DIR "/" NAME "_ctl"
#endif
/* port used by default for tcp/udp if no port is explicitely specified */
#define DEFAULT_CTL_PORT 2049

#define PROC_CTL -32

#endif
