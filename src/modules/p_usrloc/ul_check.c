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

#include "../../mem/shm_mem.h"
#include "ul_check.h"
#include "time.h"

static struct check_list_head * head = NULL;

static struct check_list_element * initialise_element(void);

static void destroy_element(struct check_list_element * element);

int init_list(void) {
	if(!head){
		if((head = (struct check_list_head *)shm_malloc(sizeof(struct check_list_head))) == NULL){
			LM_ERR("couldn't allocate shared memory.\n");
			return -1;
		}
	}
	memset(head, 0, sizeof(struct check_list_head));
	
	if(lock_init(&head->list_lock) == 0){
		LM_ERR("cannot initialise lock.\n");
		shm_free(head);
		return -1;
	}
	return 0;
}

struct check_data * get_new_element(void) {
	struct check_list_element * ret;
	if(!head){
		LM_ERR("list not initialised.\n");
		return NULL;
	}
	LM_DBG("start.\n");
	lock_get(&head->list_lock);
	
	if((ret = initialise_element()) == NULL){
		lock_release(&head->list_lock);
		return NULL;
	}
	head->element_count++;
	if(head->first == NULL){
		LM_DBG("new element is the first.\n");
		LM_DBG("element_count: %i\n", head->element_count);
		head->first = ret;
		lock_release(&head->list_lock);
		return ret->data;
	}
	LM_DBG("new element.\n");
	LM_DBG("element_count: %i\n", head->element_count);
	ret->next = head->first;
	head->first = ret;
	lock_release(&head->list_lock);
	return ret->data;
}

int must_refresh(struct check_data * element) {
	int ret;
	lock_get(&element->flag_lock);
	ret = element->refresh_flag;
	LM_DBG("refresh_flag is at %i.\n", ret);
	element->refresh_flag = 0;
	lock_release(&element->flag_lock);
	return ret;
}

int must_reconnect(struct check_data * element) {
	int ret;
	lock_get(&element->flag_lock);
	ret = element->reconnect_flag;
	LM_DBG("reconnect_flag is at %i.\n", ret);
	element->reconnect_flag = 0;
	lock_release(&element->flag_lock);
	return ret;
}

int set_must_refresh(void) {
	struct check_list_element * tmp;
	int i = 0;
	lock_get(&head->list_lock);
	tmp = head->first;
	while(tmp){
		lock_get(&tmp->data->flag_lock);
		tmp->data->refresh_flag = 1;
		lock_release(&tmp->data->flag_lock);
		tmp = tmp->next;
		i++;
		LM_DBG("element no %i.\n", i);
	}
	lock_release(&head->list_lock);
	return i;
}

int set_must_reconnect(void) {
	struct check_list_element * tmp;
	int i = 0;
	lock_get(&head->list_lock);
	tmp = head->first;
	while(tmp){
		lock_get(&tmp->data->flag_lock);
		tmp->data->reconnect_flag = 1;
		lock_release(&tmp->data->flag_lock);
		tmp = tmp->next;
		i++;
		LM_DBG("element no %i.\n", i);
	}
	lock_release(&head->list_lock);
	return i;
}


int must_retry(time_t * timer, time_t interval){
	if(!timer){
		return -1;
	}
	LM_DBG("must_retry: time is at %i, retry at %i.\n", (int)time(NULL), (int)(*timer));
	if(*timer <= time(NULL)){
		*timer = time(NULL) + interval;
		return 1;
	}
	return 0;
}

void destroy_list(void) {
	struct check_list_element * tmp;
	struct check_list_element * del;
	if(head){
		tmp = head->first;
		while(tmp){
			del = tmp;
			tmp = tmp->next;
			destroy_element(del);
		}
		lock_destroy(&head->list_lock);
		shm_free(head);
	}
	return;
}

static struct check_list_element * initialise_element(void){
	struct check_list_element * ret;
	if((ret = (struct check_list_element *)shm_malloc(sizeof(struct check_list_element))) == NULL){
		LM_ERR("couldn't allocate shared memory.\n");
		return NULL;
	}
	memset(ret, 0, sizeof(struct check_list_element));
	
	if((ret->data = (struct check_data *)shm_malloc(sizeof(struct check_data))) == NULL){
		LM_ERR("couldn't allocate shared memory.\n");
		shm_free(ret);
		return NULL;
	}
	memset(ret->data, 0, sizeof(struct check_data));
	
	if(lock_init(&ret->data->flag_lock) == 0){
		LM_ERR("cannot initialise flag lock.\n");
		shm_free(ret->data);
		shm_free(ret);
		return NULL;
	}
	return ret;
}

static void destroy_element(struct check_list_element * element){
	if(element){
		if(element->data){
/*		if(element->data->flag_lock){ */
				lock_destroy(&element->data->flag_lock);
/*			}*/
			shm_free(element->data);
		}
		shm_free(element);
	}
	return;
}
