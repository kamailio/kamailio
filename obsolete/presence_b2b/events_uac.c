#include "events_uac.h"
#include "euac_internals.h" /* for debugging purposes */
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

void free_events_uac(events_uac_t *uac)
{
	/* TRACE("freeing EUAC %p\n", uac); */

	str_free_content(&uac->headers);
	str_free_content(&uac->local_uri);
	str_free_content(&uac->remote_uri);
	str_free_content(&uac->route);
	str_free_content(&uac->outbound_proxy);
	/* if the dialog is not freed we should free it */
	if (uac->dialog) {
		euac_internals->tmb.free_dlg(uac->dialog);
		/* TRACE("freeing dialog for EUAC %p\n", uac); */
	}
	mem_free(uac);
}

events_uac_t *create_events_uac(str *remote_uri, str *local_uri, const str *events, 
		notify_callback_func cb, /* callback function for processing NOTIFY messages (parsing, ...) */
		void *cbp, /* parameter for callback function */
		const str *other_headers, str *route, str *outbound_proxy)
{
	events_uac_t *uac;
	dstring_t dstr;
	int res = 0;

	if ((!remote_uri) || (!local_uri)) {
		ERR("invalid parameters\n");
		return NULL;
	}
	
	uac = (events_uac_t*)mem_alloc(sizeof(*uac));
	if (!uac) return NULL;
	
	/* compose headers */
	dstr_init(&dstr, 256);
	dstr_append_zt(&dstr, "Event: ");
	dstr_append_str(&dstr, events);
	dstr_append_zt(&dstr, "\r\n");

	/* required by RFC 3261 */
	dstr_append_zt(&dstr, "Max-Forwards: 70\r\n"); 
	/* needed for SUBSCRIBE via TCP - if not given the message is not parsed by ser!!! */
	dstr_append_zt(&dstr, "Content-Length: 0\r\n"); 
	
	if (other_headers) dstr_append_str(&dstr, other_headers);
	/* should get Accpet headers as parameter too? */
	if (dstr_get_str(&dstr, &uac->headers) != 0) {
		ERR("can't generate headers (no mem)\n");
		dstr_destroy(&dstr);
		mem_free(uac);
		return NULL;
	}
	dstr_destroy(&dstr);

	uac->dialog = NULL;
	init_reference_counter(euac_internals->rc_grp, &uac->ref_cntr); /* main reference - removed in "destroyed" status */
	add_reference(&uac->ref_cntr); /* add reference for client */
	/*TRACE("[%s]: added reference (%d)\n", "???", 
				uac->ref_cntr.cntr);*/
	uac->status = euac_unconfirmed;
	res = str_dup(&uac->local_uri, local_uri);
	if (res == 0) res = str_dup(&uac->remote_uri, remote_uri);
	else str_clear(&uac->remote_uri);
	if (res == 0) res = str_dup(&uac->route, route);
	else str_clear(&uac->route);
	if (res == 0) res = str_dup(&uac->outbound_proxy, outbound_proxy);
	else str_clear(&uac->outbound_proxy);
	uac->timer_started = 0;
	uac->cb = cb;
	uac->cbp = cbp;

	if (res != 0) { 
		ERR("can't duplicate parameters\n");
		free_events_uac(uac);
		return NULL;
	}

	lock_events_uac();
	sprintf(uac->id, "%p:%x:%x", uac, (unsigned int)time(NULL), rand());
	euac_internals->create_cnt++;
	insert_uac_to_list(uac);
	euac_start(uac);
	unlock_events_uac();

	return uac;
}

int destroy_events_uac(events_uac_t *uac)
{
	if (!uac) {
		ERR("BUG: destroying empty uac\n");
		return -1;
	}

	lock_events_uac();
	euac_internals->destroy_cnt++;

	 DBG("destroying uac %d from: %d\n", 
		euac_internals->destroy_cnt,
		euac_internals->create_cnt);
	
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

