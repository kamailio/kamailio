#ifndef __EUAC_INTERNALS_H
#define __EUAC_INTERNALS_H

/* internal structures */

#include "events_uac.h"
#include <cds/hash_table.h>
#include <cds/ref_cntr.h>
#include "../../modules/tm/tm_load.h"
#include "trace.h"

typedef struct {
	events_uac_t *first_uac;
	events_uac_t *last_uac;
	cds_mutex_t mutex;
	
	/* two hash tables for established-dialogs(from, to, callid) 
	 * and for non-established dialogs (from, callid) 
	 * as key is used dlg_id_t* !!! */
	hash_table_t ht_confirmed; /* hashed according dialog ids */
	hash_table_t ht_unconfirmed; /* hashed according partial dialog ids */
	
	struct tm_binds tmb;
	dlg_func_t dlgb;

	/* members for trace */
	int create_cnt;
	int destroy_cnt;

	reference_counter_group_t *rc_grp;
} events_uacs_internals_t;

extern events_uacs_internals_t *euac_internals;

int init_events_uac_internals();
void destroy_events_uac_internals();
void lock_events_uac();
void unlock_events_uac();

#endif
