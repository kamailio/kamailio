#include "dialog.h"
#include <string.h>
#include "/home/janakj/sip_router/dprintf.h"


/*
 * Create a dialog
 */
int new_dialog(dialog_t** _d, struct sip_msg* _m, int _t)
{
	*_d = dmalloc(sizeof(dialog_t));
	if (*_d == 0) {
		LOG(L_ERR, "new_dialog(): No memory left\n");
		return -1;
	}
	memset(*_d, '0', sizeof(dialog_t));

	

	return 0;
}


/*
 * Match a dialog
 */
int match_dialog(dialog_t* _d, struct sip_msg* _m)
{

	return 0;
}


/*
 * Free all memory associated with dialog
 */
void free_dialog(dialog_t* _d)
{
	free_did(&_d->id);

	if (_d->luri.s) dfree(_d->luri.s);
	if (_d->ruri.s) dfree(_d->ruri.s);
	if (_d->rtarget.s) dfree(_d->rtarget.s);
	
	free_routeset(&_d->rs);
	dfree(_d);
}


/*
 * Just for debugging
 */
void print_dialog(FILE* _o, dialog_t* _d)
{
	fprintf(_o, "Dialog {\n");
	print_did(_o, &_d->did);
	fprintf(_o, "    Local Sequence : %d\n", _d->lseq);
	fprintf(_o, "    Remote Sequence: %d\n", _d->rseq);
	fprintf(_o, "    Local URI      : \'%.*s\'\n", _d->luri.len, _d->luri.s);
	fprintf(_o, "    Remote URI     : \'%.*s\'\n", _d->ruri.len, _d->ruri.s);
	fprintf(_o, "    Remote Target  : \'%.*s\'\n", _d->rtarget.len, _d->rtarget.s);
	fprintf(_o, "    Secure         : %s\n", (_d->secure) ? ("yes") : ("no"));
	print_routeset(_o, &_d->rs);
	print_state(_o, _d->state);
	fprintf(_o, "}\n");
}
