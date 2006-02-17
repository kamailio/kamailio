#ifndef __SIP_EVENTS_UAC_H
#define __SIP_EVENTS_UAC_H

/* SIP UAC able to generate SIP events subscriptions and process NOTIFY */

#include "../dialog/dlg_mod.h" /* this will be the dialog core in the future */
#include "../../timer.h"
#include "../../timer_ticks.h"
#include <cds/msg_queue.h>
#include <cds/ref_cntr.h>
#include <presence/qsa.h>

//typedef enum {
//	subscription_unconfirmed, /* after sent SUBSCRIBE */
//	subscription_confirmed, /* after confirmation with 200 OK (enable NOTIFY for it too!) */
//	subscription_resubscribing, /* after sent SUBSCRIBE */
//	subscription_predestroyed, /* after 200 OK on unSUBSCRIBE */
//	subscription_destroyed /* after last NOTIFY (may arrive in destroying status !) */
//} events_uac_status_t;

typedef enum {
	euac_unconfirmed,
	euac_unconfirmed_destroy,
	euac_confirmed,
	euac_waiting,
	euac_resubscription,
	euac_resubscription_destroy,
	euac_waiting_for_termination,
	euac_predestroyed,
	euac_destroyed,
} events_uac_status_t;

typedef void (*notify_callback_func)(struct sip_msg *, void *param);

typedef struct _events_uac_t {
	/* SUBSCRIBE-NOTIFY dialog */
	dlg_t *dialog;

	/* str aor, local_uri, contact; */

	/* callback function for NOTIFY messages (don't use locking here !!!
	 * it is always locked using events_uac mutex) */
	notify_callback_func cb;

	/* parameter for callback function */
	void *cbp;

	/* data needed for resubscriptinos */
	str headers;		
	str local_uri;
	str remote_uri;
	str route;

	struct _events_uac_t *prev, *next; /* linked list ? */

	events_uac_status_t status;
	
	/* reference counter - needed for freeing memory if
	 * reference stored on more places */
	reference_counter_data_t ref_cntr;

	struct timer_ln timer;
	int timer_started;
} events_uac_t;

/* creates structure in shm and adds it into internal list */
events_uac_t *create_events_uac(str *remote_uri, str *local_uri, 
		const str *events, 
		notify_callback_func cb, /* callback function for processing NOTIFY messages (parsing, ...) */
		void *cbp, /* parameter for callback function */
		const str *other_headers, str *route);

/* removes structure from memory and from internal lists*/
int destroy_events_uac(events_uac_t *uac);

/* adds a reference to events_uac_t */
events_uac_t *find_events_uac(dlg_id_t *id);

/* removes reference created by find_events_uac */
void remove_uac_reference(events_uac_t *uac);

/* intitialize internal structures */
int events_uac_init();

/* destroy internal structures */
void events_uac_destroy();

/* tries to process given notify message */
int process_euac_notify(struct sip_msg* m);

#endif
