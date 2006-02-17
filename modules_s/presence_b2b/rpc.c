#include "rpc.h"
#include "../../dprint.h"

#include "events_uac.h"
#include <unistd.h>

#include "../pa/presentity.h"

/*
#define rpc_lf(rpc, c)	rpc->add(c, "s","")
	rpc->printf(c, "    %.*s contact=\'%.*s\' exp=%u status=%d published=%d (id=%.*s)", 
				FMT_STR(t->id), FMT_STR(t->contact), t->expires - time(NULL),
				(int)t->state, t->is_published, FMT_STR(t->published_id));
	rpc_lf(rpc, c);
*/

/* method for UAC testing */
void test1(rpc_t *rpc, void *c)
{
	events_uac_t *uac = NULL;
	str presence = STR_STATIC_INIT("presence");
	/* str reg = STR_STATIC_INIT("reg"); */
	str to = STR_STATIC_INIT("<sip:ms2@test-domain.com>");
	str from = STR_STATIC_INIT("<sip:b2b@test-domain.com>");
	str route = STR_STATIC_INIT("<sip:vencore.sip-server.net:5060;lr>");
	
	rpc->add(c, "s", "test called");
	rpc->send(c);

	uac = create_events_uac(&to, &from, &presence, NULL, NULL, NULL, &route);

	sleep(5); /* wait a moment */
	destroy_events_uac(uac);/* remove */
}

static void test(rpc_t *rpc, void *c)
{
	void *st;
	
	/*rpc->add(c, "s", "test called");
	rpc->send(c);*/

	if (rpc->add(c, "{", &st) < 0) return;
	rpc->struct_add(st, "d", "watcher_t", sizeof(watcher_t));
	rpc->struct_add(st, "d", "dlg_t", sizeof(dlg_t));
	rpc->struct_add(st, "d", "presentity_t", sizeof(presentity_t));
	rpc->send(c);
}

/* ----- exported data structure with methods ----- */

static const char* test_doc[] = {
	"Testing events.",  /* Documentation string */
	0                   /* Method signature(s) */
};

/* 
 * RPC Methods exported by this module 
 */

rpc_export_t events_rpc_methods[] = {
	{"presence_b2b.test", test, test_doc, 0},
	{0, 0, 0, 0}
};

