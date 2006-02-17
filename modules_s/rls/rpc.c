#include "rpc.h"
#include "../../dprint.h"

#include <unistd.h>

/*
#define rpc_lf(rpc, c)	rpc->add(c, "s","")
	rpc->printf(c, "    %.*s contact=\'%.*s\' exp=%u status=%d published=%d (id=%.*s)", 
				FMT_STR(t->id), FMT_STR(t->contact), t->expires - time(NULL),
				(int)t->state, t->is_published, FMT_STR(t->published_id));
	rpc_lf(rpc, c);
*/

/* method for UAC testing */
static void rls_test(rpc_t *rpc, void *c)
{
	rpc->add(c, "s", "test called");
	rpc->send(c);
}

/* ----- exported data structure with methods ----- */

static const char* rls_test_doc[] = {
	"Testing events.",  /* Documentation string */
	0                                    /* Method signature(s) */
};

/* 
 * RPC Methods exported by this module 
 */

rpc_export_t rls_rpc_methods[] = {
	{"rls.test",   rls_test,     rls_test_doc, 0},
	{0, 0, 0, 0}
};
