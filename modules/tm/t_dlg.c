/*
 * $Id$
 *
 */

#include "t_dlg.h"

dlg_t dlg=0;

int t_newdlg( struct sip_msg *msg )
{
	/* place-holder */
	dlg=0;
	return 0;
}

dlg_t t_getdlg() {
	return dlg;
}

