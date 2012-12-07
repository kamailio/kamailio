#ifndef __EUAC_FUNCS_H
#define __EUAC_FUNCS_H

#include "events_uac.h"

/* manipulation with message */
void extract_contact(struct sip_msg *m, str *dst);

/* creating/recreating subscriptions 
 * if failover_time > 0 it calls euac_set_timer to this value 
 * noth these functions returns nonzero on error - this MUST
 * be handled everywhere */
int new_subscription(events_uac_t *uac, str *contact_to_send, int failover_time);
int renew_subscription(events_uac_t *uac, int expires, int failover_time);

/* */
events_uac_t *find_euac_nolock(struct sip_msg *m);
void euac_set_timer(events_uac_t *uac, int seconds);
void euac_clear_timer(events_uac_t *uac);
int remove_euac_reference_nolock(events_uac_t *uac);
void remove_uac_from_list(events_uac_t *uac);
void insert_uac_to_list(events_uac_t *uac);

/* processing NOTIFY requests (always sends OK response) */
void do_notification(events_uac_t *uac, struct sip_msg *m);
void discard_notification(events_uac_t *uac, struct sip_msg *m, int res_code, char *msg);
void refresh_dialog(events_uac_t *uac, struct sip_msg *m);
void refresh_dialog_resp(events_uac_t *uac, struct sip_msg *m);

/* waiting time after error (before new attempt about subscription) */
extern int resubscribe_timeout_on_err; 

/* time specifying how long wait for terminating NOTIFY
 * after 2xx response on SUBSCRIBE with 0 expires*/
extern int waiting_for_notify_time; 

/* default subscription duration */
extern int subscribe_time;

/* time interval before expiration when should be the subscription refreshed
 * (recommended are some seconds before) */
extern int resubscribe_delta;

/* minimum time for resubscriptions */
extern int min_resubscribe_time;

extern int failover_timeout;

/* for randomized start of subscriptions */
extern int max_subscribe_delay;

#endif
