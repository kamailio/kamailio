#include "did.h"
#include "/home/janakj/sip_router/dprint.h"
#include <string.h>


/*
 * Create a new dialog ID from the given parameters
 */
int new_did(did_t* _d, str* _cid, str* _lt, str* _rt)
{
	_d->cid.s = dmalloc(_cid->len);
	if (_d->cid.s == 0) {
		LOG(L_ERR, "new_did(): No memory left\n");
		return -1;
	}

	_d->lt.s = dmalloc(_lt->len);
	if (_d->lt.s == 0) {
		LOG(L_ERR, "new_did(): No memory left\n");
		dfree(_d->cid.s);
		return -2;
	}

	_d->rt.s = dmalloc(_rt->len);
	if (_d->rt.s == 0) {
		LOG(L_ERR, "new_did(): No memory left\n");
		dfree(_d->cid.s);
		dfree(_d->rt.s);
		return -3;
	}

	memcpy(_d->cid.s, _cid->s, _cid->len);
	_d->cid.len = _cid->len;

	memcpy(_d->lt.s, _lt->s, _lt->len);
	_d->lt.len = _lt->len;

	memcpy(_d->rt.s, _rt->s, _rt->len);
	_d->rt.len = _rt->len;

	return 0;
}


/*
 * Free memory associated with dialog ID
 */
void free_did(did_t* _d)
{
	if (_d->cid.s) dfree(_d->cid.s);
	if (_d->lt.s) dfree(_d->lt.s);
	if (_d->rt.s) dfree(_d->rt.s);

	     /* We do not free the did structure here becase
	      * it is always part of a dialog structure and will
	      * be freed there
	      */
}


/*
 * Match a dialog ID against a SIP message
 */
int match_did(did_t* _d, str* _cid, str* _lt, str* _rt)
{
	if ((_d->cid.len != _cid->len) ||
	    (_d->lt.len != _lt->len) ||
	    (_d->rt.len != _lt->len)) {

		DBG("match_did(): Different length\n");
		return 1;
	}

	if (memcmp(_d->cid.s, _cid->s, _cid->len)) {
		return 2;
	}

	if (memcmp(_d->lt.s, _lt->s, _lt->len)) {
		return 3;
	}

	if (memcmp(_d->rt.s, _rt->s, _rt->len)) {
		return 4;
	}

	return 0;
}


/*
 * Just for debugging
 */
void print_did(FILE* _o, did_t* _d)
{
	fprintf(_o, "    Call-ID        : \'%.*s\'\n", _d->cid.len, _d->cid.s);
	fprintf(_o, "    Local tag      : \'%.*s\'\n", _d->lt.len, _d->lt.s);
	fprintf(_o, "    Remote tag     : \'%.*s\'\n", _d->rt.len, _d->rt.s);
}
