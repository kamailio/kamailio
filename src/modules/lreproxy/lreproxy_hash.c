/*
 * Copyright (C) 2003-2008 Sippy Software, Inc., http://www.sippysoft.com
 * Copyright (C) 2014-2015 Sipwise GmbH, http://www.sipwise.com
 * Copyright (C) 2020 Mojtaba Esfandiari.S, Nasim-Telecom
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */


#include "lreproxy.h"
#include "lreproxy_hash.h"

#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/timer.h"

static void lreproxy_hash_table_free_row_lock(gen_lock_t *row_lock);

static struct lreproxy_hash_table *lreproxy_hash_table;

/* get from sipwise rtpengine */
static int str_cmp_str(const str a, const str b) {
    if (a.len < b.len)
        return -1;
    if (a.len > b.len)
        return 1;
    if (a.len == 0 && b.len == 0)
        return 0;
    return memcmp(a.s, b.s, a.len);
}

/* get from sipwise rtpengine */
static int str_equal(str a, str b) {
    return (str_cmp_str(a, b) == 0);
}

/* get from sipwise rtpengine */
static unsigned int str_hash(str s) {
    unsigned int ret = 5381;
    str it = s;

    while (it.len > 0) {
        ret = (ret << 5) + ret + *it.s;
        it.s++;
        it.len--;
    }

    return ret % lreproxy_hash_table->size;
}

/* lreproxy hash API */
int lreproxy_hash_table_init(int size) {
    int i;
    int hash_table_size;


    hash_table_size = size;


//            LM_DBG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>lreproxy_hash_table size = %d\n", hash_table_size);
            LM_INFO(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>lreproxy_hash_table size = %d\n", hash_table_size);

    // init hashtable
    lreproxy_hash_table = shm_malloc(sizeof(struct lreproxy_hash_table));
    if (!lreproxy_hash_table) {
                LM_ERR("no shm left to create lreproxy_hash_table\n");
        return 0;
    }
    memset(lreproxy_hash_table, 0, sizeof(struct lreproxy_hash_table));
    lreproxy_hash_table->size = hash_table_size;

    // init hashtable row_locks
    lreproxy_hash_table->row_locks = shm_malloc(hash_table_size * sizeof(gen_lock_t*));
    if (!lreproxy_hash_table->row_locks) {
                LM_ERR("no shm left to create lreproxy_hash_table->row_locks\n");
        lreproxy_hash_table_destroy();
        return 0;
    }
    memset(lreproxy_hash_table->row_locks, 0, hash_table_size * sizeof(gen_lock_t*));

    // init hashtable row_entry_list
    lreproxy_hash_table->row_entry_list = shm_malloc(lreproxy_hash_table->size * sizeof(struct lreproxy_hash_entry*));
    if (!lreproxy_hash_table->row_entry_list) {
                LM_ERR("no shm left to create lreproxy_hash_table->row_entry_list\n");
        lreproxy_hash_table_destroy();
        return 0;
    }
    memset(lreproxy_hash_table->row_entry_list, 0, lreproxy_hash_table->size * sizeof(struct lreproxy_hash_entry*));

    // init hashtable row_totals
    lreproxy_hash_table->row_totals = shm_malloc(hash_table_size * sizeof(unsigned int));
    if (!lreproxy_hash_table->row_totals) {
                LM_ERR("no shm left to create lreproxy_hash_table->row_totals\n");
        lreproxy_hash_table_destroy();
        return 0;
    }
    memset(lreproxy_hash_table->row_totals, 0, hash_table_size * sizeof(unsigned int));

    // init hashtable  row_locks[i], row_entry_list[i] and row_totals[i]
    for (i = 0; i < hash_table_size; i++) {
        // alloc hashtable row_locks[i]
        lreproxy_hash_table->row_locks[i] = lock_alloc();
        if (!lreproxy_hash_table->row_locks[i]) {
                    LM_ERR("no shm left to create lreproxy_hash_table->row_locks[%d]\n", i);
            lreproxy_hash_table_destroy();
            return 0;
        }

        // init hashtable row_locks[i]
        if (!lock_init(lreproxy_hash_table->row_locks[i])) {
                    LM_ERR("fail to init lreproxy_hash_table->row_locks[%d]\n", i);
            lreproxy_hash_table_destroy();
            return 0;
        }

        // init hashtable row_entry_list[i]
        lreproxy_hash_table->row_entry_list[i] = shm_malloc(sizeof(struct lreproxy_hash_entry));
        if (!lreproxy_hash_table->row_entry_list[i]) {
                    LM_ERR("no shm left to create lreproxy_hash_table->row_entry_list[%d]\n", i);
            lreproxy_hash_table_destroy();
            return 0;
        }
        memset(lreproxy_hash_table->row_entry_list[i], 0, sizeof(struct lreproxy_hash_entry));

        lreproxy_hash_table->row_entry_list[i]->tout = -1;
        lreproxy_hash_table->row_entry_list[i]->next = NULL;

        // init hashtable row_totals[i]
        lreproxy_hash_table->row_totals[i] = 0;
    }

    return 1;
}

int lreproxy_hash_table_destroy() {
    int i;

    // check lreproxy hashtable
    if (!lreproxy_hash_table) {
                LM_ERR("NULL lreproxy_hash_table\n");
        return 1;
    }

    // check lreproxy hashtable->row_locks
    if (!lreproxy_hash_table->row_locks) {
                LM_ERR("NULL lreproxy_hash_table->row_locks\n");
        shm_free(lreproxy_hash_table);
        lreproxy_hash_table = NULL;
        return 1;
    }

    // destroy hashtable content
    for (i = 0; i < lreproxy_hash_table->size; i++) {
        // lock
        if (!lreproxy_hash_table->row_locks[i]) {
                    LM_ERR("NULL lreproxy_hash_table->row_locks[%d]\n", i);
            continue;
        } else {
            lock_get(lreproxy_hash_table->row_locks[i]);
        }

        // check lreproxy hashtable->row_entry_list
        if (!lreproxy_hash_table->row_entry_list) {
                    LM_ERR("NULL lreproxy_hash_table->row_entry_list\n");
        } else {
            // destroy hashtable row_entry_list[i]
            lreproxy_hash_table_free_row_entry_list(lreproxy_hash_table->row_entry_list[i]);
            lreproxy_hash_table->row_entry_list[i] = NULL;
        }

        // unlock
        lock_release(lreproxy_hash_table->row_locks[i]);

        // destroy hashtable row_locks[i]
        lreproxy_hash_table_free_row_lock(lreproxy_hash_table->row_locks[i]);
        lreproxy_hash_table->row_locks[i] = NULL;
    }

    // destroy hashtable row_entry_list
    if (!lreproxy_hash_table->row_entry_list) {
                LM_ERR("NULL lreproxy_hash_table->row_entry_list\n");
    } else {
        shm_free(lreproxy_hash_table->row_entry_list);
        lreproxy_hash_table->row_entry_list = NULL;
    }

    // destroy hashtable row_totals
    if (!lreproxy_hash_table->row_totals) {
                LM_ERR("NULL lreproxy_hash_table->row_totals\n");
    } else {
        shm_free(lreproxy_hash_table->row_totals);
        lreproxy_hash_table->row_totals = NULL;
    }

    // destroy hashtable row_locks
    if (!lreproxy_hash_table->row_locks) {
        // should not be the case; just for code symmetry
                LM_ERR("NULL lreproxy_hash_table->row_locks\n");
    } else {
        shm_free(lreproxy_hash_table->row_locks);
        lreproxy_hash_table->row_locks = NULL;
    }

    // destroy hashtable
    if (!lreproxy_hash_table) {
        // should not be the case; just for code symmetry
                LM_ERR("NULL lreproxy_hash_table\n");
    } else {
        shm_free(lreproxy_hash_table);
        lreproxy_hash_table = NULL;
    }

    return 1;
}


void lreproxy_hash_table_free_entry(struct lreproxy_hash_entry *entry) {
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

void lreproxy_hash_table_free_row_entry_list(struct lreproxy_hash_entry *row_entry_list) {
    struct lreproxy_hash_entry *entry, *last_entry;

    if (!row_entry_list) {
                LM_ERR("try to free a NULL row_entry_list\n");
        return ;
    }

    entry = row_entry_list;
    while (entry) {
        last_entry = entry;
        entry = entry->next;
        lreproxy_hash_table_free_entry(last_entry);
        last_entry = NULL;
    }

    return ;
}

int lreproxy_hash_table_insert(str callid, str viabranch, struct lreproxy_hash_entry *value) {
    struct lreproxy_hash_entry *entry, *last_entry;
    struct lreproxy_hash_entry *new_entry = (struct lreproxy_hash_entry *) value;
    unsigned int hash_index;

    // sanity checks
    if (!lreproxy_hash_table_sanity_checks()) {
                LM_ERR("sanity checks failed\n");
        return 0;
    }

    // get entry list
    hash_index = str_hash(callid);
    entry = lreproxy_hash_table->row_entry_list[hash_index];
    last_entry = entry;

    // lock
    if (lreproxy_hash_table->row_locks[hash_index]) {
        lock_get(lreproxy_hash_table->row_locks[hash_index]);
    } else {
                LM_ERR("NULL rtpengine_hash_table->row_locks[%d]\n", hash_index);
        return 0;
    }

    while (entry) {
        // if found, don't add new entry
        if (str_equal(entry->callid, new_entry->callid) &&
            str_equal(entry->viabranch, new_entry->viabranch)) {
            // unlock
            lock_release(lreproxy_hash_table->row_locks[hash_index]);
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
            lreproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lreproxy_hash_table->row_totals[hash_index]--;
        }

        // next entry in the list
        last_entry = entry;
        entry = entry->next;
    }

    last_entry->next = new_entry;

    // update total
    lreproxy_hash_table->row_totals[hash_index]++;

    // unlock
    lock_release(lreproxy_hash_table->row_locks[hash_index]);

    return 1;
}

int lreproxy_hash_table_remove(str callid, str viabranch, enum lre_operation op) {
    struct lreproxy_hash_entry *entry, *last_entry;
    unsigned int hash_index;
    int found = 0;

    // sanity checks
    if (!lreproxy_hash_table_sanity_checks()) {
                LM_ERR("sanity checks failed\n");
        return 0;
    }

    // get first entry from entry list; jump over unused list head
    hash_index = str_hash(callid);
    entry = lreproxy_hash_table->row_entry_list[hash_index];
    last_entry = entry;

    // lock
    if (lreproxy_hash_table->row_locks[hash_index]) {
        lock_get(lreproxy_hash_table->row_locks[hash_index]);
    } else {
                LM_ERR("NULL lreproxy_hash_table->row_locks[%d]\n", hash_index);
        return 0;
    }

    while (entry) {
        // if callid found, delete entry
        if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
            (str_equal(entry->callid, callid) && viabranch.len == 0 && op == OP_DELETE)) {
            // set pointers; exclude entry
            last_entry->next = entry->next;

            // free current entry; entry points to unknown
            lreproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lreproxy_hash_table->row_totals[hash_index]--;

            found = 1;

            if (!(viabranch.len == 0 && op == OP_DELETE)) {
                // unlock
                lock_release(lreproxy_hash_table->row_locks[hash_index]);
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
            lreproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lreproxy_hash_table->row_totals[hash_index]--;
        }

        last_entry = entry;
        entry = entry->next;
    }

    // unlock
    lock_release(lreproxy_hash_table->row_locks[hash_index]);

    return found;
}
//struct lrep_node *lreproxy_hash_table_lookup(str callid, str viabranch, enum lre_operation op) {
//struct lreproxy_hash_entry *lreproxy_hash_table_lookup(str callid, str viabranch, enum lre_operation op) {
struct lreproxy_hash_entry *lreproxy_hash_table_lookup(str callid, str viabranch) {
    struct lreproxy_hash_entry *entry, *last_entry;
    unsigned int hash_index;
//    struct lrep_node *node;

    // sanity checks
    if (!lreproxy_hash_table_sanity_checks()) {
                LM_ERR("sanity checks failed\n");
        return 0;
    }

    // get first entry from entry list; jump over unused list head
    hash_index = str_hash(callid);
    entry = lreproxy_hash_table->row_entry_list[hash_index];
    last_entry = entry;

    // lock
    if (lreproxy_hash_table->row_locks[hash_index]) {
        lock_get(lreproxy_hash_table->row_locks[hash_index]);
    } else {
                LM_ERR("NULL lreproxy_hash_table->row_locks[%d]\n", hash_index);
        return 0;
    }

    while (entry) {
        // if callid found, return entry
//        if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
//            (str_equal(entry->callid, callid) && viabranch.len == 0 && op == OP_DELETE)) {
        if (str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) {
//            node = entry->node;
            // unlock
            lock_release(lreproxy_hash_table->row_locks[hash_index]);
            return entry;
//            return node;
        }

        // if expired entry discovered, delete it
        if (entry->tout < get_ticks()) {
            // set pointers; exclude entry
            last_entry->next = entry->next;

            // free current entry; entry points to unknown
            lreproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lreproxy_hash_table->row_totals[hash_index]--;
        }

        last_entry = entry;
        entry = entry->next;
    }

    // unlock
    lock_release(lreproxy_hash_table->row_locks[hash_index]);

    return NULL;
}


static void lreproxy_hash_table_free_row_lock(gen_lock_t *row_lock) {
    if (!row_lock) {
                LM_ERR("try to free a NULL lock\n");
        return ;
    }

    lock_destroy(row_lock);
    lock_dealloc(row_lock);

    return ;
}

int lreproxy_hash_table_sanity_checks() {
    // check lreproxy hashtable
    if (!lreproxy_hash_table) {
                LM_ERR("NULL lreproxy_hash_table\n");
        return 0;
    }

    // check rtpengine hashtable->row_locks
    if (!lreproxy_hash_table->row_locks) {
                LM_ERR("NULL lreproxy_hash_table->row_locks\n");
        return 0;
    }

    // check rtpengine hashtable->row_entry_list
    if (!lreproxy_hash_table->row_entry_list) {
                LM_ERR("NULL lreproxy_hash_table->row_entry_list\n");
        return 0;
    }

    // check rtpengine hashtable->row_totals
    if (!lreproxy_hash_table->row_totals) {
                LM_ERR("NULL lreproxy_hash_table->row_totals\n");
        return 0;
    }

    return 1;
}

