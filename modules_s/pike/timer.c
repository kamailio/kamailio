/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>

#include "timer.h"
#include "../../dprint.h"


#define is_in_timer_list(_th,_tl) \
	(((_tl)->prev || (_tl)->next || (_th)->first==(_tl))?1:0)

#define valid_timer_head(_pth) \
	( ((_pth)->first&&(_pth)->last) || (!(_pth)->first&&!(_pth)->last) )
	

void append_to_timer(struct pike_timer_head *pth, struct pike_timer_link *pt)
{
	DBG("DEBUG:pike:append_to_timer: attempting to insert %p in (%p,%p)\n",
		pt,pth->first,pth->last);
	assert( valid_timer_head(pth) );
	if (is_in_timer_list(pth,pt))
		remove_from_timer(pth,pt);
	if (pth->first) {
		pth->last->next = pt;
		pt->prev = pth->last;
	} else {
		pth->first = pt;
	}
	pth->last = pt;
	assert( valid_timer_head(pth) );
	DBG("DEBUG:pike:append_to_timer: success to insert %p(%p,%p) in (%p,%p)\n",
		pt,pt->prev,pt->next,pth->first,pth->last);
}




int is_empty(struct pike_timer_head *pth )
{
	return ((pth->first==0)?1:0);
}




void remove_from_timer(struct pike_timer_head *pth, struct pike_timer_link *pt)
{
	DBG("DEBUG:pike:remove_from_timer: attempting to remove %p(%p,%p) in "
		"(%p,%p)\n",pt,pt->prev,pt->next,pth->first,pth->last);
	assert( valid_timer_head(pth) );
	if ( !is_in_timer_list(pth,pt) )
		return;
	if (pt->next==0) assert( pth->last==pt );
	if (pt->prev==0) assert( pth->first==pt );
	if (pt->next)
		pt->next->prev = pt->prev;
	else
		pth->last = pt->prev;
	if (pt->prev)
		pt->prev->next = pt->next;
	else
		pth->first = pt->next;
	pt->next = pt->prev = 0;
	assert( valid_timer_head(pth) );
	DBG("DEBUG:pike:remove_from_timer: success to remove %p(%p,%p) in "
		"(%p,%p)\n", pt,pt->prev,pt->next,pth->first,pth->last);
}




struct pike_timer_link *check_and_split_timer(struct pike_timer_head *pth, int t)
{
	struct pike_timer_link *pt, *ret;

	assert( valid_timer_head(pth) );
	pt = pth->first;
	while( pt && pt->timeout<=t) {
		DBG("DEBUG:pike:check_and_split_timer: splitting %p(%p,%p) in "
			"(%p,%p)\n",pt,pt->prev,pt->next,pth->first,pth->last);
		pt=pt->next;
	}

	if (!pt) {
		/* eveything have to be removed */
		ret = pth->first;
		pth->first = pth->last = 0;
	} else if (!pt->prev) {
		/* nothing to delete found */
		ret = 0;
	} else {
		/* we did find timers to be fired! */
		/* the detached list begins with current beginning */
		ret = pth->first;
		/* and we mark the end of the split list */
		pt->prev->next = 0;
		/* the shortened list starts from where we suspended */
		pth->first = pt;
		pt->prev = 0;
	}

	assert( valid_timer_head(pth) );
	DBG("DEBUG:pike:check_and_split_timer: success to split (%p,%p)\n",
		pth->first,pth->last);
	return ret;
}




