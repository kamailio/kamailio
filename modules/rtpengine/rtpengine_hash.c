#include "rtpengine_hash.h"

#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../locking.h"
#include "../../timer.h"

static struct rtpengine_hash_table *rtpengine_hash_table;
static int hash_table_size;

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

	return ret % hash_table_size;
}

/* rtpengine hash API */
int rtpengine_hash_table_init(int size) {
	int i;

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

	// init hashtable entry_list
	rtpengine_hash_table->entry_list = shm_malloc(hash_table_size * sizeof(struct rtpengine_hash_entry));
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("no shm left to create rtpengine_hash_table->entry_list\n");
		rtpengine_hash_table_destroy();
		return 0;
	}
	memset(rtpengine_hash_table->entry_list, 0, hash_table_size * sizeof(struct rtpengine_hash_entry));

	// init hashtable entry_list[i] (head never filled); destroy table on error
	for (i = 0; i < hash_table_size; i++) {
		rtpengine_hash_table->entry_list[i] = shm_malloc(sizeof(struct rtpengine_hash_entry));
		if (!rtpengine_hash_table->entry_list[i]) {
			LM_ERR("no shm left to create rtpengine_hash_table->entry_list[%d]\n", i);
			rtpengine_hash_table_destroy();
			return 0;
		}
		memset(rtpengine_hash_table->entry_list[i], 0, sizeof(struct rtpengine_hash_entry));

		// never expire the head of the hashtable index lists
		rtpengine_hash_table->entry_list[i]->tout = -1;
		rtpengine_hash_table->entry_list[i]->next = NULL;
		rtpengine_hash_table->total = 0;
	}

	// init hashtable row_locks
	rtpengine_hash_table->row_locks = shm_malloc(hash_table_size * sizeof(gen_lock_t*));
	if (!rtpengine_hash_table->row_locks) {
		LM_ERR("no shm left to create rtpengine_hash_table->row_locks\n");
		rtpengine_hash_table_destroy();
		return 0;
	}

	// init hashtable row_locks[i]
	for (i = 0; i < hash_table_size; i++) {
		rtpengine_hash_table->row_locks[i] = lock_alloc();
		if (!rtpengine_hash_table->row_locks[i]) {
			LM_ERR("no shm left to create rtpengine_hash_table->row_locks[%d]\n", i);
			rtpengine_hash_table_destroy();
			return 0;
		}
	}

	return 1;
}

int rtpengine_hash_table_destroy() {
	int i;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return 0;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list\n");
		shm_free(rtpengine_hash_table);
		rtpengine_hash_table = NULL;
		return 0;
	}

	// destroy hashtable content
	for (i = 0; i < hash_table_size; i++) {
		// destroy hashtable entry_list[i]
		if (rtpengine_hash_table->row_locks[i]) {
			lock_get(rtpengine_hash_table->row_locks[i]);
		} else {
			LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", i);
			return 0;
		}
		rtpengine_hash_table_free_entry_list(rtpengine_hash_table->entry_list[i]);
		lock_release(rtpengine_hash_table->row_locks[i]);

		// destroy hashtable row_locks[i]
		rtpengine_hash_table_free_row_lock(rtpengine_hash_table->row_locks[i]);
		rtpengine_hash_table->row_locks[i] = NULL;
	}

	// destroy hashtable entry_list
	shm_free(rtpengine_hash_table->entry_list);
	rtpengine_hash_table->entry_list = NULL;

	// destroy hashtable row_locks
	shm_free(rtpengine_hash_table->row_locks);
	rtpengine_hash_table->row_locks = NULL;

	// destroy hashtable
	shm_free(rtpengine_hash_table);
	rtpengine_hash_table = NULL;

	return 1;
}

int rtpengine_hash_table_insert(str callid, str viabranch, struct rtpengine_hash_entry *value) {
	struct rtpengine_hash_entry *entry, *last_entry;
	struct rtpengine_hash_entry *new_entry = (struct rtpengine_hash_entry *) value;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return 0;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list\n");
		return 0;
	}

	// check rtpengine hashtable->row_locks
	if (!rtpengine_hash_table->row_locks) {
		LM_ERR("NULL rtpengine_hash_table->row_locks\n");
		return 0;
	}

	// get entry list
	hash_index = str_hash(callid);
	entry = rtpengine_hash_table->entry_list[hash_index];
	last_entry = entry;

	if (rtpengine_hash_table->row_locks[hash_index]) {
		lock_get(rtpengine_hash_table->row_locks[hash_index]);
	} else {
		LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", hash_index);
		return 0;
	}

	// lock
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
			rtpengine_hash_table->total--;
		}

		// next entry in the list
		last_entry = entry;
		entry = entry->next;
	}

	last_entry->next = new_entry;

	// update total
	rtpengine_hash_table->total++;

	// unlock
	lock_release(rtpengine_hash_table->row_locks[hash_index]);

	return 1;
}

int rtpengine_hash_table_remove(str callid, str viabranch) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return 0;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list\n");
		return 0;
	}

	// get first entry from entry list; jump over unused list head
	hash_index = str_hash(callid);
	entry = rtpengine_hash_table->entry_list[hash_index];
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
		if (str_equal(entry->callid, callid) &&
		    str_equal(entry->viabranch, viabranch)) {
			// free entry
			last_entry->next = entry->next;
			rtpengine_hash_table_free_entry(entry);

			// update total
			rtpengine_hash_table->total--;

			// unlock
			lock_release(rtpengine_hash_table->row_locks[hash_index]);

			return 1;
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
			rtpengine_hash_table->total--;
		}

		last_entry = entry;
		entry = entry->next;
	}

	// unlock
	lock_release(rtpengine_hash_table->row_locks[hash_index]);

	return 0;
}

struct rtpp_node *rtpengine_hash_table_lookup(str callid, str viabranch) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;
	struct rtpp_node *node;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return NULL;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list\n");
		return NULL;
	}

	// get first entry from entry list; jump over unused list head
	hash_index = str_hash(callid);
	entry = rtpengine_hash_table->entry_list[hash_index];
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
		if (str_equal(entry->callid, callid) &&
		    str_equal(entry->viabranch, viabranch)) {
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
			rtpengine_hash_table->total--;
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

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return ;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list\n");
		return ;
	}


	// print hashtable
	for (i = 0; i < hash_table_size; i++) {
		// lock
		if (rtpengine_hash_table->row_locks[i]) {
			lock_get(rtpengine_hash_table->row_locks[i]);
		} else {
			LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", i);
			return ;
		}

		entry = rtpengine_hash_table->entry_list[i];
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
				rtpengine_hash_table->total--;
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

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table\n");
		return 0;
	}

	return rtpengine_hash_table->total;
}

void rtpengine_hash_table_free_entry(struct rtpengine_hash_entry *entry) {
	if (!entry) {
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

void rtpengine_hash_table_free_entry_list(struct rtpengine_hash_entry *entry_list) {
	struct rtpengine_hash_entry *entry, *last_entry;

	if (!entry_list) {
		return ;
	}

	entry = entry_list;
	while (entry) {
		last_entry = entry;
		entry = entry->next;
		rtpengine_hash_table_free_entry(last_entry);
		last_entry = NULL;
	}

	return ;
}

void rtpengine_hash_table_free_row_lock(gen_lock_t *row_lock) {
	if (!row_lock) {
		return ;
	}

	lock_destroy(row_lock);

	return ;
}
