#include <cds/ref_cntr.h>
#include <cds/logger.h>
#include <cds/memory.h>

/* One global mutex for reference counting may be enough. 
 * If problems try to create pool of precreated mutexes
 * and use them randomly.
 */
static cds_mutex_t *ref_cntr_mutex = NULL;

/* global functions for initialization and destruction */

int reference_counter_initialize()
{
	if (!ref_cntr_mutex) {
		ref_cntr_mutex = (cds_mutex_t*)cds_malloc(sizeof(cds_mutex_t));
		if (ref_cntr_mutex) {
			cds_mutex_init(ref_cntr_mutex);
			return 0;
		}
	}
	return -1;
}

void reference_counter_cleanup()
{
	if (ref_cntr_mutex) {
		cds_mutex_destroy(ref_cntr_mutex);
		cds_free((void*)ref_cntr_mutex);
		ref_cntr_mutex = NULL;
	}
}

/* -------------------------------------------------------------------- */

void init_reference_counter(reference_counter_data_t *ref)
{
	if (ref) {
		ref->cntr = 1;
		ref->mutex = ref_cntr_mutex;
	}
}

void add_reference(reference_counter_data_t *ref)
{
	if (ref) {
		if (ref->mutex) cds_mutex_lock(ref->mutex);
		ref->cntr++;
		if (ref->mutex) cds_mutex_unlock(ref->mutex);
	}
}

int get_reference_count(reference_counter_data_t *ref)
{
	int res = 0;
	if (ref) {
		if (ref->mutex) cds_mutex_lock(ref->mutex);
		res = ref->cntr;
		if (ref->mutex) cds_mutex_unlock(ref->mutex);
	}
	return res;
}

int remove_reference(reference_counter_data_t *ref)
{
	int res = 0;
	if (ref) {
		if (ref->mutex) cds_mutex_lock(ref->mutex);
		if (ref->cntr > 0) ref->cntr--;
		if (ref->cntr == 0) res = 1;
		if (ref->mutex) cds_mutex_unlock(ref->mutex);
	}
	return res;
}

