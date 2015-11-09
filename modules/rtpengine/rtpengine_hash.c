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
static int str_equal(void *a, void *b) {
	return (str_cmp_str((str *) a, (str *) b) == 0);
}

/* from sipwise rtpengine */
static unsigned int str_hash(void *ss) {
	const str *s = (str*) ss;
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

	// init hashtable entry_list[i] (head never filled)
	for (i = 0; i < hash_table_size; i++) {
		rtpengine_hash_table->entry_list[i] = shm_malloc(sizeof(struct rtpengine_hash_entry));
		if (!rtpengine_hash_table->entry_list[i]) {
			LM_ERR("no shm left to create rtpengine_hash_table->entry_list[%d]\n", i);
			return 0;
		}

		// never expire the head of the hashtable index lists
		rtpengine_hash_table->entry_list[i]->tout = -1;
		rtpengine_hash_table->entry_list[i]->next = NULL;
	}

	// init lock
	rtpengine_hash_lock = lock_alloc();
	if (!rtpengine_hash_lock) {
		LM_ERR("no shm left to init rtpengine_hash_table lock");
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

int rtpengine_hash_table_insert(void *key, void *value) {
	struct rtpengine_hash_entry *entry, *last_entry;
	struct rtpengine_hash_entry *new_entry = (struct rtpengine_hash_entry *) value;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
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
		}

		// next entry in the list
		last_entry = entry;
		entry = entry->next;
	}

	last_entry->next = new_entry;

	// unlock
	lock_release(rtpengine_hash_lock);

	return 1;
}

int rtpengine_hash_table_remove(void *key) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
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
		}

		last_entry = entry;
		entry = entry->next;
	}

	// unlock
	lock_release(rtpengine_hash_lock);

	return 0;
}

void* rtpengine_hash_table_lookup(void *key) {
	struct rtpengine_hash_entry *entry, *last_entry;
	unsigned int hash_index;

	// check rtpengine hashtable
	if (!rtpengine_hash_table) {
		LM_ERR("NULL rtpengine_hash_table");
		return 0;
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
			// unlock
			lock_release(rtpengine_hash_lock);

			return entry;
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
