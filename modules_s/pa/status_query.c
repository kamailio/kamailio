#include <stdio.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "dlist.h"
#include "presentity.h"
#include "watcher.h"
#include "pdomain.h"
#include "pa_mod.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include <presence/pidf.h>
#include <cds/logger.h>

/* Helper functions */

presence_tuple_t *find_online_tuple(presentity_t *p, 
		presence_tuple_t *search_from)
{
	presence_tuple_t *t = NULL;
	
	if (!p) return t;
	if (search_from) t = search_from;
	else t = get_first_tuple(p);
	
	while (t) {
		switch (t->data.status.basic) {
			case presence_tuple_open: return t;
			/* TODO: what about other state values? */
			default: break;
		}
		t = get_next_tuple(t);
	}
	
	return NULL;
}

/* Handler functions */

int target_online(struct sip_msg* _m, char* _domain, char* _s2)
{
	struct pdomain* d;
	struct presentity *p;
	str uid = STR_NULL;
	int res = -1;
	presence_tuple_t *t;

	d = (struct pdomain*)_domain;

	if (get_presentity_uid(&uid, _m) < 0) {
		ERR("Error while extracting presentity UID\n");
		return 0; /* ??? impossible to return -1 or 1 */
	}

	/* TRACE_LOG("is \'%.*s\' online ?\n", FMT_STR(uid)); */

	lock_pdomain(d);

	if (find_presentity_uid(d, &uid, &p) == 0) {
		/* presentity found */
		t = find_online_tuple(p, NULL);
		if (t) res = 1; /* online tuple found */
	}

	unlock_pdomain(d);

	return res;
}

/* check watcher status for given value */
int test_watcher_status(struct sip_msg* _m, char* _domain, char* _status)
{
	/* returns watcher's authorization status (only existing watchers - should
	 * be called after processing SUBSCRIBE request) */

	/* find presentity, not found => -1*/
	/* find watcher, not found => -1 */
	/* test if auth == watcher status => -1/1 */

	return -1;
}

