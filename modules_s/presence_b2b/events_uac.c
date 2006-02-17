#include "events_uac.h"
#include "../../dprint.h"

#include <cds/sstr.h>
#include <cds/dstring.h>
#include <cds/sync.h>
#include <cds/hash_table.h>
#include "euac_funcs.h"
#include "euac_internals.h"
#include "euac_state_machine.h"

void remove_uac_from_list(events_uac_t *uac) 
{
	if (uac->next) uac->next->prev = uac->prev;
	else euac_internals->last_uac = uac->prev;
	if (uac->prev) uac->prev->next = uac->next;
	else euac_internals->first_uac = uac->next;
}

void insert_uac_to_list(events_uac_t *uac) 
{
	if (euac_internals->last_uac) euac_internals->last_uac->next = uac;
	else euac_internals->first_uac = uac;
	uac->prev = euac_internals->last_uac;
	uac->next = NULL;
	euac_internals->last_uac = uac;
}

events_uac_t *create_events_uac(str *remote_uri, str *local_uri, const str *events, 
		notify_callback_func cb, /* callback function for processing NOTIFY messages (parsing, ...) */
		void *cbp, /* parameter for callback function */
		const str *other_headers, str *route)
{
	events_uac_t *uac;
	dstring_t dstr;

	if ((!remote_uri) || (!local_uri)) {
		ERR("invalid parameters\n");
		return NULL;
	}
	
	uac = (events_uac_t*)shm_malloc(sizeof(*uac));
	if (!uac) return NULL;
	
	/* compose headers */
	dstr_init(&dstr, 256);
	dstr_append_zt(&dstr, "Event: ");
	dstr_append_str(&dstr, events);
	dstr_append_zt(&dstr, "\r\n");
	if (other_headers) dstr_append_str(&dstr, other_headers);
	/* should get Accpet headers as parameter too? */
	dstr_get_str(&dstr, &uac->headers);
	dstr_destroy(&dstr);

	init_reference_counter(&uac->ref_cntr); /* main reference - removed in "destroyed" status */
	add_reference(&uac->ref_cntr); /* add reference for client */
	uac->status = euac_unconfirmed;
	str_dup(&uac->local_uri, local_uri);
	str_dup(&uac->remote_uri, remote_uri);
	str_dup(&uac->route, route);
	uac->timer_started = 0;
	uac->cb = cb;
	uac->cbp = cbp;

	lock_events_uac();
	insert_uac_to_list(uac);
	new_subscription(uac, NULL);
	unlock_events_uac();

	return uac;
}

int destroy_events_uac(events_uac_t *uac)
{
	lock_events_uac();

	/* TRACE("destroying uac: %p\n", uac); */
	
	/* do not receive any other status/service messages */
	uac->cb = NULL;
	uac->cbp = NULL;

	/* remove our reference, after unlock can be
	 * the uac structure removed from memory by anybody !*/
	if (!remove_euac_reference_nolock(uac)) {
		euac_do_step(act_destroy, NULL, uac);
	}
	
	unlock_events_uac();
	
	return 0;
}

int process_euac_notify(struct sip_msg* m)
{
	events_uac_t *uac;
	
	lock_events_uac();
	uac = find_euac_nolock(m);
	if (!uac) {
		unlock_events_uac();
		return -1;
	}
	euac_do_step(act_notify, m, uac);
	unlock_events_uac();
	return 0;
}

/* ---- INITIALIZATION/DESTRUCTION ---- */

int events_uac_init()
{
	if (!euac_internals) return init_events_uac_internals();
	else return 0;
}

void events_uac_destroy()
{
	if (euac_internals) destroy_events_uac_internals();
}

