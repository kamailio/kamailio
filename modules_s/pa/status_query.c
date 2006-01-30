#include <stdio.h>
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "dlist.h"
#include "presentity.h"
#include "watcher.h"
#include "pstate.h"
#include "pdomain.h"
#include "pa_mod.h"
#include "common.h"

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
	else t = p->tuples;
	
	while (t) {
		switch (t->state) {
			case PS_ONLINE: return t;
			/* TODO: what about other state values? */
			default: break;
		}
		t = t->next;
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

	if (get_presentity_uid(&uid, _m) != 0) {
		ERR("handle_subscription(): Error while extracting presentity UID\n");
		return 0; /* ??? impossible to return -1 or 1 */
	}

	TRACE_LOG("is \'%.*s\' online ?\n", FMT_STR(uid));

	lock_pdomain(d);

	if (find_presentity_uid(d, &uid, &p) == 0) {
		/* presentity found */
		/* TODO: search for status */
		t = find_online_tuple(p, NULL);
		if (t) res = 1; /* online tuple found */
	}
	str_free_content(&uid);

	unlock_pdomain(d);

	return res;
}
