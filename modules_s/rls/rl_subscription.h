#ifndef __RL_SUBSCRIPTION_H
#define __RL_SUBSCRIPTION_H

#include "../../modules/tm/dlg.h"
#include "../../lock_ops.h"
#include "subscription_manager.h"
#include <xcap/resource_lists_parser.h>
#include <cds/vector.h>
#include <cds/ptr_vector.h>
#include <cds/sstr.h>

#include <presence/subscriber.h>
#include <presence/notifier.h>
#include <xcap/resource_list.h>

#include "trace.h"

/* type for generated database ID */
typedef char db_id_t[48];

typedef enum {
	rls_auth_none, 
	rls_auth_implicit,
	rls_auth_xcap
} rls_authorization_type_t;

typedef struct {
	rls_authorization_type_t type;
} rls_auth_params_t;

struct _virtual_subscription_t;
struct _rl_subscription_t;
typedef struct _rl_subscription_t rl_subscription_t;

typedef struct {
	str name;
	str lang;
} vs_display_name_t;

typedef struct _virtual_subscription_t {
	/* helper to reduce memory allocations */
	qsa_subscription_data_t local_subscription_pres_data;
	
	/* local subscription data */
	qsa_subscription_t *local_subscription_pres;
	rl_subscription_t *local_subscription_list;
	
	vector_t display_names;

	rl_subscription_t *subscription;

	int changed;
	
	/** whole document describing the state of this resource */
	str state_document;
	/** type of this state_document */
	str content_type;
	/** status of this subscription */
	subscription_status_t status;
	
	/* VS identifier */
	str uri;
	
	struct _virtual_subscription_t *next;
	struct _virtual_subscription_t *prev;

	/* generated id for database */
	db_id_t dbid;

	char uri_str[1];
} virtual_subscription_t;

typedef enum { 
	rls_internal_subscription, 
	rls_external_subscription 
} rls_subscription_type_t;

typedef struct {
	str *package; /* points to "parent" subscription */
	str *record_id; /* NEVER free this - it points into VS data */
	str *subscriber_id; /* NEVER free this - it points into "parent" subscription */
	
	/* created from this virtual subscription */
	virtual_subscription_t *vs;

} internal_subscription_data_t;

/** subscription to the list of resources */
struct _rl_subscription_t {
	rls_subscription_type_t type;

	/* XCAP server settings (needed for reloading internal subscriptions
	 * from DB, XCAP notifications, ...) */
	xcap_query_params_t xcap_params;
	
	union {
		/* data of external subscription */
		subscription_data_t external;

		/* data of internal subscription (pointer to "parent" 
		 * virtual subscription) */
		internal_subscription_data_t internal;
	} u;
	
	/** sequence number of NOTIFY */
	int doc_version;

	/** the count of changed virtual subscriptions 
	 * (enough changes ?= send notify) */
	int changed;
	
	/* virtual subscriptions for this rls */
	ptr_vector_t vs;

	/* uid of the watcher */
	str from_uid;

	/* generated id for database */
	db_id_t dbid;
};

str_t * rls_get_package(rl_subscription_t *s);
str_t * rls_get_uri(rl_subscription_t *s);
str_t * rls_get_subscriber(rl_subscription_t *subscription);

/********* resource list subscription functions ********/

int rls_create_subscription(struct sip_msg *m, 
		rl_subscription_t **dst, 
		flat_list_t *flat, 
		xcap_query_params_t *params);
int rls_create_internal_subscription(virtual_subscription_t *vs, 
		rl_subscription_t **dst, 
		flat_list_t *flat,
		int nesting_level);
int rls_refresh_subscription(struct sip_msg *m, rl_subscription_t *s);
int rls_find_subscription(str *from_tag, str *to_tag, str *call_id, rl_subscription_t **dst);
void rls_free(rl_subscription_t *s); /* removes from memory only */
void rls_remove(rl_subscription_t *s); /* finishes subscription - removes from DB, mem, ... */
int rls_generate_notify(rl_subscription_t *s, int full_info);
int rls_prepare_subscription_response(rl_subscription_t *s, struct sip_msg *m);

/* void rls_notify_all_modified(); */

/********* virtual subscription functions ********/

int vs_init();
int vs_destroy();

int vs_create(str *uri, 
		virtual_subscription_t **dst, 
		display_name_t *dnames, 
		rl_subscription_t *subscription,
		int nesting_level);
int vs_add_display_name(virtual_subscription_t *vs, const char *name, const char *lang);
void vs_free(virtual_subscription_t *vs);
int create_virtual_subscriptions(rl_subscription_t *ss,
		int nesting_level);
int add_virtual_subscriptions(rl_subscription_t *ss, 
		flat_list_t *flat,
		int nesting_level);

/* database operations */
int rls_db_add(rl_subscription_t *s);
int rls_db_remove(rl_subscription_t *s);
int rls_db_update(rl_subscription_t *s);
int db_load_rls(); /* load stored subscriptions on startup */

/* helper functions */
void generate_db_id(db_id_t *id, void *data);

/** returns the count of seconds remaining to subscription expiration */
int rls_subscription_expires_in(rl_subscription_t *s);

/* allocates and initializes structure */
rl_subscription_t *rls_alloc_subscription(rls_subscription_type_t type);

/* XCAP queries */
int xcap_query_rls_services(xcap_query_params_t *xcap_params,
		const str *uri, const str *package, 
		flat_list_t **dst);

/* internal notification */
void process_internal_notify(virtual_subscription_t *vs, 
		str_t *new_state_document,
		str_t *new_content_type);

void process_rls_notification(virtual_subscription_t *vs, client_notify_info_t *info);
 
#endif
