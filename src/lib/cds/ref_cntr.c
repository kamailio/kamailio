#include <cds/ref_cntr.h>
#include <cds/logger.h>
#include <cds/memory.h>

/* functions for initialization and destruction */

reference_counter_group_t *create_reference_counter_group(int mutex_cnt)
{
	reference_counter_group_t *g;
	int i;

	g = cds_malloc(sizeof(*g) + mutex_cnt * sizeof(cds_mutex_t));
	if (!g) {
		ERROR_LOG("can't allocate memory\n");
		return NULL;
	}

	for (i = 0; i < mutex_cnt; i++) {
		cds_mutex_init(&g->mutexes[i]);
	}
	g->mutex_to_assign = 0;
	g->mutex_cnt = mutex_cnt;

	return g;
}

void free_reference_counter_group(reference_counter_group_t *grp)
{
	int i;
	if (grp) {
		for (i = 0; i < grp->mutex_cnt; i++) {
			cds_mutex_destroy(&grp->mutexes[i]);
		}
		cds_free(grp);
	}
}

/* -------------------------------------------------------------------- */

void init_reference_counter(reference_counter_group_t *grp, reference_counter_data_t *ref)
{
	int m;
	if (ref && grp) {
		m = grp->mutex_to_assign;
		ref->cntr = 1;
		ref->mutex = grp->mutexes + m;
		m = (m + 1) % grp->mutex_cnt; /* can't be less than zero */
		grp->mutex_to_assign = m;
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

