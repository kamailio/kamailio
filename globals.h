/*
 * $Id$
 *
 * global variables
 *
 */


#ifndef globals_h
#define globals_h

#include "types.h"
#include "ip_addr.h"

#define NO_DNS     0
#define DO_DNS     1
#define DO_REV_DNS 2


extern char * cfg_file;
extern char *stat_file;
extern unsigned short port_no;
extern char port_no_str[];
extern int port_no_str_len;
extern unsigned int maxbuffer;
extern char * names[];
extern int names_len[];
extern struct ip_addr addresses[];
extern int addresses_no;
extern struct ip_addr* bind_address;
extern int children_no;
extern int dont_fork;
extern int check_via;
extern int received_dns;
extern int loop_checks;
extern int process_no;
/*
 * debug & log_stderr moved to dprint.h*/

extern process_bm_t process_bit;
extern int *pids;

extern int cfg_errors;
extern unsigned int msg_no;

extern unsigned int shm_mem_size;

#endif
