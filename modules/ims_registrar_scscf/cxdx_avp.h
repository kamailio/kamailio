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

#ifndef IS_CSCF_CXDX_AVP_H
#define IS_CSCF_CXDX_AVP_H

extern struct cdp_binds cdpb;            /**< Structure with pointers to cdp funcs 		*/
extern struct tm_binds tmb;

struct AAAMessage;
struct AAA_AVP;
struct sip_msg;

inline int cxdx_add_call_id(AAAMessage *msg, str data);
/**
 * Creates and adds a Destination-Realm AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_destination_realm(AAAMessage *msg, str data);

/**
 * Creates and adds a Vendor-Specifig-Application-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param vendor_id - the value of the vendor_id,
 * @param auth_id - the authorization application id
 * @param acct_id - the accounting application id
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_vendor_specific_appid(AAAMessage *msg,unsigned int vendor_id,unsigned int auth_id,unsigned int acct_id);

/**
 * Creates and adds a Auth-Session-State AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_auth_session_state(AAAMessage *msg,unsigned int data);

/**
 * Creates and adds a User-Name AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_user_name(AAAMessage *msg,str data);

/**
 * Creates and adds a Public Identity AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_public_identity(AAAMessage *msg,str data);

/**
 * Creates and adds a Visited-Network-ID AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_visited_network_id(AAAMessage *msg,str data);

/**
 * Creates and adds a UAR-Flags AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_UAR_flags(AAAMessage *msg, unsigned int sos_reg);

/**
 * Creates and adds a Authorization-Type AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_authorization_type(AAAMessage *msg,unsigned int data);

/**
 * Returns the Result-Code AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline int cxdx_get_result_code(AAAMessage *msg, int *data);

/**
 * Returns the Experimental-Result-Code AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline int cxdx_get_experimental_result_code(AAAMessage *msg, int *data);

/**
 * Returns the Server-Name AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline str cxdx_get_server_name(AAAMessage *msg);

/**
 * Returns the Capabilities from the grouped AVP from a Diameter message.
 * @param msg - the Diameter message
 * @param m - array to be filled with the retrieved mandatory capabilities
 * @param m_cnt - size of the array above to be filled
 * @param o - array to be filled with the retrieved optional capabilities
 * @param o_cnt - size of the array above to be filled
 * @returns 1 on success 0 on fail
 */
inline int cxdx_get_capabilities(AAAMessage *msg,int **m,int *m_cnt,int **o,int *o_cnt,	str **p,int *p_cnt);

/**
 * Creates and adds a SIP-Number-Auth-Items AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_sip_number_auth_items(AAAMessage *msg,unsigned int data);

/**
 * Creates and adds a SIP-Auth-Data-Item AVP.
 * @param msg - the Diameter message to add to.
 * @param auth_scheme - the value for the authorization scheme AVP
 * @param auth - the value for the authorization AVP
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_sip_auth_data_item_request(AAAMessage *msg, str auth_scheme, str auth, str username, str realm,str method, str server_name);

/**
 * Creates and adds a Server-Name AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_server_name(AAAMessage *msg,str data);

/**
 * Returns the SIP-Number-Auth-Items AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the number or 0 on error
 */
inline int cxdx_get_sip_number_auth_items(AAAMessage *msg, int *data);

/**
 * Returns the Auth-Data-Item from a Diameter answer message.
 * @param msg - the Diameter message
 * @param auth_date - the string to fill with the authorization data
 * @param item_number - the int to fill with the item number
 * @param auth_scheme - the string to fill with the authentication scheme data
 * @param authenticate - the string to fill with the authenticate data
 * @param authorization - the string to fill with the authorization data
 * @param ck - the string to fill with the cipher key
 * @param ik - the string to fill with the integrity key
 * @returns the AVP payload on success or an empty string on error
 */
int cxdx_get_auth_data_item_answer(AAAMessage *msg, AAA_AVP **auth_data,
	int *item_number,str *auth_scheme,str *authenticate,str *authorization,
	str *ck,str *ik,
	str *ip, 
	str *ha1, str *response_auth, str *digest_realm,
	str *line_identifier);

/**
 * Creates and adds a ETSI_sip_authorization AVP.
 * @param username - UserName
 * @param realm - Realm
 * @param nonce - Nonce
 * @param URI - URI
 * @param response - Response
 * @param algoritm - Algorithm
 * @param method - Method
 * @param hash - Enitity-Body-Hash
 * @returns grouped str on success
 */
str cxdx_ETSI_sip_authorization(str username, str realm, str nonce, str URI, str response, str algorithm, str method, str hash);

/**
 * Returns the User-Data from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */

inline str cxdx_get_user_data(AAAMessage *msg);

/**
 * Returns the Charging-Information from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline int cxdx_get_charging_info(AAAMessage *msg,str *ccf1,str *ccf2,str *ecf1,str *ecf2);

/**
 * Creates and adds a Server-Assignment-Type AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_server_assignment_type(AAAMessage *msg,unsigned int data);

/**
 * Creates and adds Userdata-Available AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_userdata_available(AAAMessage *msg,unsigned int data);

/**
 * Finds out the next Public-Identity AVP from a Diameter message.
 * @param msg - the Diameter message
 * @param pos - position to resume search or NULL if to start from the first AVP 
 * @param avp_code - the code of the AVP to look for
 * @param vendor_id - the vendor id of the AVP to look for
 * @param func - the name of the calling function for debugging purposes
 * @returns the AVP payload on success or an empty string on error
 */
inline AAA_AVP* cxdx_get_next_public_identity(AAAMessage *msg,AAA_AVP* pos,int avp_code,int vendor_id,const char *func);

/**
 * Returns the User-Name AVP from a Diameter message.
 * @param msg - the Diameter message
 * @returns the AVP payload on success or an empty string on error
 */
inline str cxdx_get_user_name(AAAMessage *msg);

/**
 * Creates and adds a Result-Code AVP.
 * @param msg - the Diameter message to add to.
 * @param data - the value for the AVP payload
 * @returns 1 on success or 0 on error
 */
inline int cxdx_add_result_code(AAAMessage *msg,unsigned int data);

/**
 * Transactional SIP response - tries to create a transaction if none found.
 * @param msg - message to reply to
 * @param code - the Status-code for the response
 * @param text - the Reason-Phrase for the response
 * @returns the tmb.t_repy() result
 */
int cscf_reply_transactional(struct sip_msg *msg, int code, char *text);


#endif /* IS_CSCF_CXDX_AVP_H */
