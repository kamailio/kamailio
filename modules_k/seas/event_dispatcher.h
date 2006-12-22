/* $Id$ */

#include "../../ip_addr.h"

struct unc_as{
   char valid;
   int fd;
   char name[MAX_AS_NAME];
   char flags;
   union sockaddr_union su;
};

/*uncomplete as table, from 0 to MAX_UNC_AS_NR are event, from then on are action*/
/*should only be modified by the dispatcher process, or we should add a lock*/
extern struct unc_as unc_as_t[];
extern struct as_entry *my_as;

int process_unbind_action(as_p as,char *payload,int len);
int process_bind_action(as_p as,char *payload,int len);
int dispatcher_main_loop();
int spawn_action_dispatcher(struct as_entry *as);
