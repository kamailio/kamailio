#include "rtpengine.h"
#include "rtpengine_hash.h"

#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/timer.h"

static void rtpengine_hash_table_free_row_lock(gen_lock_t *row_lock);


static struct rtpengine_hash_table *rtpengine_hash_table;

/* from sipwise rtpengine */
static int str_cmp_str(const str a, const str b) {
	if (a.len < b.len)
		return -1;
	if (a.len > b.len)
		return 1;
	if (a.len == 0 && b.len == 0)
		return 0;
	return memcmp(a.s, b.s, a.len);
}

/* from sipwise rtpengine */
static int str_equal(str a, str b) {
	return (str_cmp_str(a, b) == 0);
}

/* from sipwise rtpengine */
static unsigned int str_hash(str s) {
	unsigned int ret = 5381;
	str it = s;

	while (it.len > 0) {
		ret = (ret << 5) + ret + *it.s;
		it.s++;
		it.len--;
	}

	return ret % rtpengine_hash_table->size;
}

/* rtpengine hash API */
int rtpengine_hash_table_init(int size) {
	int i;
	int hash_table_size;

	// init hash table size
	if (size < 1) {
		hash_table_size = 1;
	} else {
		hash_table_size = size;
	}
	LM_DBG("rtpengine_hash_table size = %d\n", hash_table_size);

	// init hashtable
	rtpengine_hash_table = shm_malloc(sizeof(struct rtpengine_hash_table));
	if (!rtpengine_hash_table) {
		LM_ERR("no shm left to create rtpengine_hash_table\n");
		return 0;
	}
	memset(rtpengine_hash_table, 0, sizeof(struct rtpengine_hash_table));
	rtpengine_hash_table->size = hash_table_size;

	// init hashtable row_locks
	rtpengine_hash_table->row_locks = shm_malloc(hash_table_size * sizeof(gen_lock_t*));
	if (!rtpengine_hash_table->row_locks) {
		LM_ERR("no shm left to create rtpengine_hash_table->row_locks\n");
		rtpengine_hash_table_destroy();
		return 0;
	}
	memset(rtpengine_hash_table->row_locks, 0, hash_table_size * sizeof(gen_lock_t*));

	// init hashtable row_entry_list
	rtpengine_hash_table->row_entry_list = shm_malloc(rtpengine_hash_table->size * sizeof(struct rtpengine_hash_entry*));
	if (!rtpengine_hash_table->row_entry_list) {
		LM_ERR("no shm left to create rtpengine_hash_table->row_entry_list\n");
		rtpengine_hash_table_destroy();
		return 0;
	}
	memset(rtpengine_hash_table->row_entry_list, 0, rtpengine_hash_table->size * sizeof(struct rtpengine_hash_entry*));

	// init hashtable row_totals
	rtpengine_hash_table->row_totals = shm_malloc(hash_table_size * sizeof(unsigned int));
	if (!rtpengine_hash_table->row_totals) {
		LM_ERR("no shm left to create rtpengine_hash_table->row_totals\n");
		rtpengine_hash_table_destroy();
		return 0;
	}
	memset(rtpengine_hash_table->row_totals, 0, hash_table_size * sizeof(unsigned int));

	// init hashtable  row_locks[i], row_entry_list[i] and row_totals[i]
	for (i = 0; i < hash_table_size; i++) {
		// alloc hashtable row_locks[i]
		rtpengine_hash_table->row_locks[i] = lock_alloc();
		if (!rtpengine_hash_table->row_locks[i]) {
			LM_ERR("no shm left to create rtpengine_hash_table->row_locks[%d]\n", i);
			rtpengine_hash_table_destroy();
			return 0;
		}

		// init hashtable row_locks[i]
		if (!lock_init(rtpengine_hash_table->row_locks[i])) {
			LM_ERR("fail to init rtpengine_hash_table->row_locks[%d]\n", i);
			rtpengine_hash_table_destroy();
			return 0;
		}

		// init hashtable row_entry_list[i]
		rtpengine_hash_table->row_entry_list[i] = shm_malloc(sizeof(struct rtpengine_hash_entry));
		if (!rtpengine_hash_table->row_entry_list[i]) {
			LM_ERR("no shm left to create rtpengine_hash_table->row_entry_list[%d]\n", i);
			rtpengine_hash_table_destroy();
			return 0;
		}
		memset(rtpengine_hash_table->row_entry_list[i], 0, sizeof(struct rtpengine_hash_entry));

		rtpengine_hash_table->row_entry_list[i]->tout = -1;
		rtpengine_hash_table->row_entry_list[i]->next = NULL;

		// init hashtable row_totals[i]
		rtpengine_hash_table->row_totals[i] = 0;
	}

	return 1;
}

int rtpengine_hash_table_destroy() {
	int i;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return 1;
	}

	// check rtpengine hashtable->row_locks
	if (!rtpengine_hash_table->row_locks) {
		LM_ERR("NULL rtpengine_hash_table->row_locks\n");
		shm_free(rtpengine_hash_table);
		rtpengine_hash_table = NULL;
		return 1;
	}

	// destroy hashtable content
	for (i = 0; i < rtpengine_hash_table->size; i++) {
		// lock
		if (!rtpengine_hash_table->row_locks[i]) {
			LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", i);
			continue;
		} else {
			lock_get(rtpengine_hash_table->row_locks[i]);
		}

		// check rtpengine hashtable->row_entry_list
		if (!rtpengine_hash_table->row_entry_list) {
			LM_ERR("NULL rtpengine_hash_table->row_entry_list\n");
		} else {
			// destroy hashtable row_entry_list[i]
			rtpengine_hash_table_free_row_entry_list(rtpengine_hash_table->row_entry_list[i]);
			rtpengine_hash_table->row_entry_list[i] = NULL;
		}

		// unlock
		lock_release(rtpengine_hash_table->row_locks[i]);

		// destroy hashtable row_locks[i]
		rtpengine_hash_table_free_row_lock(rtpengine_hash_table->row_locks[i]);
		rtpengine_hash_table->row_locks[i] = NULL;
	}

	// destroy hashtable row_entry_list
	if (!rtpengine_hash_table->row_entry_list) {
		LM_ERR("NULL rtpengine_hash_table->row_entry_list\n");
	} else {
		shm_free(rtpengine_hash_table->row_entry_list);
		rtpengine_hash_table->row_entry_list = NULL;
	}

	// destroy hashtable row_totals
	if (!rtpengine_hash_table->row_totals) {
		LM_ERR("NULL rtpengine_hash_table->row_totals\n");
	} else {
		shm_free(rtpengine_hash_table->row_totals);
		rtpengine_hash_table->row_totals = NULL;
	}

	// destroy hashtable row_locks
	if (!rtpengine_hash_table->row_locks) {
		// should not be the case; just for code symmetry
		LM_ERR("NULL rtpengine_hash_table->row_locks\n");
	} else {
		shm_free(rtpengine_hash_table->row_locks);
		rtpengine_hash_table->row_locks = NULL;
	}

	// destroy hashtable
	if (!rtpengine_hash_table) {
		// should not be the case; just for code symmetry
		LM_ERR("NULL rtpengine_hash_table\n");
	} else {
		shm_free(rtpengine_hash_table);
		rtpengine_hash_table = NULL;
	}

	return 1;
}

int rtpengine_hash_table_insert(str callid, str viabranch, struct rtpengine_hash_entry *value) {
	struct rtpengine_hash_entry *entry, *last_entry;
	struct rtpengine_hash_entry *new_entry = (struct rtpengine_hash_entry *) value;
	unsigned int hash_index;

	// sanity checks
	if (!rtpengine_hash_table_sanity_checks()) {
		LM_ERR("sanity checks failed\n");
		return 0;
	}

	// get entry list
	hash_index = str_hash(callid);
	entry = rtpengine_hash_table->row_entry_list[hash_index];

	if (entry==NULL || rtpengine_hash_table->row_locks[hash_index]==NULL) {
		LM_ERR("NULL entry or lock for hash table slot[%d]\n", hash_index);
		return 0;
	}

	last_entry = entry;
	// lock
	lock_get(rtpengine_hash_table->row_locks[hash_index]);

	while (entry) {
		// if found, don't add new entry
		if (str_equal(entry->callid, new_entry->callid) &&
		    str_equal(entry->viabranch, new_entry->viabranch)) {
			// unlock
			lock_release(rtpengine_hash_table->row_locks[hash_index]);
			LM_NOTICE("callid=%.*s, viabranch=%.*s already in hashtable, ignore new value\n",
				entry->callid.len, entry->callid.s,
				entry->viabranch.len, entry->viabranch.s);
			return 0;
		}

		// if expired entry discovered, delete it
		if (entry->tout < get_ticks()) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			rtpengine_hash_table_free_entry(entry);

			// set pointers
			entry = last_entry;

			// update total
			rtpengine_hash_table->row_totals[hash_index]--;
		}

		// next entry in the list
		last_entry = entry;
		entry = entry->next;
	}

	last_entry->next = new_entry;

	// update total
	rtpengine_hash_table->row_totals[hash_index]++;

	// unlock
	lock_release(rtpengine_hash_table->row_locks[hash_index]);

	return 1;
}

int rtpengine_hash_table_remove(str callid, str viabranch, enum rtpe_operation op) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;
	int found = 0;

	// sanity checks
	if (!rtpengine_hash_table_sanity_checks()) {
		LM_ERR("sanity checks failed\n");
		return 0;
	}

	// get first entry from entry list; jump over unused list head
	hash_index = str_hash(callid);
	entry = rtpengine_hash_table->row_entry_list[hash_index];
	last_entry = entry;

	// lock
	if (rtpengine_hash_table->row_locks[hash_index]) {
		lock_get(rtpengine_hash_table->row_locks[hash_index]);
	} else {
		LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", hash_index);
		return 0;
	}

	while (entry) {
		// if callid found, delete entry
		if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
		    (str_equal(entry->callid, callid) && viabranch.len == 0 && op == OP_DELETE)) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			rtpengine_hash_table_free_entry(entry);

			// set pointers
			entry = last_entry;

			// update total
			rtpengine_hash_table->row_totals[hash_index]--;

			found = 1;

			if (!(viabranch.len == 0 && op == OP_DELETE)) {
				// unlock
				lock_release(rtpengine_hash_table->row_locks[hash_index]);
				return found;
			}

			// try to also delete other viabranch entries for callid
			last_entry = entry;
			entry = entry->next;
			continue;
		}

		// if expired entry discovered, delete it
		if (entry->tout < get_ticks()) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			rtpengine_hash_table_free_entry(entry);

			// set pointers
			entry = last_entry;

			// update total
			rtpengine_hash_table->row_totals[hash_index]--;
		}

		last_entry = entry;
		entry = entry->next;
	}

	// unlock
	lock_release(rtpengine_hash_table->row_locks[hash_index]);

	return found;
}

struct rtpp_node *rtpengine_hash_table_lookup(str callid, str viabranch, enum rtpe_operation op) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;
	struct rtpp_node *node;

	// sanity checks
	if (!rtpengine_hash_table_sanity_checks()) {
		LM_ERR("sanity checks failed\n");
		return 0;
	}

	// get first entry from entry list; jump over unused list head
	hash_index = str_hash(callid);
	entry = rtpengine_hash_table->row_entry_list[hash_index];
	last_entry = entry;

	// lock
	if (rtpengine_hash_table->row_locks[hash_index]) {
		lock_get(rtpengine_hash_table->row_locks[hash_index]);
	} else {
		LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", hash_index);
		return 0;
	}

	while (entry) {
		// if callid found, return entry
		if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
		    (str_equal(entry->callid, callid) && viabranch.len == 0 && op == OP_DELETE)) {
			node = entry->node;

			// unlock
			lock_release(rtpengine_hash_table->row_locks[hash_index]);

			return node;
		}

		// if expired entry discovered, delete it
		if (entry->tout < get_ticks()) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			rtpengine_hash_table_free_entry(entry);

			// set pointers
			entry = last_entry;

			// update total
			rtpengine_hash_table->row_totals[hash_index]--;
		}

		last_entry = entry;
		entry = entry->next;
	}

	// unlock
	lock_release(rtpengine_hash_table->row_locks[hash_index]);

	return NULL;
}

// print hash table entries while deleting expired entries
void rtpengine_hash_table_print() {
	int i;
	struct rtpengine_hash_entry *entry, *last_entry;

	// sanity checks
	if (!rtpengine_hash_table_sanity_checks()) {
		LM_ERR("sanity checks failed\n");
		return ;
	}

	// print hashtable
	for (i = 0; i < rtpengine_hash_table->size; i++) {
		// lock
		if (rtpengine_hash_table->row_locks[i]) {
			lock_get(rtpengine_hash_table->row_locks[i]);
		} else {
			LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", i);
			return ;
		}

		entry = rtpengine_hash_table->row_entry_list[i];
		last_entry = entry;

		while (entry) {
			// if expired entry discovered, delete it
			if (entry->tout < get_ticks()) {
				// set pointers; exclude entry
				last_entry->next = entry->next;

				// free current entry; entry points to unknown
				rtpengine_hash_table_free_entry(entry);

				// set pointers
				entry = last_entry;

				// update total
				rtpengine_hash_table->row_totals[i]--;
			} else {
				LM_DBG("hash_index=%d callid=%.*s tout=%u\n",
					i, entry->callid.len, entry->callid.s, entry->tout - get_ticks());
			}

			last_entry = entry;
			entry = entry->next;
		}

		// unlock
		lock_release(rtpengine_hash_table->row_locks[i]);
	}

}

unsigned int rtpengine_hash_table_total() {
	int i;
	unsigned int total = 0;

	// sanity checks
	if (!rtpengine_hash_table_sanity_checks()) {
		LM_ERR("sanity checks failed\n");
		return 0;
	}

	for (i = 0; i < rtpengine_hash_table->size; i++) {
		total += rtpengine_hash_table->row_totals[i];
	}

	return total;
}

void rtpengine_hash_table_free_entry(struct rtpengine_hash_entry *entry) {
	if (!entry) {
		LM_ERR("try to free a NULL entry\n");
		return ;
	}

	// free callid
	if (entry->callid.s) {
		shm_free(entry->callid.s);
	}

	// free viabranch
	if (entry->viabranch.s) {
		shm_free(entry->viabranch.s);
	}

	// free entry
	shm_free(entry);

	return ;
}

void rtpengine_hash_table_free_row_entry_list(struct rtpengine_hash_entry *row_entry_list) {
	struct rtpengine_hash_entry *entry, *last_entry;

	if (!row_entry_list) {
		LM_ERR("try to free a NULL row_entry_list\n");
		return ;
	}

	entry = row_entry_list;
	while (entry) {
		last_entry = entry;
		entry = entry->next;
		rtpengine_hash_table_free_entry(last_entry);
		last_entry = NULL;
	}

	return ;
}

static void rtpengine_hash_table_free_row_lock(gen_lock_t *row_lock) {
	if (!row_lock) {
		LM_ERR("try to free a NULL lock\n");
		return ;
	}

	lock_destroy(row_lock);
	lock_dealloc(row_lock);

	return ;
}

int rtpengine_hash_table_sanity_checks() {
	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return 0;
	}

	// check rtpengine hashtable->row_locks
	if (!rtpengine_hash_table->row_locks) {
		LM_ERR("NULL rtpengine_hash_table->row_locks\n");
		return 0;
	}

	// check rtpengine hashtable->row_entry_list
	if (!rtpengine_hash_table->row_entry_list) {
		LM_ERR("NULL rtpengine_hash_table->row_entry_list\n");
		return 0;
	}

	// check rtpengine hashtable->row_totals
	if (!rtpengine_hash_table->row_totals) {
		LM_ERR("NULL rtpengine_hash_table->row_totals\n");
		return 0;
	}

	return 1;
}
