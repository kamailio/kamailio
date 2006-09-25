/*
 * $Id$
 *
 * Copyright (C) 2006 SOMA Networks, Inc.
 * Written by Ron Winacott
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * History:
 * --------
 * 2006-05-11  initial version (ronw)
 */


/**
 * SST support:
 * 
 * The Session-Expires header conveys the session interval for a SIP
 * call. It is placed in an INVITE request and is allowed in any 2xx
 * class response to an INVITE. Its presence indicates that the UAC
 * wishes to use the session timer for this call. Unlike the
 * SIP-Expires header, it can only contain a delta-time, which is the
 * current time, plus the session interval from the response.
 *
 * For example, if a UAS generates a 200 OK response to a re-INVITE
 * that contained a Session-Expires header with a value of 1800
 * seconds (30 minutes), the UAS computes the session expiration as 30
 * minutes after the time when the 200 OK response was sent. For each
 * proxy, the session expiration is 30 minutes after the time when the
 * 2xx was received or sent. For the UAC, the expiration time is 30
 * minutes after the receipt of the final response.
 */

#include <stdlib.h> /* For atoi() */
#include <time.h>   /* For time() */

#include "../../parser/parse_sst.h"

#include "sst_handlers.h"

extern struct dlg_binds *dlg_binds;

/**
 * Look inside the msg for SST support.
 */
static void sstDialogTerminate(struct dlg_cell* did, int type,
		struct sip_msg* msg, void** param);

static void sstDialogUpdate(struct dlg_cell* did, int type, 
		struct sip_msg* msg, void** param);

static int sstUpdateSST(struct sip_msg *msg);

static xl_spec_t *timeout_avp = 0;

static unsigned int sst_minSE = 0;

/**
 * This is not a public API. This function is called when the module is loaded
 * to initialize the callback handlers.
 * 
 * @param timeout_avp_p - The pointer to the dialog modules timeout AVP.
 * @param minSE - The miniumum session expire value allowed by this PROXY. 
 */
void sstHandlerInit(xl_spec_t *timeout_avp_p, unsigned int minSE) 
{
	timeout_avp = timeout_avp_p;
	sst_minSE = minSE;
}

/**
 * Every time a new dialog is created (from a new INVITE) the dialog
 * module will call this callback function. We need to track the
 * dialogs lifespan from this point forward until it is terminated
 * with a BYE, CANCEL, etc. In the process, we will see if either or
 * both ends of the dialog support SIP Session Timers and setup the
 * dialog timeout to expire at the session timer expire time. Each
 * time the new re-INVITE is seen to update the SST, we will reset the
 * life span of the dialog to match it.
 *
 * If the dialog expires, we will send a BYE to both ends of the
 * conversation to PROXY terminate the dialog. 
 * FIXME: Need to check if this is legial or not.
 *
 * This function will setup the other types of dialog callbacks
 * required to track the lifespan of the dialog. It will also call any
 * sst created registered callbacks.
 *
 * @param did - The dialog ID
 * @param type - The trigger event type (CREATED)
 * @param msg - The SIP message that triggered the callback (INVITE)
 * @param param - The pointer to nothing. As we did not attach
 *                anything to this callback in the dialog module.
 *
 */
void sstDialogCreatedCB(struct dlg_cell *did, int type, 
		struct sip_msg* msg, void** param) 
{
	int rtn = 0;

	if (msg->first_line.type == SIP_REQUEST && 
			msg->first_line.u.request.method_value == METHOD_INVITE) {
		if ((rtn = sstUpdateSST(msg)) == 0) {
			/*
			 * Register for the other callbacks from the dialog.
			 */
			DBG("DEBUG:sst:sstDialogCreatedCB: Adding callback registrations "
				"DLGCB_FAILED|DLGCB_TERMINATED|DLGCB_EXPIRED\n");
			dlg_binds->register_dlgcb(did,
				DLGCB_FAILED|DLGCB_TERMINATED|DLGCB_EXPIRED,
				sstDialogTerminate, did);
			DBG("DEBUG:sst:sstDialogCreatedCB: Adding callback registrations "
				"DLGCB_REQ_WITHIN\n");
			dlg_binds->register_dlgcb(did, DLGCB_REQ_WITHIN, 
				sstDialogUpdate, did);
		}
	}
	return;
}

/**
 * The sst_checkMin() script command handler. Return 1 (true) if the
 * MIN-SE: of the message is too small compared to the passed in
 * value. This will allow the script to reply to this INVITE with a
 * "422 Session Timer Too Small" response.
 *
 * @param msg - The sip message from the script
 * @param str1 - The first function argument, the minimum time in
 *               seconds.
 * @param str2 - Not used.
 *
 * @return 0  if message MIN-SE is less then or equal to str1.
 *         -1 on an error, like no MIN-SE: header found.
 *         1 if message MIN-SE is greater then the str1 value. (too
 *           small for us)
 */
int sstCheckMinHandler(struct sip_msg *msg, char *str1, char *str2) 
{
	enum parse_sst_result result;
	unsigned minse = 0;
	int rtn = 0;

	/*
	 * Only look in INVITES or 2XX messages for the MIN-SE header.
	 *
	 * FIXME: Doe we care about SIP responses here? Only INVITEs should
	 *        be looked at. (need to check the spec for this)
	 */
	if ((msg->first_line.type == SIP_REQUEST && 
		msg->first_line.u.request.method_value == METHOD_INVITE)
		|| (msg->first_line.type == SIP_REPLY && 
		msg->first_line.u.reply.statuscode > 199 && 
		msg->first_line.u.reply.statuscode < 300)) {
		/*
		 * The message is an INVITE request, or a 2XX response so look for
		 * the MIN-SE header.
		 */
		if ((result = parse_min_se(msg, &minse)) == parse_sst_success) {
			if (str1 != 0) {
				if (minse < atoi(str1)) {
	 				DBG("DEBUG:sst:sstCheckMin: MINSE is too small. "
						"Returning true!\n");
					rtn = 1; /* Too small! */
				}
			} else {
				/*
				 * Did not pass in the correct and complet information.
				 */
				LOG(L_ERR, "ERROR:sst:sstCheckMin: Call to sstCheckMin() "
					"made with insufficient arguments.\n");
				rtn = -1;
			}
		} else {
			if (result != parse_sst_header_not_found) {
				/*
			 	 * log the error if the header was found.
			 	 */
				LOG(L_ERR, "ERROR:sst:sstCheckMin: Error parsing "
					"min_se header.");
			}
			rtn = -1;
		}
	} else {
		/*
		 * Log it, and return
		 */
		LOG(L_WARN, "WARMING:sst:sstCheckMin: The SIP message would not "
			"carry a MIN_SE header. Calling sstCheckMin() out of context.\n");
		rtn = -1;
	}
	return(rtn);
}

/**
 * This callback is called when ever a dialog is terminated. The cause of the
 * termination can be normal, failed call, or expired. It is the expired dialog
 * we are really interested in.
 * 
 * @param did - The Dialog ID / structure pointer. Used as an ID only.
 * @param type - The termination cause/reason.
 * @param msg - The pointer to the SIP message. On an DLGCB_EXPIRED type, the 
 *              message is a FAKED_REPLY (-1) and cannot be looked at safely.
 * @param param - Not used
 */
static void sstDialogTerminate(struct dlg_cell* did, int type, 
		struct sip_msg* msg, void** param) 
{
	switch (type) {
		case DLGCB_FAILED:
			DBG("DEBUG:sst:sstDialogTerminate: DID %p failed (canceled). "
					"Terminating session.\n", did);
			break;
		case DLGCB_EXPIRED:
			/* In the case of expired, the msg is pointing at a
			 * FAKED_REPLY (-1)
			 */
			LOG(L_ERR, "ERROR:sst:sstDialogTerminate: DID %p expired. "
					"Terminating session.\n", did);
			break;
		default: /* Normal termination. */
			DBG("DEBUG:sst:sstDialogTerminate:Terminating DID %p session\n",
					did);
			break;
	}
	return;
}


/**
 * Simple callback wrapper around the sstUpdateSST() function below.
 * This callback is called on any SIP message that will update the state of
 * the dialog. This includes the reINVITE that will have the new expire:
 * header value.
 * 
 * @param did - The dialog structure. The pointer is used as an ID.
 * @param type - The reason for the callback. Not used.
 * @param msg - The SIP message that causes the callback.
 * @param param - Not used.
 */
static void sstDialogUpdate(struct dlg_cell* did, int type, 
		struct sip_msg* msg, void** param) 
{
	int rtn = 0;

	if ((rtn = sstUpdateSST(msg)) == 0) {
		/* all Okay */
	}
}


/**
 * The heart of the SST module. This function is called when a dialog
 * is created or when a diualog is updated (with an INVITE) to reset
 * the dialog expire timer to the new SST value.
 * 
 * @param msg - The SIP message that could cause a SST update.
 * @return 0 on sucess. none zero on failure.
 */
static int sstUpdateSST(struct sip_msg *msg) 
{
	int rtn = 0;
	struct session_expires *se = NULL;
	unsigned int min_se = 0;
	xl_value_t xl_val;

	if ((rtn = parse_session_expires(msg, se)) != parse_sst_success) {
		if (rtn != parse_sst_header_not_found) {
			LOG(L_ERR, "ERROR:sst:sstUpdateSST: Could not parse expire "
					"message header on INVITE.\n");
		}
		return(-1);
	}

	if ((rtn = parse_min_se(msg, &min_se)) != parse_sst_success) {
		if (rtn != parse_sst_header_not_found) {
			LOG(L_ERR, "ERROR:sst:sstUpdateSST: Could not parse min_se "
					"message header on INVITE.\n");
		}
		return(-2);
	}

	/*
	 * We have an expire. See if we are bound to the dialog module.
	 */
	if (dlg_binds->register_dlgcb == NULL) {
		LOG(L_ERR, "ERROR:sst:sstUpdateSST: Dialog bind registration "
				"function pointer is NULL\n");
		return(-3);
	}

#ifdef PROXY_MIN_SE_REWITE_ALLOWED
	if (min_se < sst_minSE) {
		/* Rewrite the min_se header */
		LOG(L_ERR, "ERROR:sst:sstUpdateSST: NS-min_SE %d is less then "
				"(<) configured minSE %d.\n", min_se, sst_minSE);
	}
#endif /* PROXY_MIN_SE_REWITE_ALLOWED */
	
	/*
	 * Set the dialog timeout value to expire $avp(id[N])
	 * timeout_avp. We set the value here then when this callback
	 * returns control to the dialog module it will update the dialog
	 * timeout expire time to the value we just stored in the AVP.
	 */
	if (timeout_avp && xl_get_spec_value(msg, timeout_avp, &xl_val, 0) == 0
			&& xl_val.flags & XL_VAL_INT 
			&& xl_val.ri > 0 ) {
		/* We now hold a reference to the AVP int value */
		DBG("DEBUG:sst:sstUpdateSST: Current timeout value is %d, setting "
				"it to %d\n", xl_val.ri, se->interval);
		xl_val.ri = (time(NULL) + se->interval);
	} else {
		LOG(L_ERR, "ERROR:sst:sstUpdateSST: No timeout AVP or could not "
				"locate it. SST not reset.\n");
		return(-4); /* do not setup the callbacks */
	}
	return(0);
}
