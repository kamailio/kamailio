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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */


#ifndef REG_MOD_H
#define REG_MOD_H

#include "stats.h"
#include "../../parser/msg_parser.h"
#include "../../qvalue.h"
#include "../../usr_avp.h"
#include "../ims_usrloc_scscf/usrloc.h"
#include "../../modules/sl/sl.h"
#include "../../modules/tm/tm_load.h"

/* if DB support is used, this values must not exceed the
 * storage capacity of the DB columns! See db/schema/entities.xml */
#define CONTACT_MAX_SIZE       255
#define RECEIVED_MAX_SIZE      255
#define USERNAME_MAX_SIZE      64
#define DOMAIN_MAX_SIZE        128
#define CALLID_MAX_SIZE        255

#define PATH_MODE_STRICT	2
#define PATH_MODE_LAZY		1
#define PATH_MODE_OFF		0

#define REG_SAVE_MEM_FL     (1<<0)
#define REG_SAVE_NORPL_FL   (1<<1)
#define REG_SAVE_REPL_FL    (1<<2)
#define REG_SAVE_ALL_FL     ((1<<3)-1)

#define MOD_NAME "ims_registrar_scscf"
/** Return and break the execution of routng script */
#define CSCF_RETURN_BREAK	0
/** Return true in the routing script */
#define CSCF_RETURN_TRUE	1
/** Return false in the routing script */
#define CSCF_RETURN_FALSE -1
/** Return error in the routing script */
#define CSCF_RETURN_ERROR -2


extern int nat_flag;
extern int tcp_persistent_flag;
extern int received_avp;

extern unsigned short aor_avp_type;
extern int_str aor_avp_name;
extern unsigned short rcv_avp_type;
extern int_str rcv_avp_name;
extern unsigned short reg_callid_avp_type;
extern int_str reg_callid_avp_name;

extern str rcv_param;
extern int method_filtering;
extern int path_enabled;
extern int path_mode;
extern int path_use_params;

extern str sock_hdr_name;
extern int sock_flag;

extern usrloc_api_t ul;/*!< Structure containing pointers to usrloc functions*/

extern sl_api_t slb;

extern stat_var *accepted_registrations;
extern stat_var *rejected_registrations;
extern stat_var *default_expire_stat;
extern stat_var *max_expires_stat;


#endif /* REG_MOD_H */
