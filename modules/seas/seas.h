/* $Id$
 *
 * Copyright (C) 2006-2007 VozTelecom Sistemas S.L
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _SEAS_H
#define _SEAS_H
#include <arpa/inet.h>

#include "../../str.h"/*str*/
#include "../../ip_addr.h"/*ip_addr*/
#include "../../sr_module.h" /*version,etc*/
#include "../../modules/tm/tm_load.h"/*tm_binds*/
#include "ha.h"
#include "cluster.h"
#define MAX_AS_NR 5
#define MAX_UNC_AS_NR 5
#define MAX_AS_NAME 15
/*#define AF2PF(af)   (((af)==AF_INET)?PF_INET:((af)==AF_INET6)?PF_INET6:(af))*/
#define MAX_AS_PER_CLUSTER 10

#define HAS_FD 1
#define HAS_NAME 2

#define MAX_BINDS 10
#define MAX_WHOAMI_LEN 30

#define AS_BUF_SIZE 4000

#define ENCODED_MSG_SIZE 3200

/** EVENT FLAGS */

#define E2E_ACK 0x04
#define CANCEL_FOUND 0x08

#define AS_TYPE 1
#define CLUSTER_TYPE 2

/** ACTION identifiers **/

#define T_REQ_IN 2
#define SL_REQ_IN 3
#define RES_IN 4
#define PING_AC 5
#define BIND_AC 6
#define UNBIND_AC 7

/** ACTION identifiers **/

#define SPIRAL_FLAG 0x00000001

#define net2hostL(dst,from,index) do{ \
   memcpy(&(dst),(from)+(index),4); \
   dst=ntohl(dst); \
   (index)+=4; \
}while(0);



extern char use_stats;
extern char whoami[];
extern int is_dispatcher;

extern struct ip_addr *seas_listen_ip;
extern unsigned short seas_listen_port;

extern int write_pipe;
extern int read_pipe;
extern char seas_sigchld_received;

extern int jain_ping;
extern int jain_pings_lost;

extern int servlet_ping;
extern int servlet_pings_lost;

extern struct as_entry *as_table;

struct seas_functions{
   struct tm_binds tmb;
   cmd_function t_check_orig_trans;
};

/*TODO listen_points should be dynamically allocated ?*/
typedef struct app_server {
   int event_fd;
   int action_fd;
   str name;
   pid_t action_pid;
   struct socket_info *binds[MAX_BINDS];
   char bound_processor[MAX_BINDS];
   int num_binds;
   str ev_buffer;
   str ac_buffer;
   struct ha jain_pings;
   struct ha servlet_pings;
   struct cluster *cluster;
}as_t, *as_p;

struct cluster{
   str name;
   int num;
   int registered;
   str as_names[MAX_AS_PER_CLUSTER];
   as_p servers[MAX_AS_PER_CLUSTER];
};

/**
 * SER processes will go through the as_table, doing if(valid && memcmp(name,his_name,name_len)==0),
 * when one matches, they will put the as pointer inside the event that should process
 * that event.
 * If eventually the as becomes unavailable, the dispatcher will set valid=false, which should be
 * atomic operation. This way, we prevent having to put a mutex on the array, which would make 
 * it slower , as only one process could be accessing it at a time.
 */
struct as_entry{
   str name;
   int type;
   int connected;
   union{
      struct app_server as;
      struct cluster cs;
   }u;
   struct as_entry *next;
};


extern struct as_entry *my_as;
extern struct seas_functions seas_f; 
extern struct as_entry *as_list;

typedef struct as_msg {
   struct cell *transaction;
   char *msg;
   int len;
   int type;
   int id;
   struct as_entry *as;
}as_msg_t,*as_msg_p;

char get_processor_id(struct receive_info *rcv,as_p as);
void seas_sighandler(int signo);
char* create_as_event_t(struct cell *t,struct sip_msg *msg,char processor_id,int *evt_len,int flags);
char* create_as_event_sl(struct sip_msg *msg,char processor_id,int *evt_len,int flags);

static inline void print_ip_buf(struct ip_addr* ip, char *where,int len)
{
   switch(ip->af){
      case AF_INET:
	 snprintf(where,len,"%d.%d.%d.%d", ip->u.addr[0], ip->u.addr[1], ip->u.addr[2], ip->u.addr[3]);
	 break;
      case AF_INET6:
	 snprintf(where,len,"%x:%x:%x:%x:%x:%x:%x:%x",htons(ip->u.addr16[0]),htons(ip->u.addr16[1]),htons(ip->u.addr16[2]),
	       htons(ip->u.addr16[3]), htons(ip->u.addr16[4]), htons(ip->u.addr16[5]), htons(ip->u.addr16[6]),
	       htons(ip->u.addr16[7]));
	 break;
      default:
	 break;
   }
}
#endif
