/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*!
 * \file 
 * \brief TM :: 
 * \ingroup tm
 */


#ifndef DLG_H
#define DLG_H


#include <stdio.h>
#include "../../str.h"
#include "../../ip_addr.h"
#include "../../parser/parse_rr.h"
#include "../../parser/msg_parser.h"

/*
#define DIALOG_CALLBACKS
*/

#ifdef DIALOG_CALLBACKS
#include "t_hooks.h"
#include "h_table.h"

#define DLG_CB_UAC 30
#define DLG_CB_UAS 31

#endif /* DIALOG_CALLBACKS */


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
 * It is necessary to analyze the dialog data to find out
 * what URI put into the Record-Route, where the message
 * should be really sent and how to construct the route
 * set of the message. This structure stores this information
 * so we don't have to calculate each time we want to send a
 * message within dialog
 */
typedef struct dlg_hooks {
	str ru;
	str nh;
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
	str dst_uri;		/* Destination URI */
	str loc_dname;          /* Local Display Name */
	str rem_dname;          /* Remote Display Name */
	unsigned char secure;   /* Secure flag -- currently not used */
	dlg_state_t state;      /* State of the dialog */
	rr_t* route_set;        /* Route set */
	dlg_hooks_t hooks;      /* Various hooks used to store information that
				 * can be reused when building a message (to
				 * prevent repeated analyzing of the dialog data
				 */
	struct socket_info* send_sock;
#ifdef DIALOG_CALLBACKS
	struct tmcb_head_list dlg_callbacks;
#endif
} dlg_t;

typedef enum {
	IS_TARGET_REFRESH,
	IS_NOT_TARGET_REFRESH,
	TARGET_REFRESH_UNKNOWN
} target_refresh_t;

/*
 * Create a new dialog
 */
int new_dlg_uac(str* _cid, str* _ltag, unsigned int _lseq, str* _luri, str* _ruri, dlg_t** _d);
typedef int (*new_dlg_uac_f)(str* _cid, str* _ltag, unsigned int _lseq, str* _luri, str* _ruri, dlg_t** _d);


/**
* Function to add Display Names to an existing dialog
*/
int dlg_add_extra(dlg_t* _d, str* _ldname, str* _rdname);
typedef int (*dlg_add_extra_f)(dlg_t* _d, str* _ldname, str* _rdname);


/*
 * A response arrived, update dialog
 */
int dlg_response_uac(dlg_t* _d, struct sip_msg* _m, target_refresh_t is_target_refresh);
typedef int (*dlg_response_uac_f)(dlg_t* _d, struct sip_msg* _m, target_refresh_t is_target_refresh);

/*
 * Establishing a new dialog, UAS side
 */
int new_dlg_uas(struct sip_msg* _req, int _code, /*str* _tag,*/ dlg_t** _d);
typedef int (*new_dlg_uas_f)(struct sip_msg* _req, int _code, dlg_t** _d);

/*
 * UAS side - update dialog state and to tag
 */
int update_dlg_uas(dlg_t *_d, int _code, str* _tag);
typedef int (*update_dlg_uas_f)(dlg_t *_d, int _code, str* _tag);

/*
 * UAS side - update a dialog from a request
 */
int dlg_request_uas(dlg_t* _d, struct sip_msg* _m, target_refresh_t is_target_request);
typedef int (*dlg_request_uas_f)(dlg_t* _d, struct sip_msg* _m, target_refresh_t is_target_request);


/*
 * Destroy a dialog state
 */
void free_dlg(dlg_t* _d);
typedef void (*free_dlg_f)(dlg_t* _d);


/*
 * Print a dialog structure, just for debugging
 */
void print_dlg(FILE* out, dlg_t* _d);
typedef void (*print_dlg_f)(FILE* out, dlg_t* _d);


/*
 * Calculate length of the route set
 */
int calculate_routeset_length(dlg_t* _d);


/*
 *
 * Print the route set
 */
char* print_routeset(char* buf, dlg_t* _d);

/*
 * wrapper to calculate_hooks
 * added by dcm
 */
int w_calculate_hooks(dlg_t* _d);
typedef int (*calculate_hooks_f)(dlg_t* _d);

/*
 * set dialog's request uri and destination uri (optional)
 */
int set_dlg_target(dlg_t* _d, str* _ruri, str* _duri);
typedef int (*set_dlg_target_f)(dlg_t* _d, str* _ruri, str* _duri);

#ifdef DIALOG_CALLBACKS

/* dialog callback
 * params:  type - DLG_UAC or DLG_UAS
 *          dlg  - dialog structure
 *          msg  - message used for creating the new dialog for the new_dlg_uas
 *                 case, 0 otherwise (new_dlg_uac)
 */
typedef void (dialog_cb) (int type, dlg_t* dlg, struct sip_msg* msg);

/* callbacks for new dialogs (called each time a new dialog (uas or uac) is
 * created). Can be used for installing in-dialog callbacks
 * returns < 0 on error*/
int register_new_dlg_cb(int types, dialog_cb f, void* param);
/* callbacks for messages sent dialogs */
int register_dlg_tmcb(int type, dlg_t* dlg, transaction_cb f, void* param);
void run_trans_dlg_callbacks(dlg_t* dlg, struct cell* trans,
								struct retr_buf* rbuf);
/* cleanup on exit */
void destroy_new_dlg_cbs(void);

typedef int (*register_new_dlg_cb_f)(int, dialog_cb, void*);
typedef int (*register_dlg_tmcb_f)(int, dlg_t*, transaction_cb, void*);
#endif /* DIALOG_CALLBACKS */


#endif /* DLG_H */
