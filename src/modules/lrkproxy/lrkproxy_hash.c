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


#include "lrkproxy.h"
#include "lrkproxy_hash.h"

#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/mem/shm_mem.h"
#include "../../core/timer.h"

static void lrkproxy_hash_table_free_row_lock(gen_lock_t *row_lock);

static struct lrkproxy_hash_table *lrkproxy_hash_table;

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

    return ret % lrkproxy_hash_table->size;
}

/* lrkproxy hash API */
int lrkproxy_hash_table_init(int size) {
    int i;
    int hash_table_size;


    hash_table_size = size;


//            LM_DBG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>lrkproxy_hash_table size = %d\n", hash_table_size);
            LM_INFO(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>lrkproxy_hash_table size = %d\n", hash_table_size);

    // init hashtable
    lrkproxy_hash_table = shm_malloc(sizeof(struct lrkproxy_hash_table));
    if (!lrkproxy_hash_table) {
                LM_ERR("no shm left to create lrkproxy_hash_table\n");
        return 0;
    }
    memset(lrkproxy_hash_table, 0, sizeof(struct lrkproxy_hash_table));
    lrkproxy_hash_table->size = hash_table_size;

    // init hashtable row_locks
    lrkproxy_hash_table->row_locks = shm_malloc(hash_table_size * sizeof(gen_lock_t*));
    if (!lrkproxy_hash_table->row_locks) {
                LM_ERR("no shm left to create lrkproxy_hash_table->row_locks\n");
        lrkproxy_hash_table_destroy();
        return 0;
    }
    memset(lrkproxy_hash_table->row_locks, 0, hash_table_size * sizeof(gen_lock_t*));

    // init hashtable row_entry_list
    lrkproxy_hash_table->row_entry_list = shm_malloc(lrkproxy_hash_table->size * sizeof(struct lrkproxy_hash_entry*));
    if (!lrkproxy_hash_table->row_entry_list) {
                LM_ERR("no shm left to create lrkproxy_hash_table->row_entry_list\n");
        lrkproxy_hash_table_destroy();
        return 0;
    }
    memset(lrkproxy_hash_table->row_entry_list, 0, lrkproxy_hash_table->size * sizeof(struct lrkproxy_hash_entry*));

    // init hashtable row_totals
    lrkproxy_hash_table->row_totals = shm_malloc(hash_table_size * sizeof(unsigned int));
    if (!lrkproxy_hash_table->row_totals) {
                LM_ERR("no shm left to create lrkproxy_hash_table->row_totals\n");
        lrkproxy_hash_table_destroy();
        return 0;
    }
    memset(lrkproxy_hash_table->row_totals, 0, hash_table_size * sizeof(unsigned int));

    // init hashtable  row_locks[i], row_entry_list[i] and row_totals[i]
    for (i = 0; i < hash_table_size; i++) {
        // alloc hashtable row_locks[i]
        lrkproxy_hash_table->row_locks[i] = lock_alloc();
        if (!lrkproxy_hash_table->row_locks[i]) {
                    LM_ERR("no shm left to create lrkproxy_hash_table->row_locks[%d]\n", i);
            lrkproxy_hash_table_destroy();
            return 0;
        }

        // init hashtable row_locks[i]
        if (!lock_init(lrkproxy_hash_table->row_locks[i])) {
                    LM_ERR("fail to init lrkproxy_hash_table->row_locks[%d]\n", i);
            lrkproxy_hash_table_destroy();
            return 0;
        }

        // init hashtable row_entry_list[i]
        lrkproxy_hash_table->row_entry_list[i] = shm_malloc(sizeof(struct lrkproxy_hash_entry));
        if (!lrkproxy_hash_table->row_entry_list[i]) {
                    LM_ERR("no shm left to create lrkproxy_hash_table->row_entry_list[%d]\n", i);
            lrkproxy_hash_table_destroy();
            return 0;
        }
        memset(lrkproxy_hash_table->row_entry_list[i], 0, sizeof(struct lrkproxy_hash_entry));

        lrkproxy_hash_table->row_entry_list[i]->tout = -1;
        lrkproxy_hash_table->row_entry_list[i]->next = NULL;

        // init hashtable row_totals[i]
        lrkproxy_hash_table->row_totals[i] = 0;
    }

    return 1;
}

int lrkproxy_hash_table_destroy() {
    int i;

    // check lrkproxy hashtable
    if (!lrkproxy_hash_table) {
                LM_ERR("NULL lrkproxy_hash_table\n");
        return 1;
    }

    // check lrkproxy hashtable->row_locks
    if (!lrkproxy_hash_table->row_locks) {
                LM_ERR("NULL lrkproxy_hash_table->row_locks\n");
        shm_free(lrkproxy_hash_table);
        lrkproxy_hash_table = NULL;
        return 1;
    }

    // destroy hashtable content
    for (i = 0; i < lrkproxy_hash_table->size; i++) {
        // lock
        if (!lrkproxy_hash_table->row_locks[i]) {
                    LM_ERR("NULL lrkproxy_hash_table->row_locks[%d]\n", i);
            continue;
        } else {
            lock_get(lrkproxy_hash_table->row_locks[i]);
        }

        // check lrkproxy hashtable->row_entry_list
        if (!lrkproxy_hash_table->row_entry_list) {
                    LM_ERR("NULL lrkproxy_hash_table->row_entry_list\n");
        } else {
            // destroy hashtable row_entry_list[i]
            lrkproxy_hash_table_free_row_entry_list(lrkproxy_hash_table->row_entry_list[i]);
            lrkproxy_hash_table->row_entry_list[i] = NULL;
        }

        // unlock
        lock_release(lrkproxy_hash_table->row_locks[i]);

        // destroy hashtable row_locks[i]
        lrkproxy_hash_table_free_row_lock(lrkproxy_hash_table->row_locks[i]);
        lrkproxy_hash_table->row_locks[i] = NULL;
    }

    // destroy hashtable row_entry_list
    if (!lrkproxy_hash_table->row_entry_list) {
                LM_ERR("NULL lrkproxy_hash_table->row_entry_list\n");
    } else {
        shm_free(lrkproxy_hash_table->row_entry_list);
        lrkproxy_hash_table->row_entry_list = NULL;
    }

    // destroy hashtable row_totals
    if (!lrkproxy_hash_table->row_totals) {
                LM_ERR("NULL lrkproxy_hash_table->row_totals\n");
    } else {
        shm_free(lrkproxy_hash_table->row_totals);
        lrkproxy_hash_table->row_totals = NULL;
    }

    // destroy hashtable row_locks
    if (!lrkproxy_hash_table->row_locks) {
        // should not be the case; just for code symmetry
                LM_ERR("NULL lrkproxy_hash_table->row_locks\n");
    } else {
        shm_free(lrkproxy_hash_table->row_locks);
        lrkproxy_hash_table->row_locks = NULL;
    }

    // destroy hashtable
    if (!lrkproxy_hash_table) {
        // should not be the case; just for code symmetry
                LM_ERR("NULL lrkproxy_hash_table\n");
    } else {
        shm_free(lrkproxy_hash_table);
        lrkproxy_hash_table = NULL;
    }

    return 1;
}


void lrkproxy_hash_table_free_entry(struct lrkproxy_hash_entry *entry) {
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

void lrkproxy_hash_table_free_row_entry_list(struct lrkproxy_hash_entry *row_entry_list) {
    struct lrkproxy_hash_entry *entry, *last_entry;

    if (!row_entry_list) {
                LM_ERR("try to free a NULL row_entry_list\n");
        return ;
    }

    entry = row_entry_list;
    while (entry) {
        last_entry = entry;
        entry = entry->next;
        lrkproxy_hash_table_free_entry(last_entry);
        last_entry = NULL;
    }

    return ;
}

int lrkproxy_hash_table_insert(str callid, str viabranch, struct lrkproxy_hash_entry *value) {
    struct lrkproxy_hash_entry *entry, *last_entry;
    struct lrkproxy_hash_entry *new_entry = (struct lrkproxy_hash_entry *) value;
    unsigned int hash_index;

    // sanity checks
    if (!lrkproxy_hash_table_sanity_checks()) {
                LM_ERR("sanity checks failed\n");
        return 0;
    }

    // get entry list
    hash_index = str_hash(callid);
    entry = lrkproxy_hash_table->row_entry_list[hash_index];
    last_entry = entry;

    // lock
    if (lrkproxy_hash_table->row_locks[hash_index]) {
        lock_get(lrkproxy_hash_table->row_locks[hash_index]);
    } else {
                LM_ERR("NULL lrkproxy_hash_table->row_locks[%d]\n", hash_index);
        return 0;
    }

    while (entry) {
        // if found, don't add new entry
        if (str_equal(entry->callid, new_entry->callid) &&
            str_equal(entry->viabranch, new_entry->viabranch)) {
            // unlock
            lock_release(lrkproxy_hash_table->row_locks[hash_index]);
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
            lrkproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lrkproxy_hash_table->row_totals[hash_index]--;
        }

        // next entry in the list
        last_entry = entry;
        entry = entry->next;
    }

    last_entry->next = new_entry;

    // update total
    lrkproxy_hash_table->row_totals[hash_index]++;

    // unlock
    lock_release(lrkproxy_hash_table->row_locks[hash_index]);

    return 1;
}

int lrkproxy_hash_table_remove(str callid, str viabranch, enum lrk_operation op) {
    struct lrkproxy_hash_entry *entry, *last_entry;
    unsigned int hash_index;
    int found = 0;

    // sanity checks
    if (!lrkproxy_hash_table_sanity_checks()) {
                LM_ERR("sanity checks failed\n");
        return 0;
    }

    // get first entry from entry list; jump over unused list head
    hash_index = str_hash(callid);
    entry = lrkproxy_hash_table->row_entry_list[hash_index];
    last_entry = entry;

    if (!entry)
                LM_INFO("============>entry is null\n");
    else
                LM_INFO("============>entry is not null\n");


    // lock
    if (lrkproxy_hash_table->row_locks[hash_index]) {
        lock_get(lrkproxy_hash_table->row_locks[hash_index]);
    } else {
                LM_ERR("NULL lrkproxy_hash_table->row_locks[%d]\n", hash_index);
        return 0;
    }

    while (entry) {
                LM_INFO("remove============>current_callid=%.*s, entry_callid=%.*s\n",
                        callid.len, callid.s,
                        entry->callid.len, entry->callid.s);

                LM_INFO("remove============>current_viabranch=%.*s, entry_viabranch=%.*s\n",
                        viabranch.len, viabranch.s,
                        entry->viabranch.len, entry->viabranch.s);
        // if callid found, delete entry
        if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
            (str_equal(entry->callid, callid) && viabranch.len == 0 && op == OP_DELETE) ||
            str_equal(entry->callid, callid)){
//            if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
//                (str_equal(entry->callid, callid) && viabranch.len == 0 && op == OP_DELETE)) {
            // set pointers; exclude entry

            // set pointers; exclude entry
            last_entry->next = entry->next;

            // free current entry; entry points to unknown
            lrkproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lrkproxy_hash_table->row_totals[hash_index]--;

            found = 1;

            if (!(viabranch.len == 0 && op == OP_DELETE)) {
                // unlock
                lock_release(lrkproxy_hash_table->row_locks[hash_index]);
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
            lrkproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lrkproxy_hash_table->row_totals[hash_index]--;
        }

        last_entry = entry;
        entry = entry->next;
    }

    // unlock
    lock_release(lrkproxy_hash_table->row_locks[hash_index]);

    return found;
}
//struct lrkp_node *lrkproxy_hash_table_lookup(str callid, str viabranch, enum lrk_operation op) {
//struct lrkproxy_hash_entry *lrkproxy_hash_table_lookup(str callid, str viabranch, enum lrk_operation op) {
struct lrkproxy_hash_entry *lrkproxy_hash_table_lookup(str callid, str viabranch) {
    struct lrkproxy_hash_entry *entry=NULL;
    struct lrkproxy_hash_entry *last_entry = NULL;
    unsigned int hash_index;
//    struct lrkp_node *node;

    // sanity checks
    if (!lrkproxy_hash_table_sanity_checks()) {
                LM_ERR("sanity checks failed\n");
        return 0;
    }

    // get first entry from entry list; jump over unused list head
    hash_index = str_hash(callid);
    entry = lrkproxy_hash_table->row_entry_list[hash_index];
    last_entry = entry;


    // lock
    if (lrkproxy_hash_table->row_locks[hash_index]) {
        lock_get(lrkproxy_hash_table->row_locks[hash_index]);
    } else {
                LM_ERR("NULL lrkproxy_hash_table->row_locks[%d]\n", hash_index);
        return 0;
    }


    while (entry) {

        // if callid found, return entry
        if ((str_equal(entry->callid, callid) && str_equal(entry->viabranch, viabranch)) ||
            (str_equal(entry->callid, callid) && viabranch.len == 0) ||
            str_equal(entry->callid, callid)){
//            node = entry->node;
            // unlock

            lock_release(lrkproxy_hash_table->row_locks[hash_index]);

            return entry;
//            return node;
        }

        // if expired entry discovered, delete it
        if (entry->tout < get_ticks()) {
            // set pointers; exclude entry
            last_entry->next = entry->next;

            // free current entry; entry points to unknown
            lrkproxy_hash_table_free_entry(entry);

            // set pointers
            entry = last_entry;

            // update total
            lrkproxy_hash_table->row_totals[hash_index]--;
        }

        last_entry = entry;
        entry = entry->next;
    }

    // unlock
    lock_release(lrkproxy_hash_table->row_locks[hash_index]);

    return NULL;
}


static void lrkproxy_hash_table_free_row_lock(gen_lock_t *row_lock) {
    if (!row_lock) {
                LM_ERR("try to free a NULL lock\n");
        return ;
    }

    lock_destroy(row_lock);
    lock_dealloc(row_lock);

    return ;
}

int lrkproxy_hash_table_sanity_checks() {
    // check lrkproxy hashtable
    if (!lrkproxy_hash_table) {
                LM_ERR("NULL lrkproxy_hash_table\n");
        return 0;
    }

    // check lrkproxy hashtable->row_locks
    if (!lrkproxy_hash_table->row_locks) {
                LM_ERR("NULL lrkproxy_hash_table->row_locks\n");
        return 0;
    }

    // check lrkproxy hashtable->row_entry_list
    if (!lrkproxy_hash_table->row_entry_list) {
                LM_ERR("NULL lrkproxy_hash_table->row_entry_list\n");
        return 0;
    }

    // check lrkproxy hashtable->row_totals
    if (!lrkproxy_hash_table->row_totals) {
                LM_ERR("NULL lrkproxy_hash_table->row_totals\n");
        return 0;
    }

    return 1;
}

