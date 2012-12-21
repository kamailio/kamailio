#include "euac_internals.h"
#include "euac_funcs.h"

events_uacs_internals_t *euac_internals = NULL;

/* this function should move into dialog module ! */
static int cmp_unconfirmed_local_dlg_ids(dlg_id_t *a, dlg_id_t *b)
{
	if (!a) {
		if (!b) return -1;
		else return 0;
	}
	if (!b) return 1;

	if (str_case_equals(&a->call_id, &b->call_id) != 0) return 1;
	if (str_case_equals(&a->loc_tag, &b->loc_tag) != 0) return 1; /* case sensitive ? */
	/* FIXME: is really rem_tag the empty one? */
	return 0;
}

int cmp_uac_unconfirmed_dlg(ht_key_t a, ht_key_t b)
{
	events_uac_t *uac_a = (events_uac_t*)a;
	events_uac_t *uac_b = (events_uac_t*)b;
	dlg_t *dlg_a = NULL;
	dlg_t *dlg_b = NULL;
	
	if (uac_a) dlg_a = uac_a->dialog;
	if (uac_b) dlg_b = uac_b->dialog;
	
	if (dlg_a) {
		if (!dlg_b) return -1;
		else return cmp_unconfirmed_local_dlg_ids(&dlg_a->id, &dlg_b->id);
	}
	else {
		if (!dlg_b) return 0;
	}
	return -1;
}

int init_events_uac_internals()
{
    load_tm_f load_tm;
	bind_dlg_mod_f bind_dlg;
	
	/* must be called only once */
	euac_internals = mem_alloc(sizeof(*euac_internals));
	if (!euac_internals) {
		ERR("can't allocate memory for internal UAC structures\n");
		return -1;
	}
	
	/* import the TM auto-loading function */
	if ( !(load_tm=(load_tm_f)find_export("load_tm", NO_SCRIPT, 0))) {
		ERR("Can't import tm!\n");
		return -1;
	}
	/* let the auto-loading function load all TM stuff */
	if (load_tm(&euac_internals->tmb)==-1) {
		ERR("load_tm() failed\n");
		return -1;
	}
	
	/* import the dialog auto-loading function */
	bind_dlg = (bind_dlg_mod_f)find_export("bind_dlg_mod", -1, 0);
	if (!bind_dlg) {
		LOG(L_ERR, "Can't import dlg\n");
		return -1;
	}
	if (bind_dlg(&euac_internals->dlgb) != 0) {
		ERR("bind_dlg_mod() failed\n");
		return -1;
	}
	
	euac_internals->first_uac = NULL;
	euac_internals->last_uac = NULL;
	cds_mutex_init(&euac_internals->mutex);
	ht_init(&euac_internals->ht_confirmed, 
			(hash_func_t)euac_internals->dlgb.hash_dlg_id, 
			(key_cmp_func_t)euac_internals->dlgb.cmp_dlg_ids, 
			16603);
	ht_init(&euac_internals->ht_unconfirmed, 
			(hash_func_t)euac_internals->dlgb.hash_dlg_id, 
			(key_cmp_func_t)cmp_unconfirmed_local_dlg_ids, 
			2039);

	euac_internals->create_cnt = 0;
	euac_internals->destroy_cnt = 0;
	euac_internals->rc_grp = create_reference_counter_group(16);

	return 0;
}

void destroy_events_uac_internals()
{
	events_uac_t *uac;

	if (euac_internals) {
		uac = euac_internals->first_uac;
		euac_internals->first_uac = NULL;
		euac_internals->last_uac = NULL;
		while (uac) {
			/* make sure that it will not change prev and next elements
			 * during remove_euac_reference */
			uac->prev = NULL;
			uac->next = NULL;
			
			remove_euac_reference_nolock(uac);
			uac = uac->next;
		}
		
		ht_destroy(&euac_internals->ht_confirmed);
		ht_destroy(&euac_internals->ht_unconfirmed);
		cds_mutex_destroy(&euac_internals->mutex);
		mem_free(euac_internals);
		euac_internals = NULL;
	}
	/* TRACE("Events uac destroyed\n"); */
}

void lock_events_uac()
{
	if (euac_internals) cds_mutex_lock(&euac_internals->mutex);
}

void unlock_events_uac()
{
	if (euac_internals) cds_mutex_unlock(&euac_internals->mutex);
}

