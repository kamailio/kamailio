/* sp-ul_db module
 *
 * Copyright (C) 2007 1&1 Internet AG
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
 */

#include "../../timer.h"
#include "../../timer_proc.h"
#include "../../sr_module.h"
#include "ul_db_watch.h"
#include "ul_db.h"
#include "p_usrloc_mod.h"

typedef struct ul_db_watch_list {
	int id;
	int active;
	struct ul_db_watch_list * next;
} ul_db_watch_list_t;

ul_db_watch_list_t ** list = NULL;

gen_lock_t * list_lock;

static ul_db_handle_list_t * handles;

void check_dbs(unsigned int ticks, void *param);

static int init_watch_db_list(void);

int ul_db_watch_init(void){
	if(init_watch_db_list() < 0){
		return -1;
	}
	if((list = shm_malloc(sizeof(ul_db_watch_list_t *))) == NULL){
		LM_ERR("couldn't allocate shared memory.\n");
		return -1;
	}
	*list = NULL;
	return 0;
}

int init_db_check(void){
	int ret;
	if(db_master_write){
		LM_INFO("start timer, interval %i seconds\n", retry_interval);
		ret = fork_dummy_timer(PROC_TIMER, "TIMER UL WATCH", 1, check_dbs, NULL, retry_interval);
	} else {
		ret = 0;
	}
	return ret;
}

void ul_db_watch_destroy(void){
	ul_db_watch_list_t * del;
	ul_db_handle_list_t * del2;
	if(list_lock){
		lock_destroy(list_lock);
		lock_dealloc(list_lock);
		list_lock = NULL;
	}
	if(list){
		while(list && *list){
			del = *list;
			*list = (*list)->next;
			shm_free(del);
		}
		shm_free(list);
		list = NULL;
	}
	while(handles){
		del2 = handles;
		handles = handles->next;
		pkg_free(del2);
	}
	return;
}

void check_dbs(unsigned int ticks, void *param){
	LM_DBG("check availability of databases");
	ul_db_watch_list_t * tmp;
	ul_db_handle_list_t * tmp2, * new_element;
	int found;
	int i;
	
	if(!list_lock){
		return;
	}
	lock_get(list_lock);	
	tmp = *list;
	while(tmp){
		tmp2 = handles;
		found = 0;
		while(tmp2){
			if(tmp2->handle->id == tmp->id){
				found = 1;
				if(tmp->active){
					LM_INFO("handle %i found, check it\n", tmp->id);
					tmp2->handle->active = 1;
					ul_db_check(tmp2->handle);
				} else if (tmp2->handle->active) {
					for(i=0; i<DB_NUM; i++){
						if(tmp2->handle->db[i].dbh){
							tmp2->handle->db[i].dbf.close(tmp2->handle->db[i].dbh);
							tmp2->handle->db[i].dbh = NULL;
						}
					}
					tmp2->handle->active = 0;
				}
			}
			tmp2 = tmp2->next;
		}
		if(!found){
			LM_NOTICE("handle %i not found, create it\n", tmp->id);
			if((new_element = pkg_malloc(sizeof(ul_db_handle_list_t))) == NULL){
				LM_ERR("couldn't allocate private memory\n");
				lock_release(list_lock);
				return;
			}
			memset(new_element, 0, sizeof(ul_db_handle_list_t));
			if((new_element->handle = pkg_malloc(sizeof(ul_db_handle_t))) == NULL){
				LM_ERR("couldn't allocate private memory\n");
				pkg_free(new_element);
				lock_release(list_lock);
				return;
			}
			memset(new_element->handle, 0, sizeof(ul_db_handle_t));
			new_element->next = handles;
			handles = new_element;
			handles->handle->id = tmp->id;
			ul_db_check(handles->handle);
		}
		tmp = tmp->next;
		i++;
	}
	lock_release(list_lock);
}

int ul_register_watch_db(int id){
	ul_db_watch_list_t * new_id = NULL, * tmp;
	if(!list_lock){
		if(init_watch_db_list() < 0){
			return -1;
		}
	}
	lock_get(list_lock);
	tmp = *list;
	while(tmp){
		if(tmp->id == id){
			tmp->active = 1;
			lock_release(list_lock);
			return 0;
		}
		tmp = tmp->next;
	}
	if((new_id = shm_malloc(sizeof(ul_db_watch_list_t))) == NULL){
		LM_ERR("couldn't allocate shared memory\n");
		lock_release(list_lock);
		return -1;
	}
	memset(new_id, 0, sizeof(ul_db_watch_list_t));
	new_id->active = 1;
	new_id->next = *list;
	new_id->id = id;
	*list = new_id;
	lock_release(list_lock);
	return 0;
}

int ul_unregister_watch_db(int id){
	ul_db_watch_list_t * tmp;
	if(!list_lock){
		return 0;
	}
	lock_get(list_lock);
	tmp = *list;
	while(tmp){
		if(tmp->id == id){
			tmp->active = 0;
			lock_release(list_lock);
			return 0;
		}
		tmp = tmp->next;
	}
	lock_release(list_lock);
	return 0;
}

static int init_watch_db_list(void){
	if((list_lock = lock_alloc()) == NULL){
		LM_ERR("could not allocate lock\n");
		return -1;
	}
	if(lock_init(list_lock) == NULL){
		LM_ERR("could not initialise lock\n");
		return -1;
	}
	return 0;
}
