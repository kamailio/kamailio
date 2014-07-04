/*
 * $Id$
 *
 * Copyright (C) 2012 Smile Communications, jason.penton@smilecoms.com
 * Copyright (C) 2012 Smile Communications, richard.good@smilecoms.com
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

#ifndef __IMS_GETTERS_H
#define __IMS_GETTERS_H

#include "../../str.h"

#include "../../parser/contact/parse_contact.h"

/** Return and break the execution of routng script */
#define CSCF_RETURN_BREAK	0 
/** Return true in the routing script */
#define CSCF_RETURN_TRUE	1
/** Return false in the routing script */
#define CSCF_RETURN_FALSE -1
/** Return error in the routing script */
#define CSCF_RETURN_ERROR -2

/** Enumeration for dialog directions */
enum cscf_dialog_direction {
	CSCF_MOBILE_ORIGINATING=0,
	CSCF_MOBILE_TERMINATING=1,
	CSCF_MOBILE_UNKNOWN=2
};

/**
 * Duplicate a str, safely.
 * \Note This checks if:
 *  - src was an empty string
 *  - malloc failed
 * \Note On any error, the dst values are reset for safety
 * \Note A label "out_of_memory" must be defined in the calling function to handle
 * allocation errors. 
 * @param dst - destination str
 * @param src - source src
 * @param mem - type of mem to duplicate into (shm/pkg)
 */
#define str_dup(dst,src,mem) \
do {\
	if ((src).len) {\
		(dst).s = mem##_malloc((src).len);\
		if (!(dst).s){\
			LM_ERR("Error allocating %d bytes in %s!\n",(src).len,#mem);\
			(dst).len = 0;\
			goto out_of_memory;\
		}\
		memcpy((dst).s,(src).s,(src).len);\
		(dst).len = (src).len;\
	}else{\
		(dst).s=0;(dst).len=0;\
	}\
} while (0)

/**
 * Frees a str content.
 * @param x - the str to free
 * @param mem - type of memory that the content is using (shm/pkg)
 */
#define str_free(x,mem) \
do {\
	if ((x).s) mem##_free((x).s);\
	(x).s=0;(x).len=0;\
} while(0)

/**
 * Parses all the contact headers.
 * @param msg - the SIP message
 * @returns the first contact_body
 */
contact_body_t *cscf_parse_contacts(struct sip_msg *msg);
/**
 * Returns the Private Identity extracted from the Authorization header.
 * If none found there takes the SIP URI in To without the "sip:" prefix
 * \todo - remove the fallback case to the To header
 * @param msg - the SIP message
 * @param realm - the realm to match in an Authorization header
 * @returns the str containing the private id, no mem dup
 */
str cscf_get_private_identity(struct sip_msg *msg, str realm);
/**
 * Returns the Public Identity extracted from the To header
 * @param msg - the SIP message
 * @returns the str containing the public id, no mem dup
 */
str cscf_get_public_identity(struct sip_msg *msg);
/**
 * Returns the expires value from the Expires header in the message.
 * It searches into the Expires header and if not found returns -1
 * @param msg - the SIP message, if available
 * @is_shm - msg from from shared memory 
 * @returns the value of the expire or -1 if not found
 */
int cscf_get_expires_hdr(struct sip_msg *msg, int is_shm);
/**
 * Returns the expires value from the message.
 * First it searches into the Expires header and if not found it also looks 
 * into the expires parameter in the contact header
 * @param msg - the SIP message
 * @param is_shm - msg from shared memory
 * @returns the value of the expire or the default 3600 if none found
 */
int cscf_get_max_expires(struct sip_msg *msg, int is_shm);
/**
 * Finds if the message contains the orig parameter in the first Route header
 * @param msg - the SIP message
 * @param str1 - not used
 * @param str2 - not used
 * @returns #CSCF_RETURN_TRUE if yes, else #CSCF_RETURN_FALSE
 */
int cscf_has_originating(struct sip_msg *msg, char *str1, char *str2);
/**
 * Looks for the P-Asserted-Identity header and extracts its content
 * @param msg - the sip message
 * @is_shm - is the message a shm message
 * @returns the asserted identity
 */
str cscf_get_asserted_identity(struct sip_msg *msg, int is_shm);
/**
 * Extracts the realm from a SIP/TEL URI. 
 * - SIP - the hostname
 * - TEL - the phone-context parameter
 * @param msg - the SIP message
 * @returns the realm
 */
str cscf_get_realm_from_uri(str uri);
/** 
 * Delivers the Realm from request URI
 * @param msg sip message 
 * @returns realm as String on success 0 on fail
 */
str cscf_get_realm_from_ruri(struct sip_msg *msg);
/**
 * Get the Public Identity from the Request URI of the message
 * @param msg - the SIP message
 * @returns the public identity
 */
str cscf_get_public_identity_from_requri(struct sip_msg *msg);

/**
 * Get the contact from the Request URI of the message
 * NB: free returned result str when done from shm
 * @param msg - the SIP message
 * @returns the contact (don't forget to free from shm)
 * 
 * NOTE: should only be called when REQ URI has been converted sip:user@IP_ADDRESS:PORT or tel:IP_ADDRESS:PORT
 */
str cscf_get_contact_from_requri(struct sip_msg *msg);

/**
 * Looks for the Call-ID header
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field 
 * @returns the callid value
 */
str cscf_get_call_id(struct sip_msg *msg, struct hdr_field **hr);
/**
 * Check if the contact has an URI parameter with the value "sos",
 * used for detecting an Emergency Registration
 * http://tools.ietf.org/html/draft-patel-ecrit-sos-parameter-0x
 * @param uri - contact uri to be checked
 * @return 1 if found, 0 if not, -1 on error
 */
int cscf_get_sos_uri_param(str uri);
/**
 * Return the P-Visited-Network-ID header
 * @param msg - the SIP message
 * @returns the str with the header's body
 */
str cscf_get_visited_network_id(struct sip_msg *msg, struct hdr_field **h);
/**
 * Adds a header to the message as the first one in the message
 * @param msg - the message to add a header to
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int cscf_add_header_first(struct sip_msg *msg, str *hdr, int type);

/**
 * Returns the next header structure for a given header name.
 * @param msg - the SIP message to look into
 * @param header_name - the name of the header to search for
 * @param last_header - last header to ignore in the search, or NULL if to start from the first one
 * @returns the hdr_field on success or NULL if not found  
 */
struct hdr_field* cscf_get_next_header(struct sip_msg * msg,
        /**
         * Looks for the First Via header and returns its body.
         * @param msg - the SIP message
         * @param h - the hdr_field to fill with the result
         * @returns the first via_body
         */ str header_name, struct hdr_field* last_header);
struct via_body* cscf_get_first_via(struct sip_msg *msg, struct hdr_field **h);
/**
 * Looks for the Last Via header and returns it.
 * @param msg - the SIP message
 * @returns the last via body body
 */
struct via_body* cscf_get_last_via(struct sip_msg *msg);
/**
 * Looks for the UE Via in First Via header if its a request
 * or in the last if its a response and returns its body
 * @param msg - the SIP message
 * @returns the via of the UE
 */
struct via_body* cscf_get_ue_via(struct sip_msg *msg);
/**
 * Looks for the WWW-Authenticate header and returns its body.
 * @param msg - the SIP message
 * @param h - the hdr_field to fill with the result
 * @returns the www-authenticate body
 */
str cscf_get_authenticate(struct sip_msg *msg, struct hdr_field **h);
/**
 * Adds a header to the message
 * @param msg - the message to add a header to
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int cscf_add_header(struct sip_msg *msg, str *hdr, int type);
/**
 *	Get the expires header value from a message. 
 * @param msg - the SIP message
 * @returns the expires value or -1 if not found
 */
int cscf_get_expires(struct sip_msg *msg);
/**
 * Check if the message is an initial request for a dialog. 
 *		- BYE, PRACK, UPDATE, NOTIFY belong to an already existing dialog
 * @param msg - the message to check
 * @returns 1 if initial, 0 if not
 */
int cscf_is_initial_request(struct sip_msg *msg);

/**
 *	Get the public identity from P-Asserted-Identity, or From if asserted not found.
 * @param msg - the SIP message
 * @param uri - uri to fill into
 * @returns 1 if found, 0 if not
 */
int cscf_get_originating_user(struct sip_msg * msg, str *uri);

/**
 *	Get public identity from Request-URI for terminating.
 * returns in uri the freshly pkg allocated uri - don't forget to free
 * @param msg - the SIP message
 * @param uri - uri to fill into
 * @returns 1 if found, else 0 
 */
int cscf_get_terminating_user(struct sip_msg * msg, str *uri);

/**
 * Return the P-Access-Network-Info header
 * @param msg - the SIP message
 * @returns the str with the header's body
 */

str cscf_get_access_network_info(struct sip_msg *msg, struct hdr_field **h);

/**
 * Return the P-Charging-Vector header
 * @param msg - the SIP message
 * @returns the str with the header's body
 */

str cscf_get_charging_vector(struct sip_msg *msg, struct hdr_field **h);

/**
 * Return the P-Charging-Vector tokens
 * @param msg - the SIP message
 * @returns the str with icid, orig_ioi and term_ioi
 */
int cscf_get_p_charging_vector(struct sip_msg *msg, str * icid, str * orig_ioi,
	str * term_ioi);


/**
 * Get the to tag
 * @param msg  - the SIP Message to look into
 * @param tag - the pointer to the tag to write to
 * @returns 0 on error or 1 on success
 */
int cscf_get_to_tag(struct sip_msg* msg, str* tag);

/**
 * Get the from tag
 * @param msg - the SIP message to look into
 * @param tag - the pointer to the tag to write to
 * @returns 0 on error or 1 on success
 */
int cscf_get_from_tag(struct sip_msg* msg, str* tag);



/**
 * Get the local uri from the From header.
 * @param msg - the message to look into
 * @param local_uri - ptr to fill with the value
 * @returns 1 on success or 0 on error
 */
int cscf_get_from_uri(struct sip_msg* msg, str *local_uri);

/**
 * Get the local uri from the To header.
 * @param msg - the message to look into
 * @param local_uri - ptr to fill with the value
 * @returns 1 on success or 0 on error
 */
int cscf_get_to_uri(struct sip_msg* msg, str *local_uri);

/**
 * Looks for the Event header and extracts its content.
 * @param msg - the sip message
 * @returns the string event value or an empty string if none found
 */
str cscf_get_event(struct sip_msg *msg);

/*! \brief
 * Check if the originating REGISTER message was formed correctly
 * The whole message must be parsed before calling the function
 * _s indicates whether the contact was star
 */
int cscf_check_contacts(struct sip_msg* _m, int* _s);

/*! \brief
 * parse all the messages required by the registrar
 */
int cscf_parse_message_for_register(struct sip_msg* _m);

/**
 * Returns the content of the P-Associated-URI header
 * Public_id is pkg_alloced and should be later freed.
 * Inside values are not duplicated.
 * @param msg - the SIP message to look into
 * @param public_id - array to be allocated and filled with the result
 * @param public_id_cnt - the size of the public_id array
 * @param is_shm - msg from shared memory
 * @returns 1 on success or 0 on error
 */
int cscf_get_p_associated_uri(struct sip_msg *msg,str **public_id,int *public_id_cnt, int is_shm);

/**
 * Looks for the realm parameter in the Authorization header and returns its value.
 * @param msg - the SIP message
 * @returns the realm
 */
str cscf_get_realm(struct sip_msg *msg);

/**
 * Returns the content of the Service-Route header.
 * data vector is pkg_alloced and should be later freed
 * inside values are not duplicated
 * @param msg - the SIP message
 * @param size - size of the returned vector, filled with the result
 * @param is_shm - msg from shared memory
 * @returns - the str vector of uris
 */
str* cscf_get_service_route(struct sip_msg *msg, int *size, int is_shm);

/**
 * Returns the s_dialog_direction from the direction string.
 * @param direction - "orig" or "term"
 * @returns the s_dialog_direction if ok or #DLG_MOBILE_UNKNOWN if not found
 */
enum cscf_dialog_direction cscf_get_dialog_direction(char *direction);

long cscf_get_content_length (struct sip_msg* msg);

/**
 * Looks for the Contact header and extracts its content
 * @param msg - the sip message
 * @returns the first contact in the message
 */
str cscf_get_contact(struct sip_msg *msg);

/**
 * Adds a header to the reply message
 * @param msg - the request to add a header to its reply
 * @param content - the str containing the new header
 * @returns 1 on succes, 0 on failure
 */
int cscf_add_header_rpl(struct sip_msg *msg, str *hdr);

/**
 * Looks for the Call-ID header
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field 
 * @returns the callid value
 */
int cscf_get_cseq(struct sip_msg *msg,struct hdr_field **hr);

/**
 * Looks for the P-Called-Party-ID header and extracts the public identity from it
 * @param msg - the sip message
 * @param hr - ptr to return the found hdr_field 
 * @returns the P-Called_Party-ID
 */
str cscf_get_public_identity_from_called_party_id(struct sip_msg *msg,struct hdr_field **hr);

#endif

