#ifndef SUBSCRIBE_H
#define SUBSCRIBE_H

#include "presence.h"
#include "../../str.h"

typedef struct subscribtion
{
	str to_user;
	str to_domain;
	str from_user;
	str from_domain;
	str event;
	str event_id;
	str to_tag;
	str from_tag;
	str callid;
	int cseq;
	str contact;
	str record_route;
	int expires;
	str status;
	str reason;
	int version;
}subs_t;
void msg_active_watchers_clean(unsigned int ticks,void *param);

void msg_watchers_clean(unsigned int ticks,void *param);

int handle_subscribe(struct sip_msg*, char*, char*);


#endif
