/* 
 * Copyright (C) 2010 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @brief Kamailio core :: counters/stats
 * @file
 * @ingroup: core
 */

#include "counters.h"
#include "str_hash.h"
#include "str.h"
#include "compiler_opt.h"
#include "mem/mem.h"
#include "mem/shm_mem.h"


#define CNT_HASH_SIZE	64
/* group hash size (rpc use) */
#define GRP_HASH_SIZE	16
/* initial sorted groups array size (rpc use) */
#define GRP_SORTED_SIZE	16
/* intial counter id 2 record array size */
#define CNT_ID2RECORD_SIZE	64

#define CACHELINE_PAD 128



/* leave space for one flag */
#define MAX_COUNTER_ID 32767
/* size (number of entries) of the temporary array used for keeping stats
   pre-prefork init.  Note: if more counters are registered then this size,
   the array will be dynamically increased (doubled each time). The value
   here is meant only to optimize startup/memory fragmentation. */
#define PREINIT_CNTS_VALS_SIZE 128

struct counter_record {
	str group;
	str name;
	counter_handle_t h;
	unsigned short flags;
	void* cbk_param;
	counter_cbk_f cbk;
	struct counter_record* grp_next; /* next in group */
	str doc;
};


struct grp_record {
	str group;
	struct counter_record* first;
};



/** hash table mapping a counter name to an id */
static struct str_hash_table cnts_hash_table;
/** array maping id 2 record */
struct counter_record** cnt_id2record;
static int cnt_id2record_size;
/** hash table for groups (maps a group name to a counter list) */
static struct str_hash_table grp_hash_table;
/** array of groups, sorted */
static struct grp_record** grp_sorted;
static int grp_sorted_max_size;
static int grp_sorted_crt_size;
static int grp_no; /* number of groups */

/** counters array. a[proc_no][counter_id] =>
  _cnst_vals[proc_no*cnts_no+counter_id] */
counter_array_t* _cnts_vals = 0;
int _cnts_row_len; /* number of elements per row */
static int cnts_no; /* number of registered counters */
static int cnts_max_rows; /* set to 0 if not yet fully init */


int counters_initialized(void)
{
	if (unlikely(_cnts_vals == 0)) {
		/* not init yet */
		return 0;
	}
	return 1;
}

/** init the coutner hash table(s).
 * @return 0 on success, -1 on error.
 */
int init_counters()
{
	if (str_hash_alloc(&cnts_hash_table, CNT_HASH_SIZE) < 0)
		goto error;
	str_hash_init(&cnts_hash_table);
	if (str_hash_alloc(&grp_hash_table, GRP_HASH_SIZE) < 0)
		goto error;
	str_hash_init(&grp_hash_table);
	cnts_no = 1; /* start at 1 (0 used only for invalid counters) */
	cnts_max_rows = 0; /* 0 initially, !=0 after full init
						  (counters_prefork_init()) */
	grp_no = 0;
	cnt_id2record_size = CNT_ID2RECORD_SIZE;
	cnt_id2record = pkg_malloc(sizeof(*cnt_id2record) * cnt_id2record_size);
	if (cnt_id2record == 0)
		goto error;
	memset(cnt_id2record, 0, sizeof(*cnt_id2record) * cnt_id2record_size);
	grp_sorted_max_size = GRP_SORTED_SIZE;
	grp_sorted_crt_size = 0;
	grp_sorted = pkg_malloc(sizeof(*grp_sorted) * grp_sorted_max_size);
	if (grp_sorted == 0)
		goto error;
	memset(grp_sorted, 0, sizeof(*grp_sorted) * grp_sorted_max_size);
	return 0;
error:
	destroy_counters();
	return -1;
}



void destroy_counters()
{
	int r;
	struct str_hash_entry* e;
	struct str_hash_entry* bak;
	if (_cnts_vals) {
		if (cnts_max_rows)
			/* fully init => it is in shm */
			shm_free(_cnts_vals);
		else
			/* partially init (before prefork) => pkg */
			pkg_free(_cnts_vals);
		_cnts_vals = 0;
	}
	if (cnts_hash_table.table) {
		for (r=0; r< cnts_hash_table.size; r++) {
			clist_foreach_safe(&cnts_hash_table.table[r], e, bak, next) {
				pkg_free(e);
			}
		}
		pkg_free(cnts_hash_table.table);
	}
	if (grp_hash_table.table) {
		for (r=0; r< grp_hash_table.size; r++) {
			clist_foreach_safe(&grp_hash_table.table[r], e, bak, next) {
				pkg_free(e);
			}
		}
		pkg_free(grp_hash_table.table);
	}
	if (cnt_id2record)
		pkg_free(cnt_id2record);
	if (grp_sorted)
		pkg_free(grp_sorted);
	cnts_hash_table.table = 0;
	cnts_hash_table.size = 0;
	cnt_id2record = 0;
	grp_sorted = 0;
	grp_hash_table.table = 0;
	grp_hash_table.size = 0;
	grp_sorted_crt_size = 0;
	grp_sorted_max_size = 0;
	cnts_no = 0;
	_cnts_row_len = 0;
	cnts_max_rows = 0;
	grp_no = 0;
}



/** complete counter intialization, when the number of processes is known.
 * shm must be available.
 * @return 0 on success, < 0 on error
 */
int counters_prefork_init(int max_process_no)
{
	counter_array_t* old;
	int size, row_size;
	counter_handle_t h;
	/* round cnts_no so that cnts_no * sizeof(counter) it's a CACHELINE_PAD
	   multiple */
	/* round-up row_size to a CACHELINE_PAD multiple  if needed */
	row_size = ((sizeof(*_cnts_vals) * cnts_no - 1) / CACHELINE_PAD + 1) *
		CACHELINE_PAD;
	/* round-up the resulted row_siue to a sizeof(*_cnts_vals) multiple */
	row_size = ((row_size -1) / sizeof(*_cnts_vals) + 1) *
				sizeof(*_cnts_vals);
	/* get updated cnts_no (row length) */
	_cnts_row_len = row_size / sizeof(*_cnts_vals);
	size = max_process_no * row_size;
	/* replace the temporary pre-fork pkg array (with only 1 row) with
	   the final shm version (with max_process_no rows) */
	old = _cnts_vals;
	_cnts_vals = shm_malloc(size);
	if (_cnts_vals == 0)
		return -1;
	memset(_cnts_vals, 0, size);
	cnts_max_rows = max_process_no;
	/* copy prefork values into the newly shm array */
	if (old) {
		for (h.id = 0; h.id < cnts_no; h.id++)
			counter_pprocess_val(process_no, h) = old[h.id].v;
		pkg_free(old);
	}
	return 0;
}



/** adds new group to the group hash table (no checks, internal version).
 * @return pointer to new group record on success, 0 on error.
 */
static struct grp_record* grp_hash_add(str* group)
{
	struct str_hash_entry* g;
	struct grp_record* grp_rec;
	struct grp_record** r;

	/* grp_rec copied at &g->u.data */
	g = pkg_malloc(sizeof(struct str_hash_entry) - sizeof(g->u.data) +
					sizeof(*grp_rec) + group->len + 1);
	if (g == 0)
		goto error;
	grp_rec = (struct grp_record*)&g->u.data[0];
	grp_rec->group.s = (char*)(grp_rec + 1);
	grp_rec->group.len = group->len;
	grp_rec->first = 0;
	memcpy(grp_rec->group.s, group->s, group->len + 1);
	g->key = grp_rec->group;
	g->flags = 0;
	/* insert group into the sorted group array */
	if (grp_sorted_max_size <= grp_sorted_crt_size) {
		/* must increase the array */
		r = pkg_realloc(grp_sorted, 2 * grp_sorted_max_size *
						sizeof(*grp_sorted));
		if (r == 0)
			goto error;
		grp_sorted= r;
		grp_sorted_max_size *= 2;
		memset(&grp_sorted[grp_sorted_crt_size], 0,
				(grp_sorted_max_size - grp_sorted_crt_size) *
				sizeof(*grp_sorted));
	}
	for (r = grp_sorted; r < (grp_sorted + grp_sorted_crt_size); r++)
		if (strcmp(grp_rec->group.s, (*r)->group.s) < 0)
			break;
	if (r != (grp_sorted + grp_sorted_crt_size))
		memmove(r+1, r, (int)(long)((char*)(grp_sorted + grp_sorted_crt_size) -
						(char*)r));
	grp_sorted_crt_size++;
	*r = grp_rec;
	/* insert into the hash only on success */
	str_hash_add(&grp_hash_table, g);
	return grp_rec;
error:
	if (g)
		pkg_free(g);
	return 0;
}



/** lookup a group into the group hash (internal version).
 * @return pointer to grp_record on success, 0 on failure (not found).
 */
static struct grp_record* grp_hash_lookup(str* group)
{
	struct str_hash_entry* e;
	e = str_hash_get(&grp_hash_table, group->s, group->len);
	return e?(struct grp_record*)&e->u.data[0]:0;
}



/** lookup a group and if not found create a new group record.
 * @return pointer to grp_record on succes, 0 on failure ( not found and
 *  failed to create new group record).
 */
static struct grp_record* grp_hash_get_create(str* group)
{
	struct grp_record* ret;

	ret = grp_hash_lookup(group);
	if (ret)
		return ret;
	return grp_hash_add(group);
}



/** adds new counter to the hash table (no checks, internal version).
 * @return pointer to new record on success, 0 on error.
 */
static struct counter_record* cnt_hash_add(
							str* group, str* name,
							int flags, counter_cbk_f cbk,
							void* param, const char* doc)
{
	struct str_hash_entry* e;
	struct counter_record* cnt_rec;
	struct grp_record* grp_rec;
	struct counter_record** p;
	counter_array_t* v;
	int doc_len;
	int n;
	
	e = 0;
	if (cnts_no >= MAX_COUNTER_ID)
		/* too many counters */
		goto error;
	grp_rec = grp_hash_get_create(group);
	if (grp_rec == 0)
		/* non existing group an no new one could be created */
		goto error;
	doc_len = doc?strlen(doc):0;
	/* cnt_rec copied at &e->u.data[0] */
	e = pkg_malloc(sizeof(struct str_hash_entry) - sizeof(e->u.data) +
					sizeof(*cnt_rec) + name->len + 1 + group->len + 1 +
					doc_len + 1);
	if (e == 0)
		goto error;
	cnt_rec = (struct counter_record*)&e->u.data[0];
	cnt_rec->group.s = (char*)(cnt_rec + 1);
	cnt_rec->group.len = group->len;
	cnt_rec->name.s = cnt_rec->group.s + group->len + 1;
	cnt_rec->name.len = name->len;
	cnt_rec->doc.s = cnt_rec->name.s + name->len +1;
	cnt_rec->doc.len = doc_len;
	cnt_rec->h.id = cnts_no++;
	cnt_rec->flags = flags;
	cnt_rec->cbk_param = param;
	cnt_rec->cbk = cbk;
	cnt_rec->grp_next = 0;
	memcpy(cnt_rec->group.s, group->s, group->len + 1);
	memcpy(cnt_rec->name.s, name->s, name->len + 1);
	if (doc)
		memcpy(cnt_rec->doc.s, doc, doc_len + 1);
	else
		cnt_rec->doc.s[0] = 0;
	e->key = cnt_rec->name;
	e->flags = 0;
	/* check to see if it fits in the prefork tmp. vals array.
	   This array contains only one "row", is allocated in pkg and
	   is used only until counters_prefork_init() (after that the
	   array is replaced with a shm version with all the needed rows).
	 */
	if (cnt_rec->h.id >= _cnts_row_len || _cnts_vals == 0) {
		/* array to small or not yet allocated => reallocate/allocate it
		   (min size PREINIT_CNTS_VALS_SIZE, max MAX_COUNTER_ID)
		 */
		n = (cnt_rec->h.id < PREINIT_CNTS_VALS_SIZE) ?
				PREINIT_CNTS_VALS_SIZE :
				((2 * (cnt_rec->h.id + (cnt_rec->h.id == 0)) < MAX_COUNTER_ID)?
					(2 * (cnt_rec->h.id + (cnt_rec->h.id == 0))) :
					MAX_COUNTER_ID + 1);
		v = pkg_realloc(_cnts_vals, n * sizeof(*_cnts_vals));
		if (v == 0)
			/* realloc/malloc error */
			goto error;
		_cnts_vals = v;
		/* zero newly allocated memory */
		memset(&_cnts_vals[_cnts_row_len], 0,
				(n - _cnts_row_len) * sizeof(*_cnts_vals));
		_cnts_row_len = n; /* record new length */
	}
	/* add a pointer to it in the records array */
	if (cnt_id2record_size <= cnt_rec->h.id) {
		/* must increase the array */
		p = pkg_realloc(cnt_id2record,
						2 * cnt_id2record_size * sizeof(*cnt_id2record));
		if (p == 0)
			goto error;
		cnt_id2record = p;
		cnt_id2record_size *= 2;
		memset(&cnt_id2record[cnt_rec->h.id], 0,
				(cnt_id2record_size - cnt_rec->h.id) * sizeof(*cnt_id2record));
	}
	cnt_id2record[cnt_rec->h.id] = cnt_rec;
	/* add into the hash */
	str_hash_add(&cnts_hash_table, e);
	/* insert it sorted in the per group list */
	for (p = &grp_rec->first; *p; p = &((*p)->grp_next))
		if (strcmp(cnt_rec->name.s, (*p)->name.s) < 0)
			break;
	cnt_rec->grp_next = *p;
	*p = cnt_rec;
	return cnt_rec;
error:
	if (e)
		pkg_free(e);
	return 0;
}



/** lookup a (group, name) pair into the cnts hash (internal version).
 * @param group - counter group name. If "" the first matching counter with
 *                the given name will be returned (k compat).
 * @param name
 * @return pointer to counter_record on success, 0 on failure (not found).
 */
static struct counter_record* cnt_hash_lookup(str* group, str* name)
{
	struct str_hash_entry* e;
	struct str_hash_entry* first;
	struct counter_record* cnt_rec;
	e = str_hash_get(&cnts_hash_table, name->s, name->len);
	/* fast path */
	if (likely(e)) {
		cnt_rec = (struct counter_record*)&e->u.data[0];
		if (likely( group->len == 0 ||
				(cnt_rec->group.len == group->len &&
				memcmp(cnt_rec->group.s, group->s, group->len) == 0)))
		return cnt_rec;
	} else
		return 0;
	/* search between records with same name, but different groups */
	first = e;
	do {
		cnt_rec = (struct counter_record*)&e->u.data[0];
		if (cnt_rec->group.len == group->len &&
			cnt_rec->name.len  == name->len &&
			memcmp(cnt_rec->group.s, group->s, group->len) == 0 &&
			memcmp(cnt_rec->name.s, name->s, name->len) == 0)
			/* found */
			return cnt_rec;
		e = e->next;
	} while(e != first);
	return 0;
}



/** lookup a counter and if not found create a new counter record.
 * @return pointer to counter_record on succes, 0 on failure ( not found and
 *  failed to create new group record).
 */
static struct counter_record* cnt_hash_get_create(
								str* group, str* name,
								int flags,
								counter_cbk_f cbk,
								void* param, const char* doc)
{
	struct counter_record* ret;

	ret = cnt_hash_lookup(group, name);
	if (ret)
		return ret;
	return cnt_hash_add(group, name, flags, cbk, param, doc);
}



/** register a new counter.
 * Can be called only before forking (e.g. from mod_init() or
 * init_child(PROC_INIT)).
 * @param handle - result parameter, it will be filled with the counter
 *                  handle on success (can be null if not needed).
 * @param group - group name
 * @param name  - counter name (group.name must be unique).
 * @param flags  - counter flags: one of CNT_F_*.
 * @param cbk   - read callback function (if set it will be called each time
 *                  someone will call counter_get()).
 * @param cbk_param - callback param.
 * @param doc       - description/documentation string.
 * @param reg_flags - register flags: 1 - don't fail if counter already
 *                    registered (act like counter_lookup(handle, group, name).
 * @return 0 on succes, < 0 on error (-1 not init or malloc error, -2 already
 *         registered (and register_flags & 1 == 0).
 */
int counter_register(	counter_handle_t* handle, const char* group,
						const char* name, int flags,
						counter_cbk_f cbk, void* cbk_param,
						const char* doc,
						int reg_flags)
{
	str grp;
	str n;
	struct counter_record* cnt_rec;

	if (unlikely(cnts_max_rows)) {
		/* too late */
		BUG("late attempt to register counter: %s.%s\n", group, name);
		goto error;
	}
	n.s = (char*)name;
	n.len = strlen(name);
	if (unlikely(group == 0 || *group == 0)) {
		BUG("attempt to register counter %s without a group\n", name);
		goto error;
	}
	grp.s = (char*)group;
	grp.len = strlen(group);
	cnt_rec = cnt_hash_lookup(&grp, &n);
	if (cnt_rec) {
		if (reg_flags & 1)
			goto found;
		else {
			if (handle) handle->id = 0;
			return -2;
		}
	} else
		cnt_rec = cnt_hash_get_create(&grp, &n, flags, cbk, cbk_param, doc);
	if (unlikely(cnt_rec == 0))
		goto error;
found:
	if (handle) *handle = cnt_rec->h;
	return 0;
error:
	if (handle) handle->id = 0;
	return -1;
}



/** fill in the handle of an existing counter (str parameters).
  * @param handle - filled with the corresp. handle on success.
  * @param group - counter group name. If "" the first matching
  *                counter with the given name will be returned
  *                (k compat).
  * @param name - counter name.
 * @return 0 on success, < 0 on error
 */
int counter_lookup_str(counter_handle_t* handle, str* group, str* name)
{
	struct counter_record* cnt_rec;

	cnt_rec = cnt_hash_lookup(group, name);
	if (likely(cnt_rec)) {
		*handle = cnt_rec->h;
		return 0;
	}
	handle->id = 0;
	return -1;
}



/** fill in the handle of an existing counter (asciiz parameters).
  * @param handle - filled with the corresp. handle on success.
  * @param group - counter group name. If 0 or "" the first matching
  *                counter with the given name will be returned
  *                (k compat).
  * @param name - counter name.
 * @return 0 on success, < 0 on error
 */
int counter_lookup(counter_handle_t* handle,
					const char* group, const char* name)
{
	str grp;
	str n;

	n.s = (char*)name;
	n.len = strlen(name);
	grp.s = (char*)group;
	grp.len = group?strlen(group):0;
	return counter_lookup_str(handle, &grp, &n);
}



/** register all the counters declared in a null-terminated array.
  * @param group - counters group.
  * @param defs  - null terminated array containing counters definitions.
  * @return 0 on success, < 0 on error ( - (counter_number+1))
  */
int counter_register_array(const char* group, counter_def_t* defs)
{
	int r;
	
	for (r=0; defs[r].name; r++)
		if (counter_register(	defs[r].handle,
								group, defs[r].name, defs[r].flags,
								defs[r].get_cbk, defs[r].get_cbk_param,
								defs[r].descr, 0) <0)
			return -(r+1); /* return - (idx of bad counter + 1) */
	return 0;
}



/** get the value of the counter, bypassing callbacks.
 * @param handle - counter handle obtained using counter_lookup() or
 *                 counter_register().
 * @return counter value.
 */
counter_val_t counter_get_raw_val(counter_handle_t handle)
{
	int r;
	counter_val_t ret;

	if (unlikely(_cnts_vals == 0)) {
		/* not init yet */
		BUG("counters not fully initialized yet\n");
		return 0;
	}
	if (unlikely(handle.id >= cnts_no || (short)handle.id < 0)) {
		BUG("invalid counter id %d (max %d)\n", handle.id, cnts_no - 1);
		return 0;
	}
	ret = 0;
	for (r = 0; r < cnts_max_rows; r++)
		ret += counter_pprocess_val(r, handle);
	return ret;
}



/** get the value of the counter, using the callbacks (if defined).
 * @param handle - counter handle obtained using counter_lookup() or
 *                 counter_register().
 * @return counter value. */
counter_val_t counter_get_val(counter_handle_t handle)
{
	struct counter_record* cnt_rec;

	if (unlikely(_cnts_vals == 0 || cnt_id2record == 0)) {
		/* not init yet */
		BUG("counters not fully initialized yet\n");
		return 0;
	}
	cnt_rec = cnt_id2record[handle.id];
	if (unlikely(cnt_rec->cbk))
		return cnt_rec->cbk(handle, cnt_rec->cbk_param);
	return counter_get_raw_val(handle);
}



/** reset the  counter.
 * Reset a counter, unless it has the CNT_F_NO_RESET flag set.
 * @param handle - counter handle obtained using counter_lookup() or
 *                 counter_register().
 * Note: it's racy.
 */
void counter_reset(counter_handle_t handle)
{
	int r;

	if (unlikely(_cnts_vals == 0 || cnt_id2record == 0)) {
		/* not init yet */
		BUG("counters not fully initialized yet\n");
		return;
	}
	if (unlikely(handle.id >= cnts_no)) {
		BUG("invalid counter id %d (max %d)\n", handle.id, cnts_no - 1);
		return;
	}
	if (unlikely(cnt_id2record[handle.id]->flags & CNT_F_NO_RESET))
		return;
	for (r=0; r < cnts_max_rows; r++)
		counter_pprocess_val(r, handle) = 0;
	return;
}



/** return the name for counter handle.
 * @param handle - counter handle obtained using counter_lookup() or
 *                 counter_register().
 * @return asciiz pointer on success, 0 on error.
 */
char* counter_get_name(counter_handle_t handle)
{
	if (unlikely(_cnts_vals == 0 || cnt_id2record == 0)) {
		/* not init yet */
		BUG("counters not fully initialized yet\n");
		goto error;
	}
	if (unlikely(handle.id >= cnts_no)) {
		BUG("invalid counter id %d (max %d)\n", handle.id, cnts_no - 1);
		goto error;
	}
	return cnt_id2record[handle.id]->name.s;
error:
	return 0;
}



/** return the group name for counter handle.
 * @param handle - counter handle obtained using counter_lookup() or
 *                 counter_register().
 * @return asciiz pointer on success, 0 on error.
 */
char* counter_get_group(counter_handle_t handle)
{
	if (unlikely(_cnts_vals == 0 || cnt_id2record == 0)) {
		/* not init yet */
		BUG("counters not fully initialized yet\n");
		goto error;
	}
	if (unlikely(handle.id >= cnts_no)) {
		BUG("invalid counter id %d (max %d)\n", handle.id, cnts_no - 1);
		goto error;
	}
	return cnt_id2record[handle.id]->group.s;
error:
	return 0;
}



/** return the description (doc) string for a given counter.
 * @param handle - counter handle obtained using counter_lookup() or
 *                 counter_register().
 * @return asciiz pointer on success, 0 on error.
 */
char* counter_get_doc(counter_handle_t handle)
{
	if (unlikely(_cnts_vals == 0 || cnt_id2record == 0)) {
		/* not init yet */
		BUG("counters not fully initialized yet\n");
		goto error;
	}
	if (unlikely(handle.id >= cnts_no)) {
		BUG("invalid counter id %d (max %d)\n", handle.id, cnts_no - 1);
		goto error;
	}
	return cnt_id2record[handle.id]->doc.s;
error:
	return 0;
}



/** iterate on all the counter group names.
 * @param cbk - pointer to a callback function that will be called for each
 *              group name.
 * @param p   - parameter that will be passed to the callback function
 *              (along the group name).
 */
void counter_iterate_grp_names(void (*cbk)(void* p, str* grp_name), void* p)
{
	int r;

	for (r=0; r < grp_sorted_crt_size; r++)
		cbk(p, &grp_sorted[r]->group);
}



/** iterate on all the variable names in a specified group.
 * @param group - group name.
 * @param cbk - pointer to a callback function that will be called for each
 *              variable name.
 * @param p   - parameter that will be passed to the callback function
 *              (along the variable name).
 */
void counter_iterate_grp_var_names(	const char* group,
									void (*cbk)(void* p, str* var_name),
									void* p)
{
	struct counter_record* r;
	struct grp_record* g;
	str grp;
	
	grp.s = (char*)group;
	grp.len = strlen(group);
	g = grp_hash_lookup(&grp);
	if (g)
		for (r = g->first; r; r = r->grp_next)
			cbk(p, &r->name);
}



/** iterate on all the variable names and handles in a specified group.
 * @param group - group name.
 * @param cbk - pointer to a callback function that will be called for each
 *              [variable name, variable handle] pair.
 * @param p   - parameter that will be passed to the callback function
 *              (along the group name, variable name and variable handle).
 */
void counter_iterate_grp_vars(const char* group,
							  void (*cbk)(void* p, str* g, str* n,
								  			counter_handle_t h),
							  void *p)
{
	struct counter_record* r;
	struct grp_record* g;
	str grp;
	
	grp.s = (char*)group;
	grp.len = strlen(group);
	g = grp_hash_lookup(&grp);
	if (g)
		for (r = g->first; r; r = r->grp_next)
			cbk(p, &r->group, &r->name, r->h);
}

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
