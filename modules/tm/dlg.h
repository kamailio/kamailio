/*
 * $Id$
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
 *   info@iptel.org
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
 * -------
 * 2003-03-29 Created by janakj
 */

#ifndef DLG_H
#define DLG_H


#include <stdio.h>
#include "../../str.h"
#include "../../parser/parse_rr.h"
#include "../../parser/msg_parser.h"


/*
 * Dialog sequence
 */
typedef struct dlg_seq {
	unsigned int value;    /* Sequence value */
	unsigned char is_set;  /* is_set flag */
} dlg_seq_t;


/*
 * Dialog state
 */
typedef enum dlg_state {
	DLG_NEW = 0,   /* New dialog, no reply received yet */
	DLG_EARLY,     /* Early dialog, provisional response received */
	DLG_CONFIRMED, /* Confirmed dialog, 2xx received */
	DLG_DESTROYED  /* Destroyed dialog */
} dlg_state_t;


/*
 * Structure describing a dialog identifier
 */
typedef struct dlg_id {
	str call_id;    /* Call-ID */
	str rem_tag;    /* Remote tag of the dialog */
	str loc_tag;    /* Local tag of the dialog */
} dlg_id_t;


/*
 * It is neccessary to analyze the dialog data to find out
 * what URI put into the Record-Route, where the message
 * should be really sent and how to construct the route
 * set of the message. This structure stores this information
 * so we don't have to calculate each time we want to send a
 * message within dialog
 */
typedef struct dlg_hooks {
	str* request_uri;   /* This should be put into Request-URI */
	str* next_hop;      /* Where the message should be really sent */
	rr_t* first_route;  /* First route to be printed into the message */
	str* last_route;    /* If not zero add this as the last route */
} dlg_hooks_t;


/*
 * Structure representing dialog state
 */
typedef struct dlg {
	dlg_id_t id;            /* Dialog identifier */
	dlg_seq_t loc_seq;      /* Local sequence number */
	dlg_seq_t rem_seq;      /* Remote sequence number */
	str loc_uri;            /* Local URI */
	str rem_uri;            /* Remote URI */
	str rem_target;         /* Remote target URI */
	unsigned char secure;   /* Secure flag -- currently not used */
	dlg_state_t state;      /* State of the dialog */
	rr_t* route_set;        /* Route set */
	dlg_hooks_t hooks;      /* Various hooks used to store information that
				 * can be reused when building a message (to
				 * prevent repeated analysing of the dialog data
				 */
} dlg_t;


/*
 * Create a new dialog
 */
int new_dlg_uac(str* _cid, str* _ltag, unsigned int _lseq, str* _luri, str* _ruri, dlg_t** _d);


/*
 * A response arrived, update dialog
 */
int dlg_response_uac(dlg_t* _d, struct sip_msg* _m);


/*
 * Establishing a new dialog, UAS side
 */
int new_dlg_uas(struct sip_msg* _req, int _code, str* _tag, dlg_t** _d);


/*
 * UAS side - update a dialog from a request
 */
int dlg_request_uas(dlg_t* _d, struct sip_msg* _m);


/*
 * Destroy a dialog state
 */
void free_dlg(dlg_t* _d);


/*
 * Print a dialog structure, just for debugging
 */
void print_dlg(FILE* out, dlg_t* _d);


/*
 * Calculate length of the route set
 */
int calculate_routeset_length(dlg_t* _d);


/*
 *
 * Print the route set
 */
char* print_routeset(char* buf, dlg_t* _d);


#endif /* DLG_H */
