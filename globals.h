/*
 * $Id*
 *
 * global variables
 *
 */


#ifndef globals_h
#define globals_h

#define NO_DNS     0
#define DO_DNS     1
#define DO_REV_DNS 2


extern char * cfg_file;
extern unsigned short port_no;
extern char * names[];
extern unsigned long addresses[];
extern int addresses_no;
extern int child_no;
extern int debug;
extern int dont_fork;
extern int log_stderr;
extern int check_via;
extern int received_dns;


#endif
