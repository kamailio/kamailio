/*
 * $Id$
 *
 * SIP message related functions
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
 * \brief SIP registrar module - SIP message related functions
 * \ingroup registrar   
 */  


#ifndef SIP_MSG_H
#define SIP_MSG_H

#include "../../qvalue.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"


/*! \brief
 * Parse the whole message and bodies of all header fields
 * that will be needed by registrar
 */
int parse_message_for_register(struct sip_msg* _m);


/*! \brief
 * Check if the originating REGISTER message was formed correctly
 * The whole message must be parsed before calling the function
 * _s indicates whether the contact was star
 */
int check_contacts(struct sip_msg* _m, int* _s);


/*! \brief
 * Calculate absolute expires value per contact as follows:
 * 1) If the contact has expires value, use the value. If it
 *    is not zero, add actual time to it
 * 2) If the contact has no expires parameter, use expires
 *    header field in the same way
 * 3) If the message contained no expires header field, use
 *    the default value
 */
//void calc_contact_expires(struct sip_msg* _m, param_t* _ep, int* _e);


/*! \brief
 * Calculate contact q value as follows:
 * 1) If q parameter exist, use it
 * 2) If the parameter doesn't exist, use default value
 */
int calc_contact_q(param_t* _q, qvalue_t* _r);

contact_t* get_first_contact(struct sip_msg* _m);
contact_t* get_next_contact(contact_t* _c);


#endif /* SIP_MSG_H */
