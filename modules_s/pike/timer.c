#include "timer.h"
#include "../../dprint.h"



void append_to_timer(struct pike_timer_head *pth, struct pike_timer *pt)
{
	if (pth->first) {
		pth->last->next = pt;
		pt->prev = pth->last;
	} else {
		pth->first = pt;
	}
	pth->last = pt;
}




int is_empty(struct pike_timer_head *pth )
{
	return ((pth->first==0)?1:0);
}




void remove_from_timer(struct pike_timer_head *pth, struct pike_timer *pt)
{
	if (pt->next)
		pt->next->prev = pt->prev;
	else
		pth->last = pt->prev;
	if (pt->prev)
		pt->prev->next = pt->next;
	else
		pth->first = pt->next;
	pt->next = pt->prev = 0;
}




struct pike_timer *check_and_split_timer(struct pike_timer_head *pth, int t)
{
	struct pike_timer *pt, *ret;

	pt = pth->first;
	while( pt && pt->timeout<=t)
		pt=pt->next;

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

	return ret;
}



