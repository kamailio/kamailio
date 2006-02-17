#include "euac_state_machine.h"
#include "euac_internals.h"
#include "euac_funcs.h"

/* waiting time after error (before new attempt about subscription) */
int resubscribe_timeout_on_err = 120; 

/* time specifying how long wait for terminating NOTIFY
 * after 2xx response on SUBSCRIBE with 0 expires*/
int waiting_for_notify_time = 30; 

int subscribe_time = 3600;

/* time interval before expiration when should be the subscription refreshed
 * (recommended are some seconds before) */
int resubscribe_delta = 30;

/* minimum time for resubscriptions */
int min_resubscribe_time = 30;

/* helper functions for internal usage */

static int get_resubscribe_time(struct sip_msg *m)
{
	int expiration = get_expiration_value(m);

	expiration -= resubscribe_delta;
	if (expiration < min_resubscribe_time) expiration = min_resubscribe_time;
	return expiration;
}

static void confirm_dialog(events_uac_t *uac, struct sip_msg *m)
{
	/* remove dialog from, unconfirmed */
	ht_remove(&euac_internals->ht_unconfirmed, &uac->dialog->id);	
	
	/* process confirmation response */
	euac_internals->tmb.dlg_response_uac(uac->dialog, m);

	/* add to confirmed dialogs */
	DBG("adding into confirmed EUACs\n");
	ht_add(&euac_internals->ht_confirmed, &uac->dialog->id, uac);

	/* TRACE("confirmed dialog: %.*s * %.*s * %.*s\n",
			FMT_STR(uac->dialog->id.loc_tag),
			FMT_STR(uac->dialog->id.rem_tag),
			FMT_STR(uac->dialog->id.call_id)); */
}

static void destroy_unconfirmed_dialog(events_uac_t *uac)
{
	/* remove dialog from, unconfirmed */
	ht_remove(&euac_internals->ht_unconfirmed, &uac->dialog->id);	
	if (uac->dialog) {
		euac_internals->tmb.free_dlg(uac->dialog);
		uac->dialog = NULL;
	}
}

static void destroy_confirmed_dialog(events_uac_t *uac)
{
	/* remove dialog from, confirmed */
	ht_remove(&euac_internals->ht_confirmed, &uac->dialog->id);	
	if (uac->dialog) {
		euac_internals->tmb.free_dlg(uac->dialog);
		uac->dialog = NULL;
	}
}
	
/* -------------------------------------------------------------------- */
/* changing state machine status functions */

void do_step_unconfirmed(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	str contact = STR_NULL;
	int expires = 0;

	switch (action) {
		case act_notify:
				refresh_dialog(uac, m); /* ? */
				do_notification(uac, m);
				break;
		case act_1xx: break;
		case act_2xx:
				uac->status = euac_confirmed;
				confirm_dialog(uac, m);	
				expires = get_resubscribe_time(m);
				/* DBG("expires after %d seconds\n", expires); */
				euac_set_timer(uac, expires);
				break;
		case act_3xx:
				destroy_unconfirmed_dialog(uac);
				extract_contact(m, &contact);
				if (!is_str_empty(&contact)) {
					new_subscription(uac, &contact);
					str_free_content(&contact);
				}
				else { 
					/* redirect, but no contact given => process like error */
					uac->status = euac_waiting;
					euac_set_timer(uac, resubscribe_timeout_on_err);
				}
				break;
		case act_4xx: /* 4xx, 5xx, ... */
				destroy_unconfirmed_dialog(uac);
				uac->status = euac_waiting;
				euac_set_timer(uac, resubscribe_timeout_on_err);
				break;
		case act_destroy:
				uac->status = euac_unconfirmed_destroy;
				break;
		case act_tick:
			ERR("BUG: invalid action %d\n", action);
			break;
	}
}
				
void do_step_unconfirmed_destroy(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	int expires = 0;

	switch (action) {
		case act_1xx:
		case act_destroy: break;
		case act_notify:
			refresh_dialog(uac, m);
			discard_notification(uac, m, 200, "OK");
			break;
						 
		case act_2xx:
			uac->status = euac_confirmed;
			confirm_dialog(uac, m);	
			expires = get_resubscribe_time(m);
			/* if (expires == 0) wait_for_terminating_notify(uac);
			else */
			renew_subscription(uac, 0);
			uac->status = euac_predestroyed;
			break;
		case act_3xx:
		case act_4xx:
			destroy_unconfirmed_dialog(uac);
			uac->status = euac_destroyed;
			break;
		case act_tick:
			ERR("BUG: invalid action %d\n", action);
			break;
	}
}

void do_step_resubscription_destroy(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_1xx:
		case act_destroy: break;
		case act_notify:
			refresh_dialog(uac, m);
			discard_notification(uac, m, 200, "OK");
			break;
						 
		case act_2xx:
			uac->status = euac_confirmed;
			confirm_dialog(uac, m);	
			/* expires = get_expiration_value(m);
			if (expires == 0) wait_for_terminating_notify(uac);
			else */
			renew_subscription(uac, 0);
			uac->status = euac_predestroyed;
			break;
		case act_3xx:
		case act_4xx:
			destroy_confirmed_dialog(uac);
			uac->status = euac_destroyed;
			break;
		case act_tick:
			ERR("BUG: invalid action %d\n", action);
			break;
	}
}

void do_step_confirmed(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	/* int expires = 0; process the value present in Subscription-State header? */

	switch (action) {
		case act_1xx:
		case act_2xx:
		case act_3xx:
		case act_4xx:
			ERR("BUG: invalid action %d\n", action);
			break;
			
		case act_destroy:
			euac_clear_timer(uac);
			renew_subscription(uac, 0);
			uac->status = euac_predestroyed;
			break;
		case act_notify: 
			refresh_dialog(uac, m);	
			do_notification(uac, m);
			break;
		case act_tick:
			renew_subscription(uac, subscribe_time);
			uac->status = euac_resubscription;
			break;
	}
}

void do_step_resubscription(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	str contact = STR_NULL;
	int expires = 0;

	switch (action) {
		case act_notify:
				refresh_dialog(uac, m);
				do_notification(uac, m);
				break;
		case act_1xx: break;
		case act_2xx:
				uac->status = euac_confirmed;
				refresh_dialog_resp(uac, m);
				expires = get_resubscribe_time(m);
				euac_set_timer(uac, expires);
				break;
		case act_3xx:
				destroy_confirmed_dialog(uac);
				extract_contact(m, &contact);
				if (!is_str_empty(&contact)) {
					uac->status = euac_unconfirmed;
					new_subscription(uac, &contact);
					str_free_content(&contact);
				}
				else { 
					/* redirect, but no contact given => process like error */
					uac->status = euac_waiting;
					euac_set_timer(uac, resubscribe_timeout_on_err);
				}
				break;
		case act_4xx: /* 4xx, 5xx, ... */
				destroy_confirmed_dialog(uac);
				uac->status = euac_waiting;
				euac_set_timer(uac, resubscribe_timeout_on_err);
				break;
		case act_destroy:
				uac->status = euac_resubscription_destroy;
				break;
		case act_tick:
			ERR("BUG: invalid action %d\n", action);
			break;
	}
}

void do_step_resubscribe_destroy(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_1xx:
		case act_destroy: break;
		case act_notify:
			refresh_dialog(uac, m);
			discard_notification(uac, m, 200, "OK");
			break;
						 
		case act_2xx:
				refresh_dialog_resp(uac, m);	
				renew_subscription(uac, 0);
				uac->status = euac_predestroyed;
				break;
		case act_3xx:
		case act_4xx:
				destroy_confirmed_dialog(uac);
				uac->status = euac_destroyed;
				break;
		case act_tick:
			ERR("BUG: invalid action %d\n", action);
			break;
	}
}

void do_step_destroyed(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_destroy: break;
		case act_notify:
			WARN("received NOTIFY for destroyed dialog !\n");
			discard_notification(uac, m, 481, "Subscription does not exist");
			break;
		default:
			ERR("action not allowed (%d)\n", action);
			break;
	}
}

void do_step_predestroyed(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_notify:
				refresh_dialog(uac, m);
				discard_notification(uac, m, 200, "OK");
				break;
		case act_1xx: break;
		case act_2xx:
				uac->status = euac_waiting_for_termination;
				euac_set_timer(uac, waiting_for_notify_time);
				break;
		case act_3xx:
		case act_4xx:
				destroy_confirmed_dialog(uac);
				uac->status = euac_destroyed;
				break;
		case act_destroy:
				break;
		case act_tick:
			ERR("action not allowed (%d)\n", action);
			break;
	}
}

void do_step_waiting_for_term_notify(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_notify:
				discard_notification(uac, m, 200,  "OK");
				if (is_terminating_notify(m)) {
					destroy_confirmed_dialog(uac);
					euac_clear_timer(uac);
					uac->status = euac_destroyed;
					/* DBG("destroying dialog (NOTIFY)\n"); */
				}
				else {
					DBG("discarding NOTIFY (not terminating)\n"); 
				}
				break;
		case act_tick:
				/* wait no more */
				uac->status = euac_destroyed;
				destroy_confirmed_dialog(uac);
				/* DBG("destroying dialog (timer)\n"); */
				break;
		case act_1xx:
		case act_2xx:
		case act_3xx:
		case act_4xx:
				ERR("action not allowed (%d)\n", action);
				break;
		case act_destroy:
				break;
	}
}

void do_step_waiting(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_1xx:
		case act_2xx:
		case act_3xx:
		case act_4xx:
				ERR("action not allowed (%d)\n", action);
				break;
		case act_notify:
				ERR("action not allowed (%d) - discarding NOTIFY for non 2xx subscription\n", action);
				discard_notification(uac, m, 500,  "Internal error");
				break;
		case act_destroy:
				euac_clear_timer(uac);
				uac->status = euac_destroyed;
				break;
		case act_tick:
			new_subscription(uac, NULL);
			uac->status = euac_unconfirmed;
			break;
	}
}

/* this function can remove the uac from memory as a side effect 
 * thus it should be the last action done on UAC (or you have to
 * add reference before this call)!!! */
void euac_do_step(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	int was_destroyed = (uac->status == euac_destroyed);
	/* events_uac_status_t old_status = uac->status;
	TRACE("euac do step (%p)\n", uac); */
	
	switch (uac->status) {
		case euac_unconfirmed:
			do_step_unconfirmed(action, m, uac);
			break;
		case euac_unconfirmed_destroy:
			do_step_unconfirmed_destroy(action, m, uac);
			break;
		case euac_confirmed:
			do_step_confirmed(action, m, uac);
			break;
		case euac_waiting:
			do_step_waiting(action, m, uac);
			break;
		case euac_resubscription:
			do_step_resubscription(action, m, uac);
			break;
		case euac_resubscription_destroy:
			do_step_resubscription_destroy(action, m, uac);
			break;
		case euac_waiting_for_termination:
			do_step_waiting_for_term_notify(action, m, uac);
			break;
		case euac_predestroyed:
			do_step_predestroyed(action, m, uac);
			break;
		case euac_destroyed:
			do_step_destroyed(action, m, uac);
			break; 
	}
	
/*	TRACE("euac step (%p) %d ---(%d)---> %d\n", 
			uac, old_status, action, uac->status); */	

	if ((!was_destroyed) && (uac->status == euac_destroyed)) {
		/* remove reference if UAC going to destroyed status (ONLY
		 * when moving !!!), it is not needed more */
		remove_euac_reference_nolock(uac);
	}
}
