/*
 * $Id$
 *
 */

#include "t_dlg.h"

static struct dialog *dlg=0;

int t_newdlg( struct sip_msg *msg )
{
	/* place-holder */
	dlg=0;
	return 0;
}

struct dialog *t_getdlg() {
	return dlg;
}

