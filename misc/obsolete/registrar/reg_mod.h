/* 
 * $Id$ 
 *
 * registrar module interface
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#ifndef REG_MOD_H
#define REG_MOD_H

#include "../../parser/msg_parser.h"
#include "../../qvalue.h"
#include "../../usr_avp.h"
#include "../usrloc/usrloc.h"
#include "../../modules/sl/sl.h"

extern int default_expires;
extern qvalue_t default_q;
extern int append_branches;
extern int load_nat_flag;
extern int save_nat_flag;
extern int trust_received_flag;
extern int min_expires;
extern int max_expires;
extern int received_avp;
extern float def_q;
extern int received_to_uri; /*copy received to uri, don't add it to dst_uri*/
extern str rcv_param;
extern str aor_attr;
extern str server_id_attr;
extern int max_contacts;

extern usrloc_api_t ul;  /* Structure containing pointers to usrloc functions */

extern sl_api_t slb;

extern str reply_code_attr, reply_reason_attr, contact_attr;
extern avp_ident_t avpid_code, avpid_reason, avpid_contact;

#endif /* REG_MOD_H */
