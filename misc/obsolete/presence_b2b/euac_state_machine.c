#include "euac_state_machine.h"
#include "euac_internals.h"
#include "euac_funcs.h"
#include <cds/sip_utils.h>

/* if set to number > 0 SUBSCRIBE requests are sent randomly, at last 
 * after max_subscribe_delay seconds */
int max_subscribe_delay = 0;

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

/* timeout for failover */
int failover_timeout = 60;

/* helper functions for internal usage */

static void accept_response(events_uac_t *uac, euac_action_t action)
{
	if (action != act_1xx) {
		/* remove reference reserved for cb (final responses 
		 * only or timer tick instead of 408) */
		remove_euac_reference_nolock(uac); 
	}
}

static void decline_response(events_uac_t *uac, euac_action_t action)
{
	ERR("[%s]: out of order response action = %d) (BUG?)\n", uac->id, action);
	if (action != act_1xx) {
		remove_euac_reference_nolock(uac); 
	}
}


static int get_resubscribe_time(struct sip_msg *m)
{
	int expiration;
	
	if (get_expiration_value(m, &expiration) != 0) expiration = 0;

	expiration -= resubscribe_delta;
	if (expiration < min_resubscribe_time) expiration = min_resubscribe_time;
	return expiration;
}

static void send_error_notification(events_uac_t *uac)
{
	/* sends innternal notification about arror status */
}

static void confirm_dialog(events_uac_t *uac, struct sip_msg *m)
{
	/* remove dialog from, unconfirmed */
	ht_remove(&euac_internals->ht_unconfirmed, &uac->dialog->id);	
	
	/* process confirmation response */
	euac_internals->tmb.dlg_response_uac(uac->dialog, m, IS_TARGET_REFRESH);

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
		case act_1xx: 
				accept_response(uac, action);
				break;
		case act_2xx:
				uac->status = euac_confirmed;
				euac_clear_timer(uac);
				confirm_dialog(uac, m);	
				expires = get_resubscribe_time(m);
				/* DBG("expires after %d seconds\n", expires); */
				euac_set_timer(uac, expires);
				accept_response(uac, action);
				break;
		case act_3xx:
				accept_response(uac, action);
				euac_clear_timer(uac);
				destroy_unconfirmed_dialog(uac);
				extract_contact(m, &contact);
				if (!is_str_empty(&contact)) {
					if (new_subscription(uac, &contact, failover_timeout) != 0) {
						/* error */
						uac->status = euac_waiting;
						send_error_notification(uac);
						euac_set_timer(uac, resubscribe_timeout_on_err);
					}
					str_free_content(&contact);
				}
				else { 
					/* redirect, but no contact given => process like error */
					uac->status = euac_waiting;
					send_error_notification(uac);
					euac_set_timer(uac, resubscribe_timeout_on_err);
				}
				break;
		case act_tick:
		case act_4xx: /* 4xx, 5xx, ... */
				uac->status = euac_waiting;
				euac_clear_timer(uac);
				destroy_unconfirmed_dialog(uac);
				send_error_notification(uac);
				euac_set_timer(uac, resubscribe_timeout_on_err);
				accept_response(uac, action);
				break;
		case act_destroy:
				uac->status = euac_unconfirmed_destroy;
				break;
	}
}
				
void do_step_unconfirmed_destroy(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	int expires = 0;

	switch (action) {
		case act_1xx: 
				accept_response(uac, action);
				break;
		case act_destroy: break;
		case act_notify:
			refresh_dialog(uac, m);
			discard_notification(uac, m, 200, "OK");
			break;
						 
		case act_2xx:
			accept_response(uac, action);
			euac_clear_timer(uac);
			confirm_dialog(uac, m);	
			expires = get_resubscribe_time(m);
			/* if (expires == 0) wait_for_terminating_notify(uac);
			else */
			uac->status = euac_predestroyed;
			if (renew_subscription(uac, 0, failover_timeout) != 0) {
				/* error */
				uac->status = euac_destroyed;
				destroy_confirmed_dialog(uac);
				remove_euac_reference_nolock(uac); /* free EUAC */
			}
			break;
		case act_3xx:
		case act_4xx: 
		case act_tick: /* accept response too! */
			uac->status = euac_destroyed;
			accept_response(uac, action);
			euac_clear_timer(uac);
			destroy_unconfirmed_dialog(uac);
			remove_euac_reference_nolock(uac); /* free EUAC */
			break;
	}
}

void do_step_resubscription_destroy(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_1xx: 
			accept_response(uac, action);
			break;
		case act_destroy: break;
		case act_notify:
			refresh_dialog(uac, m);
			discard_notification(uac, m, 200, "OK");
			break;
						
		case act_2xx:
			accept_response(uac, action);
			euac_clear_timer(uac);
			/* expires = get_expiration_value(m);
			if (expires == 0) wait_for_terminating_notify(uac);
			else */
			uac->status = euac_predestroyed;
			if (renew_subscription(uac, 0, failover_timeout) != 0) {
				/* error */
				uac->status = euac_destroyed;
				destroy_confirmed_dialog(uac);
				remove_euac_reference_nolock(uac); /* free EUAC */			
			}
			break;
		case act_3xx:
		case act_4xx:
		case act_tick:
			uac->status = euac_destroyed;
			accept_response(uac, action);
			euac_clear_timer(uac);
			destroy_confirmed_dialog(uac);
			remove_euac_reference_nolock(uac); /* free EUAC */
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
			decline_response(uac, action);
			ERR("[%s]: invalid action %d (BUG?)\n", uac->id, action);
			break;
			
		case act_destroy:
			uac->status = euac_predestroyed;
			euac_clear_timer(uac);
			if (renew_subscription(uac, 0, failover_timeout) != 0) {
				/* error */
				uac->status = euac_destroyed;
				destroy_confirmed_dialog(uac);
				remove_euac_reference_nolock(uac); /* free EUAC */
			}
			break;
		case act_notify: 
			refresh_dialog(uac, m);	
			do_notification(uac, m);
			break;
		case act_tick:
			uac->status = euac_resubscription;
			if (renew_subscription(uac, subscribe_time, failover_timeout) != 0) {
				/* error */
				uac->status = euac_waiting;
				euac_clear_timer(uac);
				destroy_confirmed_dialog(uac);
				send_error_notification(uac);
				euac_set_timer(uac, resubscribe_timeout_on_err);
				break;
			}
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
		case act_1xx: 
				accept_response(uac, action);
				break;
		case act_2xx:
				uac->status = euac_confirmed;
				accept_response(uac, action);
				euac_clear_timer(uac);
				refresh_dialog_resp(uac, m);
				expires = get_resubscribe_time(m);
				euac_set_timer(uac, expires);
				break;
		case act_3xx:
				accept_response(uac, action);
				euac_clear_timer(uac);
				destroy_confirmed_dialog(uac);
				extract_contact(m, &contact);
				if (!is_str_empty(&contact)) {
					uac->status = euac_unconfirmed;
					if (new_subscription(uac, &contact, failover_timeout) != 0) {
						/* error */
						uac->status = euac_waiting;
						send_error_notification(uac);
						euac_set_timer(uac, resubscribe_timeout_on_err);
					}
					str_free_content(&contact);
				}
				else { 
					/* redirect, but no contact given => process like error */
					uac->status = euac_waiting;
					send_error_notification(uac);
					euac_set_timer(uac, resubscribe_timeout_on_err);
				}
				break;
		case act_tick:
		case act_4xx: /* 4xx, 5xx, ... */
				uac->status = euac_waiting;
				accept_response(uac, action);
				euac_clear_timer(uac);
				destroy_confirmed_dialog(uac);
				send_error_notification(uac);
				euac_set_timer(uac, resubscribe_timeout_on_err);
				break;
		case act_destroy:
				uac->status = euac_resubscription_destroy;
				break;
	}
}

void do_step_resubscribe_destroy(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_1xx: 
				accept_response(uac, action);
				break;
		case act_destroy: break;
		case act_notify:
			refresh_dialog(uac, m);
			discard_notification(uac, m, 200, "OK");
			break;
						 
		case act_2xx:
				uac->status = euac_predestroyed;
				accept_response(uac, action);
				euac_clear_timer(uac);
				refresh_dialog_resp(uac, m);	
				if (renew_subscription(uac, 0, failover_timeout) != 0) {
					uac->status = euac_destroyed;
					destroy_confirmed_dialog(uac);
					remove_euac_reference_nolock(uac); /* free EUAC */
				}
				break;
		case act_tick:
		case act_3xx:
		case act_4xx:
				uac->status = euac_destroyed;
				accept_response(uac, action);
				euac_clear_timer(uac);
				destroy_confirmed_dialog(uac);
				remove_euac_reference_nolock(uac); /* free EUAC */
				break;
	}
}

void do_step_destroyed(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_destroy: break;
		case act_notify:
			WARN("[%s]: received NOTIFY for destroyed dialog !\n", uac->id);
			discard_notification(uac, m, 481, "Subscription does not exist");
			break;

		/* response can be received because the step predestroyed -> destroyed could 
		 * be done after receiving terminating NOTIFY (before response) */
		case act_2xx:
		case act_3xx:
		case act_4xx: 
			accept_response(uac, action);
			break;
			
		default:
			ERR("[%s]: action not allowed (%d) (BUG?)\n", uac->id, action);
			break;
	}
}

void do_step_predestroyed(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
	switch (action) {
		case act_notify:
				refresh_dialog(uac, m);
				discard_notification(uac, m, 200, "OK");
				if (is_terminating_notify(m)) {
					destroy_confirmed_dialog(uac);
					euac_clear_timer(uac);
					uac->status = euac_destroyed;
					/* DBG("destroying dialog (NOTIFY)\n"); */
					remove_euac_reference_nolock(uac); /* free EUAC */
				}
				break;
		case act_1xx: 
				accept_response(uac, action);
				break;
		case act_2xx:
				uac->status = euac_waiting_for_termination;
				euac_clear_timer(uac);
				euac_set_timer(uac, waiting_for_notify_time);
				accept_response(uac, action);
				break;
		case act_tick:
		case act_3xx:
		case act_4xx:
				uac->status = euac_destroyed;
				euac_clear_timer(uac);
				destroy_confirmed_dialog(uac);
				accept_response(uac, action);
				remove_euac_reference_nolock(uac); /* free EUAC */
				break;
		case act_destroy:
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
					remove_euac_reference_nolock(uac); /* free EUAC */
				}
				else {
					DBG("discarding NOTIFY (not terminating)\n"); 
				}
				break;
		case act_tick:
				/* wait no more */
				if (!uac->dialog) WARN("[%s]: destroying dialog with timer (no term NOTIFY)!\n", uac->id);
				else WARN("[%s]: destroying dialog with timer (no term NOTIFY; %.*s, %.*s, %.*s)!\n", 
						uac->id,
						FMT_STR(uac->dialog->id.loc_tag),
						FMT_STR(uac->dialog->id.rem_tag),
						FMT_STR(uac->dialog->id.call_id));
				uac->status = euac_destroyed;
				destroy_confirmed_dialog(uac);
				remove_euac_reference_nolock(uac); /* free EUAC */
				break;
		case act_1xx:
		case act_2xx:
		case act_3xx:
		case act_4xx:
				decline_response(uac, action);
				ERR("[%s]: action not allowed (%d) (BUG?)\n", uac->id, action);
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
				decline_response(uac, action);
				ERR("[%s]: action not allowed (%d) (BUG?)\n", uac->id, action);
				break;
		case act_notify:
				ERR("[%s]: action not allowed (%d) (BUG?)- "
						"discarding NOTIFY for non established subscription\n", 
						uac->id, action);
				discard_notification(uac, m, 500,  "Internal error");
				break;
		case act_destroy:
				uac->status = euac_destroyed;
				euac_clear_timer(uac);
				remove_euac_reference_nolock(uac); /* free EUAC */
				break;
		case act_tick:
			uac->status = euac_unconfirmed;
			if (new_subscription(uac, NULL, failover_timeout) != 0) {
				/* error */
				uac->status = euac_waiting;
				euac_set_timer(uac, resubscribe_timeout_on_err);
			}
			break;
	}
}

/* this function can remove the uac from memory as a side effect 
 * thus it should be the last action done on UAC (or you have to
 * add reference before this call)!!! */
void euac_do_step(euac_action_t action, struct sip_msg *m, events_uac_t *uac)
{
/*	TRACE("STEP [%s]: %d ---(%d)---> ...\n", 
			uac->id, uac->status, action);*/
	
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
	
}

void euac_start(events_uac_t *uac)
{
	int subscribe_delay = 0;
	
	if (max_subscribe_delay > 0) {
		uac->status = euac_waiting;
		/* subscribe_delay = (double)rand() / (double)RAND_MAX * max_subscribe_delay; */
		subscribe_delay = rand() % (max_subscribe_delay) + 1; /* dissallow timer with 0 expiration */
		euac_set_timer(uac, subscribe_delay);
	}
	else {
		uac->status = euac_unconfirmed;
		if (new_subscription(uac, NULL, failover_timeout) != 0) {
			uac->status = euac_waiting;
			euac_set_timer(uac, resubscribe_timeout_on_err);
		}
	}
}

