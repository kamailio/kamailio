/*
 * Route & Record-Route module
 *
 * $Id$
 */

#include "../../sr_module.h"
#include <stdio.h>
#include "utils.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "rr.h"

#define MAX_RR_LEN 80

/*
 * Rewrites request URI from Route HF if any
 */
static int rewriteFromRoute(struct sip_msg* _m, char* _s1, char* _s2);

/*
 * Adds a Record Route entry for this proxy
 */
static int addRecordRoute(struct sip_msg* _m, char* _s1, char* _s2);


static struct module_exports rr_exports= {
	"rr",
	(char*[]) {
		"rewriteFromRoute",
		"addRecordRoute"
	},
	(cmd_function[]) {
		rewriteFromRoute,
		addRecordRoute
	},
	(int[]) {
		0,
		0
	},
	(fixup_function[]) {
		0,
		0
	},
	2, /* number of functions*/
	0, /* response function*/
	0, /* destroy function */
	0, /* oncancel function */
	0  /* per-child init function */
};


#ifdef STATIC_RR
struct module_exports* rr_mod_register()
#else
struct module_exports* mod_register()
#endif
{
	fprintf(stderr, "rr - registering\n");
	return &rr_exports;
}


/*
 * Rewrites request URI from Route HF if any
 */

static int rewriteFromRoute(struct sip_msg* _m, char* _s1, char* _s2)
{
	str first_uri;
	char* next_uri;
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "rewriteFromRoute(): Invalid parameter _m\n");
		return -2;
	}
#endif

	if (findRouteHF(_m) != FALSE) {
		if (parseRouteHF(_m, &first_uri, &next_uri) == FALSE) {
			LOG(L_ERR, "rewriteFromRoute(): Error while parsing Route HF\n");
			return -1;
		}
		if (rewriteReqURI(_m, &first_uri) == FALSE) {
			LOG(L_ERR, "rewriteFromRoute(): Error while rewriting request URI\n");
			return -1;
		}
		if (remFirstRoute(_m, next_uri) == FALSE) {
			LOG(L_ERR, "rewriteFromRoute(): Error while removing the first Route URI\n");
			return -1;
		}
		return 1;
	}
	DBG("rewriteFromRoute(): There is no Route HF\n");
	return -1;
}


/*
 * Adds a Record Route entry for this proxy
 */
static int addRecordRoute(struct sip_msg* _m, char* _s1, char* _s2)
{
	str b;
#ifdef PARANOID
	if (!_m) {
		LOG(L_ERR, "addRecordRoute(): Invalid parameter _m\n");
		return -2;
	}
#endif
	b.s = (char*)pkg_malloc(MAX_RR_LEN);
	if (!b.s) {
		LOG(L_ERR, "addRecordRoute(): No memory left\n");
		return -1;
	}

	if (buildRRLine(_m, &b) == FALSE) {
		LOG(L_ERR, "addRecordRoute(): Error while building Record-Route line\n");
		pkg_free(b.s);
		return -1;
	}

	if (addRRLine(_m, &b) == FALSE) {
		LOG(L_ERR, "addRecordRoute(): Error while adding Record-Route line\n");
		pkg_free(b.s);
		return -1;
	}
	return 1;
}
