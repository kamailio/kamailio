/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * History:
 * --------
 *  2005-05-02  flags field added to node stucture -better sync between timer
 *              and worker processes; some races eliminated (bogdan)
 */


#include <assert.h>

#include "../../dprint.h"
#include "timer.h"
#include "ip_tree.h"





inline void append_to_timer(struct list_link *head, struct list_link *new_ll )
{
	DBG("DEBUG:pike:append_to_timer:  %p in %p(%p,%p)\n",
		new_ll, head,head->prev,head->next);
	assert( !has_timer_set(new_ll) );

	new_ll->prev = head->prev;
	head->prev->next = new_ll;
	head->prev = new_ll;
	new_ll->next = head;
}



inline void remove_from_timer(struct list_link *head, struct list_link *ll)
{
	DBG("DEBUG:pike:remove_from_timer:  %p from %p(%p,%p)\n",
		ll, head,head->prev,head->next);
	assert( has_timer_set(ll) );

	ll->next->prev = ll->prev;
	ll->prev->next = ll->next;

	ll->next = ll->prev = 0;
}


void update_in_timer(struct list_link *head, struct list_link *ll)
{
	remove_from_timer( head, ll);
	append_to_timer( head, ll);
}


/* "head" list MUST not be empty */
void check_and_split_timer(struct list_link *head, int time,
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
		DBG("DEBUG:pike:check_and_split_timer: splitting %p(%p,%p)node=%p\n",
			ll,ll->prev,ll->next, node);
		/* mark the node as expired and un-mark it as being in timer list */
		node->flags |= NODE_EXPIRED_FLAG;
		node->flags &= ~NODE_INTIMER_FLAG;
		b = node->branch;
		ll=ll->next;
		/*DBG("DEBUG:pike:check_and_split_timer: b=%d; [%d,%d]\n",
				b,b>>3,1<<(b&0x07));*/
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

	DBG("DEBUG:pike:check_and_split_timer: succ. to split (h=%p)(p=%p,n=%p)\n",
		head,head->prev,head->next);
	return;
}




