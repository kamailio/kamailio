/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
 * 
 * The initial version of this code was written by Dragos Vingarzan
 * (dragos(dot)vingarzan(at)fokus(dot)fraunhofer(dot)de and the
 * Fruanhofer Institute. It was and still is maintained in a separate
 * branch of the original SER. We are therefore migrating it to
 * Kamailio/SR and look forward to maintaining it from here on out.
 * 2011/2012 Smile Communications, Pty. Ltd.
 * ported/maintained/improved by 
 * Jason Penton (jason(dot)penton(at)smilecoms.com and
 * Richard Good (richard(dot)good(at)smilecoms.com) as part of an 
 * effort to add full IMS support to Kamailio/SR using a new and
 * improved architecture
 * 
 * NB: Alot of this code was originally part of OpenIMSCore,
 * FhG Fokus. 
 * Copyright (C) 2004-2006 FhG Fokus
 * Thanks for great work! This is an effort to 
 * break apart the various CSCF functions into logically separate
 * components. We hope this will drive wider use. We also feel
 * that in this way the architecture is more complete and thereby easier
 * to manage in the Kamailio/SR environment
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *
 * History:
 * --------
 *  2011-02-02  initial version (jason.penton)
 */

#ifndef RX_AUTH_DATA_H
#define RX_AUTH_DATA_H

extern struct tm_binds tmb;
extern struct cdp_binds cdpb;
extern struct dlg_binds dlgb;

enum dialog_direction {
    DLG_MOBILE_ORIGINATING = 1,
    DLG_MOBILE_TERMINATING = 2,
    DLG_MOBILE_REGISTER = 3,
    DLG_MOBILE_UNKNOWN = 4
};

typedef struct rx_authsessiondata {
    str callid;
    str ftag;
    str ttag;
    //for registration session
    int subscribed_to_signaling_path_status; // 0 not subscribed 1 is subscribed
    str domain;				//the domain the registration aor belongs to (for registration)
    str registration_aor; //the aor if this rx session is a subscription to signalling status
    int must_terminate_dialog; //0 means when this session terminates it must not terminate the relevant dialog, 1 means it must terminate the dialog
} rx_authsessiondata_t;

int create_new_regsessiondata(str* domain, str* aor, rx_authsessiondata_t** session_data);
int create_new_callsessiondata(str* callid, str* ftag, str* ttag, rx_authsessiondata_t** session_data);

#endif

