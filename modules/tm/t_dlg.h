/*
 * $Id$
 *
 */

#ifndef _T_DLG_H
#define _T_DLG_H

#include "../../parser/msg_parser.h"

struct dialog {
	int place_holder;
};

typedef struct dialog *dlg_t;

int t_newdlg( struct sip_msg *msg );
dlg_t t_getdlg() ;

#endif
