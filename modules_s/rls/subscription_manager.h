#ifndef __SUBSCRIPTION_MANAGER_H
#define __SUBSCRIPTION_MANAGER_H

#include <cds/sstr.h>
#include "time_event_manager.h"
#include "../../modules/tm/dlg.h"
#include "trace.h"

struct _subscription_data_t;

typedef enum {
	auth_rejected,
	auth_polite_block,
	auth_unresolved,
	auth_granted
} authorization_result_t;

typedef int(*send_notify_func)(struct _subscription_data_t *s);
typedef int(*terminate_func)(struct _subscription_data_t *s);
typedef authorization_result_t (*subscription_authorize_func)(struct _subscription_data_t *s);

typedef enum {
	subscription_uninitialized,
	subscription_active,
	subscription_pending,
	subscription_terminated,
	subscription_terminated_to,	/* terminated timeout */
	subscription_terminated_pending,	/* terminated pending subscription */
	subscription_terminated_pending_to	/* terminated pending subscription (timeout) */
} subscription_status_t;

typedef struct _subscription_data_t {
	/** data for timer events */
	time_event_data_t expiration;
	/** SIP dialog structure */
	dlg_t *dialog;
	/** whatever user data */
	void *usr_data;
	/** the status of this subscription */
	subscription_status_t status;
	/** linking element */
	struct _subscription_data_t *next;
	/** linking element */
	struct _subscription_data_t *prev;
	/** contact for re-subscribe and responses */
	str_t contact;
	/** subscription destination identifier */
	str_t record_id;
	/** event package */
	str_t package;
	/** subscriber's uri (due to authorization) */
	str_t subscriber;	

} subscription_data_t;

typedef struct {
	subscription_data_t *first;
	subscription_data_t *last;

	/** callback function for notify message sending */
	send_notify_func notify;
	/** callback function for subscription terminating (timeout) */
	terminate_func terminate;
	/** callback function for authorization */
	subscription_authorize_func authorize;

	/** mutex given from caller (common for timer and subscription structures) */
	gen_lock_t *mutex;
	
	/** its own time event manager */
	time_event_manager_t timer;

	int default_expiration;
	int min_expiration;
	int max_expiration;
	
} subscription_manager_t;

/** initialization for all subscription managers - MUST be called */
int subscription_management_init(void);
	
/** create a new subscription manager */
subscription_manager_t *sm_create(send_notify_func notify, 
		terminate_func terminate, 
		subscription_authorize_func authorize,
		gen_lock_t *mutex,
		int min_exp, 
		int max_exp, 
		int default_exp,
		int expiration_timer_period);

/** initialize a new subscription manager */
int sm_init(subscription_manager_t *sm,
		send_notify_func notify, 
		terminate_func terminate, 
		subscription_authorize_func authorize,
		gen_lock_t *mutex,
		int min_exp, 
		int max_exp, 
		int default_exp,
		int expiration_timer_period);

/** initializes internal data members SIP dialog and status 
 * and intializes expiration timer */
int sm_init_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *dst, 
		struct sip_msg *m);

/** refreshes expiration timer and SIP dialog for given subscription
 * from given message */
int sm_refresh_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *s, 
		struct sip_msg *m);
		
/** releases timer and removes subscription from subscription
 * manager, but it doesn't free the occupied memory !*/
void sm_release_subscription_nolock(subscription_manager_t *mng,
		subscription_data_t *dst);

/** adds some response lumps into message */
int sm_prepare_subscription_response(subscription_manager_t *mng,
		subscription_data_t *s, 
		struct sip_msg *m);

/** finds subscription according to dialog id */
int sm_find_subscription(subscription_manager_t *mng,
		str_t *from_tag, str_t *to_tag, str_t *call_id, 
		subscription_data_t **dst);

/** returns 0 if the subscriptions is in one of terminated states */
int sm_subscription_terminated(subscription_data_t *s);

/** returns 0 if the subscriptions is in one of pending states */
int sm_subscription_pending(subscription_data_t *s);

/** returns the count of seconds remaining to subscription expiration */
int sm_subscription_expires_in(subscription_manager_t *mng,
		subscription_data_t *s);

int sm_init_subscription_nolock_ex(subscription_manager_t *mng,
		subscription_data_t *dst, 
		dlg_t *dialog,
		subscription_status_t status,
		const str_t *contact,
		const str_t *record_id,
		const str_t *package,
		const str_t *subscriber,
		int expires_after,
		void *subscription_data);

#endif
