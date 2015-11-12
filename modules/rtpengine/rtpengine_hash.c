#include "rtpengine_hash.h"

#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/shm_mem.h"
#include "../../locking.h"
#include "../../timer.h"

static gen_lock_t *rtpengine_hash_lock;
static struct rtpengine_hash_table *rtpengine_hash_table;
static int hash_table_size;

/* from sipwise rtpengine */
static int str_cmp_str(const str *a, const str *b) {
	if (a->len < b->len)
		return -1;
	if (a->len > b->len)
		return 1;
	if (a->len == 0 && b->len == 0)
		return 0;
	return memcmp(a->s, b->s, a->len);
}

/* from sipwise rtpengine */
static int str_equal(str *a, str *b) {
	return (str_cmp_str(a, b) == 0);
}

/* from sipwise rtpengine */
static unsigned int str_hash(str *s) {
	unsigned int ret = 5381;
	str it = *s;

	while (it.len > 0) {
		ret = (ret << 5) + ret + *it.s;
		it.s++;
		it.len--;
	}

	return ret % hash_table_size;
}

/* rtpengine glib hash API */
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

	// init lock
	rtpengine_hash_lock = lock_alloc();
	if (!rtpengine_hash_lock) {
		LM_ERR("no shm left to init rtpengine_hash_table lock");
		rtpengine_hash_table_destroy();
		return 0;
	}

	return 1;
}

int rtpengine_hash_table_destroy() {
	int i;
	struct rtpengine_hash_entry *entry, *last_entry;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return 0;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list");
		shm_free(rtpengine_hash_table);
		rtpengine_hash_table = NULL;
		return 0;
	}

	// destroy hashtable entry_list[i]
	lock_get(rtpengine_hash_lock);
	for (i = 0; i < hash_table_size; i++) {
		entry = rtpengine_hash_table->entry_list[i];
		while (entry) {
			last_entry = entry;
			entry = entry->next;
			shm_free(last_entry->callid.s);
			shm_free(last_entry);
		}
	}

	// destroy hashtable entry_list
	shm_free(rtpengine_hash_table->entry_list);
	rtpengine_hash_table->entry_list = NULL;

	// destroy hashtable
	shm_free(rtpengine_hash_table);
	rtpengine_hash_table = NULL;
	lock_release(rtpengine_hash_lock);

	// destroy lock
	if (!rtpengine_hash_lock) {
		LM_ERR("NULL rtpengine_hash_lock");
	} else {
		lock_dealloc(rtpengine_hash_lock);
		rtpengine_hash_lock = NULL;
	}

	return 1;
}

int rtpengine_hash_table_insert(str *key, struct rtpengine_hash_entry *value) {
	struct rtpengine_hash_entry *entry, *last_entry;
	struct rtpengine_hash_entry *new_entry = (struct rtpengine_hash_entry *) value;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return 0;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list");
		return 0;
	}

	// get entry list
	hash_index = str_hash(key);
	entry = rtpengine_hash_table->entry_list[hash_index];
	last_entry = entry;

	// lock
	lock_get(rtpengine_hash_lock);
	while (entry) {
		// if key found, don't add new entry
		if (str_equal(&entry->callid, &new_entry->callid)) {
			// unlock
			lock_release(rtpengine_hash_lock);
			LM_ERR("Call id %.*s already in hashtable, ignore new value", entry->callid.len, entry->callid.s);
			return 0;
		}

		// if expired entry discovered, delete it
		if (entry->tout < get_ticks()) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			shm_free(entry->callid.s);
			shm_free(entry);

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
	lock_release(rtpengine_hash_lock);

	return 1;
}

int rtpengine_hash_table_remove(str *key) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return 0;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list");
		return 0;
	}

	// get first entry from entry list; jump over unused list head
	hash_index = str_hash(key);
	entry = rtpengine_hash_table->entry_list[hash_index];
	last_entry = entry;

	// lock
	lock_get(rtpengine_hash_lock);
	while (entry) {
		// if key found, delete entry
		if (str_equal(&entry->callid, (str *)key)) {
			// free entry
			last_entry->next = entry->next;
			shm_free(entry->callid.s);
			shm_free(entry);

			// update total
			rtpengine_hash_table->total--;

			// unlock
			lock_release(rtpengine_hash_lock);

			return 1;
		}

		// if expired entry discovered, delete it
		if (entry->tout < get_ticks()) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			shm_free(entry->callid.s);
			shm_free(entry);

			// set pointers
			entry = last_entry;

			// update total
			rtpengine_hash_table->total--;
		}

		last_entry = entry;
		entry = entry->next;
	}

	// unlock
	lock_release(rtpengine_hash_lock);

	return 0;
}

struct rtpp_node *rtpengine_hash_table_lookup(str *key) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;
	struct rtpp_node *node;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return NULL;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list");
		return NULL;
	}

	// get first entry from entry list; jump over unused list head
	hash_index = str_hash(key);
	entry = rtpengine_hash_table->entry_list[hash_index];
	last_entry = entry;

	// lock
	lock_get(rtpengine_hash_lock);
	while (entry) {
		// if key found, return entry
		if (str_equal(&entry->callid, (str *)key)) {
			node = entry->node;

			// unlock
			lock_release(rtpengine_hash_lock);

			return node;
		}

		// if expired entry discovered, delete it
		if (entry->tout < get_ticks()) {
			// set pointers; exclude entry
			last_entry->next = entry->next;

			// free current entry; entry points to unknown
			shm_free(entry->callid.s);
			shm_free(entry);

			// set pointers
			entry = last_entry;

			// update total
			rtpengine_hash_table->total--;
		}

		last_entry = entry;
		entry = entry->next;
	}

	// unlock
	lock_release(rtpengine_hash_lock);

	return NULL;
}

// print hash table entries while deleting expired entries
void rtpengine_hash_table_print() {
	int i;
	struct rtpengine_hash_entry *entry, *last_entry;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return ;
	}

	// check rtpengine hashtable->entry_list
	if (!rtpengine_hash_table->entry_list) {
		LM_ERR("NULL rtpengine_hash_table->entry_list");
		return ;
	}

	// lock
	lock_get(rtpengine_hash_lock);

	// print hashtable
	for (i = 0; i < hash_table_size; i++) {
		entry = rtpengine_hash_table->entry_list[i];
		last_entry = entry;

		while (entry) {
			// if expired entry discovered, delete it
			if (entry->tout < get_ticks()) {
				// set pointers; exclude entry
				last_entry->next = entry->next;

				// free current entry; entry points to unknown
				shm_free(entry->callid.s);
				shm_free(entry);

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
	}

	// unlock
	lock_release(rtpengine_hash_lock);
}

unsigned int rtpengine_hash_table_total() {

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return 0;
	}

	return rtpengine_hash_table->total;
}
