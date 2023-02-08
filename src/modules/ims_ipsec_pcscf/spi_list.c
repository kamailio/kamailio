/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
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

#include "spi_list.h"
#include "../../core/mem/shm_mem.h"


spi_list_t create_list()
{
	spi_list_t lst;
	lst.head = NULL;
	lst.tail = NULL;
	return lst;
}

void destroy_list(spi_list_t *lst)
{
	if(!lst) {
		return;
	}

	spi_node_t *l = lst->head;
	while(l) {
		spi_node_t *n = l->next;
		shm_free(l);
		l = n;
	}

	lst->head = NULL;
	lst->tail = NULL;
}

int spi_add(spi_list_t *list, uint32_t spi_cid, uint32_t spi_sid,
		uint16_t sport, uint16_t cport)
{
	if(!list) {
		return 1;
	}

	// create new node
	spi_node_t *n = shm_malloc(sizeof(spi_node_t));
	if(!n)
		return 1;

	n->next = NULL;
	n->spi_cid = spi_cid;
	n->spi_sid = spi_sid;
	n->sport = sport;
	n->cport = cport;

	//when list is empty
	if(!list->head) {
		list->head = n;
		list->tail = n;
		return 0;
	}

	list->tail->next = n;
	list->tail = n;

	return 0;
}

int spi_remove_head(spi_list_t *list)
{
	if(!list) {
		return 1;
	}

	//when list is empty
	if(!list->head) {
		return 1;
	}

	spi_node_t *t = list->head;
	list->head = t->next;
	shm_free(t);

	return 0;
}

int spi_remove(spi_list_t *list, uint32_t spi_cid, uint32_t spi_sid)
{
	if(!list) {
		return 0;
	}

	//when list is empty
	if(!list->head) {
		return 0;
	}

	//when target is head
	if(list->head->spi_cid == spi_cid && list->head->spi_sid == spi_sid) {
		spi_node_t *t = list->head;
		list->head = t->next;

		//if head==tail, adjust tail too
		if(t == list->tail) {
			list->tail = list->head;
		}

		shm_free(t);
		return 1;
	}


	spi_node_t *prev = list->head;
	spi_node_t *curr = list->head->next;

	while(curr) {
		if(curr->spi_cid == spi_cid && curr->spi_sid == spi_sid) {
			spi_node_t *t = curr;

			//detach node
			prev->next = curr->next;

			//is it the last element
			if(t == list->tail) {
				list->tail = prev;
			}

			shm_free(t);
			return 1;
		}

		prev = curr;
		curr = curr->next;
	}

	return -1; // out of scope
}

int spi_in_list(spi_list_t *list, uint32_t spi_cid, uint32_t spi_sid)
{
	if(!list) {
		return 0;
	}

	if(!list->head)
		return 0;

	spi_node_t *n = list->head;
	while(n) {
		if(n->spi_cid == spi_cid && n->spi_sid == spi_sid) {
			return 1;
		}
		n = n->next;
	}

	return 0;
}
