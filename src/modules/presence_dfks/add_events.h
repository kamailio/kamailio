#ifndef _DFKS_ADD_EV_H_
#define _DFKS_ADD_EV_H_

int dfks_add_events(void);
int dfks_publ_handler(struct sip_msg *msg);
int dfks_subs_handler(struct sip_msg *msg);
#endif
