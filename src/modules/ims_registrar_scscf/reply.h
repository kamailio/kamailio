/*
 * $Id$
 *
 * Send a reply
 *
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
 */

/*!
 * \file
 * \brief SIP registrar module - send a reply
 * \ingroup registrar   
 */  


#ifndef REPLY_H
#define REPLY_H

#include "../../parser/msg_parser.h"
#include "../ims_usrloc_scscf/ucontact.h"
#include "../../parser/contact/contact.h"

#include "../../modules/tm/tm_load.h"

//we use shared memory for this so we can use async diameter
/*! \brief
 * Buffer for Contact header field
 */
typedef struct contact_for_header {
	char* buf;
	int buf_len;
	int data_len;
} contact_for_header_t;


/*! \brief
 * Send a reply
 */
int reg_send_reply(struct sip_msg* _m, contact_for_header_t* contact_header);


/*! \brief
 * Send a reply using tm
 */
int reg_send_reply_transactional(struct sip_msg* _m, contact_for_header_t* contact_header, struct cell* t_cell);


/*! \brief
 * Build Contact HF for reply
 */
int build_contact(impurecord_t* impurec, contact_for_header_t** contact_header);
int build_expired_contact(contact_t* chi, contact_for_header_t** contact_header); //this is for building the expired response - ie reply to dereg

int build_p_associated_uri(ims_subscription* s);


/*! \brief
 * Release contact buffer if any
 */
void free_contact_buf(contact_for_header_t* contact_header);

void free_p_associated_uri_buf(void);

void free_expired_contact_buf(void);



#endif /* REPLY_H */
