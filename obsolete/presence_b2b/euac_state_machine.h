#ifndef __EUAC_STATE_MACHINE_H
#define __EUAC_STATE_MACHINE_H

#include "events_uac.h"

typedef enum {
	act_1xx, /* not final responses */
	act_2xx, /* all ok responses */
	act_3xx, /* redirect responses */
	act_4xx, /* all error responses 4xx, 5xx, 6xx, ... */
	act_notify, /* NOTIFY arrives */
	act_destroy,  /* called destroy from client (like create) */
	act_tick
} euac_action_t;


void euac_do_step(euac_action_t action, struct sip_msg *m, events_uac_t *uac);
void euac_start(events_uac_t *uac);

#endif
