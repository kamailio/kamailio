/* 
 * Copyright (C) 2001-2003 FhG Fokus
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


#include <assert.h>

#include "../../dprint.h"
#include "timer.h"
#include "ip_tree.h"



void append_to_timer(struct list_link *head, struct list_link *new_ll )
{
	LM_DBG("%p in %p(%p,%p)\n",	new_ll, head,head->prev,head->next);
	assert( !has_timer_set(new_ll) );

	new_ll->prev = head->prev;
	head->prev->next = new_ll;
	head->prev = new_ll;
	new_ll->next = head;
}



void remove_from_timer(struct list_link *head, struct list_link *ll)
{
	LM_DBG("%p from %p(%p,%p)\n", ll, head,head->prev,head->next);
	assert( has_timer_set(ll) );

	ll->next->prev = ll->prev;
	ll->prev->next = ll->next;

	ll->next = ll->prev = 0;
}



/* "head" list MUST not be empty */
void check_and_split_timer(struct list_link *head, unsigned int time,
							struct list_link *split, unsigned char *mask)
{
	struct list_link *ll;
	struct ip_node   *node;
	unsigned char b;
	int i;

	/*  reset the mask */
	for(i=0;i<32;mask[i++]=0);

	ll = head->next;
	while( ll!=head && (node=ll2ipnode(ll))->expires<=time) {
		LM_DBG("splitting %p(%p,%p)node=%p\n", ll,ll->prev,ll->next, node);
		/* mark the node as expired and un-mark it as being in timer list */
		node->flags |= NODE_EXPIRED_FLAG;
		node->flags &= ~NODE_INTIMER_FLAG;
		b = node->branch;
		ll=ll->next;
		/*LM_DBG("b=%d; [%d,%d]\n",	b,b>>3,1<<(b&0x07));*/
		mask[b>>3] |= (1<<(b&0x07));
	}

	if (ll==head->next) {
		/* nothing to return */
		split->next = split->prev = split;
	} else {
		/* the detached list begins with current beginning */
		split->next = head->next;
		split->next->prev = split;
		/* and we mark the end of the split list */
		split->prev = ll->prev;
		split->prev->next = split;
		/* the shortened list starts from where we suspended */
		head->next = ll;
		ll->prev = head;
	}

	LM_DBG("succ. to split (h=%p)(p=%p,n=%p)\n", head,head->prev,head->next);
	return;
}




