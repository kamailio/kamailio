/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * ----------
 * 2003-02-28 protocolization of t_uac_dlg completed (jiri)
 */


#ifndef _UAC_H
#define _UAC_H

#include "defs.h"


#include <stdio.h>
#include "config.h"
#include "t_dlg.h"

/* substitution character for FIFO UAC */
#define SUBST_CHAR '!'

#define DEFAULT_CSEQ	10

extern char *uac_from;
extern char *fifo;
extern int fifo_mode;

int uac_init();
int uac_child_init( int rank );

typedef int (*tuac_f)(str *msg_type, str *dst, str *headers,str *body,
	str *from, transaction_cb completion_cb, void *cbp,
	struct dialog *dlg );

typedef int (*tuacdlg_f)(str* msg_type, str* dst, int proto, str* ruri, str* to,
			 str* from, str* totag, str* fromtag, int* cseq,
			 str* callid, str* headers, str* body,
			 transaction_cb completion_cb, void* cbp
			 );

/* look at uac.c for usage guidelines */
/*
 * Send a request within a dialog
 */
int t_uac_dlg(str* msg,                     /* Type of the message - MESSAGE, OPTIONS etc. */
	      str* dst,                     /* Real destination (can be different 
										   than R-URI */
		  int proto,
	      str* ruri,                    /* Request-URI */
	      str* to,                      /* To - including tag */
	      str* from,                    /* From - including tag */
	      str* totag,                   /* To tag */
	      str* fromtag,                 /* From tag */
	      int* cseq,                    /* CSeq */
	      str* cid,                     /* Call-ID */
	      str* headers,                 /* Optional headers including CRLF */
	      str* body,                    /* Message body */
	      transaction_cb completion_cb, /* Callback parameter */
	      void* cbp                     /* Callback pointer */
	      );


int fifo_uac_dlg( FILE *stream, char *response_file );


#endif
