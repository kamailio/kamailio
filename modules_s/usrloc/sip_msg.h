/*
 * $Id$
 *
 * SIP message related functions
 */

#ifndef SIP_MSG_H
#define SIP_MSG_H


#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"


/*
 * Parse the whole messsage and bodies of all header fieds
 * that will be needed by registrar
 */
int parse_message(struct sip_msg* _m);


/*
 * Check if the originating REGISTER message was formed correctly
 * The whole message must be parsed before calling the function
 * _s indicates whether the contact was star
 */
int check_contacts(struct sip_msg* _m, int* _s);


/*
 * Get the first contact in message
 */
contact_t* get_first_contact(struct sip_msg* _m);


/* 
 * Get next contact in message
 */
contact_t* get_next_contact(contact_t* _c);


/*
 * Calculate absolute expires value per contact as follows:
 * 1) If the contact has expires value, use the value. If it
 *    is not zero, add actual time to it
 * 2) If the contact has no expires parameter, use expires
 *    header field in the same way
 * 3) If the message contained no expires header field, use
 *    the default value
 */
int calc_contact_expires(struct sip_msg* _m, cparam_t* _ep, int* _e);


/*
 * Calculate contact q value as follows:
 * 1) If q parameter exist, use it
 * 2) If the parameter doesn't exist, use default value
 */
int calc_contact_q(cparam_t* _q, float* _r);


#endif /* SIP_MSG_H */
