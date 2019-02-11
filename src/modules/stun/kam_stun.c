/*
 * Portions Copyright (C) 2013 Crocodile RCS Ltd
 *
 * Based on "ser_stun.c". Copyright (C) 2001-2003 FhG Fokus
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
 * \brief STUN :: Configuration
 * \ingroup stun
 */
/*!
 * \defgroup stun STUN Nat traversal support
 * 
 */

#include <arpa/inet.h>
#include "kam_stun.h"
#include "../../core/forward.h"

/*
 * ****************************************************************************
 *                     Declaration of functions                               *
 * ****************************************************************************
 */
static int stun_parse_header(struct stun_msg* req, USHORT_T* error_code);
static int stun_parse_body(
				struct stun_msg* req,
				struct stun_unknown_att** unknown,
				USHORT_T* error_code);
static void stun_delete_unknown_attrs(struct stun_unknown_att* unknown);
static struct stun_unknown_att* stun_alloc_unknown_attr(USHORT_T type);
static int stun_add_address_attr(struct stun_msg* res, 
						UINT_T		af,
						USHORT_T	port,
						UINT_T*		ip_addr,
						USHORT_T	type,
						int do_xor);
static int add_unknown_attr(struct stun_msg* res, struct stun_unknown_att* unknown);
static int add_error_code(struct stun_msg* res, USHORT_T error_code);
static int copy_str_to_buffer(struct stun_msg* res, const char* data, UINT_T pad);
static int reallock_buffer(struct stun_buffer* buffer, UINT_T len);
static int buf_copy(struct stun_buffer* msg, void* source, UINT_T len);
static void clean_memory(struct stun_msg* req,
				struct stun_msg* res,	struct stun_unknown_att* unknown);
static int stun_create_response(
						struct stun_msg* req,
						struct stun_msg* res,
						struct receive_info* ri,
						struct stun_unknown_att* unknown, 
						UINT_T error_code);
static int stun_add_common_text_attr(struct stun_msg* res, USHORT_T type, char* value, 
							USHORT_T pad);


/*
 * ****************************************************************************
 *                      Definition of functions                               *
 * ****************************************************************************
 */
 
/* 
 * process_stun_msg(): 
 * 			buf - incoming message
 * 			len - length of incoming message
 * 			ri  - information about socket that received a message and 
 *                also information about sender (its IP, port, protocol)
 * 
 * This function ensures processing of incoming message. It's common for both
 * TCP and UDP protocol. There is no other function as an interface.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 * 
 */
int process_stun_msg(char* buf, unsigned len, struct receive_info* ri)
{
	struct stun_msg 			msg_req;
	struct stun_msg 			msg_res;
	struct dest_info			dst;
	struct stun_unknown_att*	unknown;
	USHORT_T					error_code;
	 
	memset(&msg_req, 0, sizeof(msg_req));
	memset(&msg_res, 0, sizeof(msg_res));
	
	msg_req.msg.buf.s = buf;
	msg_req.msg.buf.len = len;	
	unknown = NULL;
	error_code = RESPONSE_OK;
	
	if (stun_parse_header(&msg_req, &error_code) != 0) {
		goto error;
	}
	
	if (error_code == RESPONSE_OK) {
		if (stun_parse_body(&msg_req, &unknown, &error_code) != 0) {
			goto error;
		}
	}
	
	if (stun_create_response(&msg_req, &msg_res, ri,  
							unknown, error_code) != 0) {
		goto error;
	}
	
	init_dst_from_rcv(&dst, ri);

#ifdef EXTRA_DEBUG	
	struct ip_addr ip;
	su2ip_addr(&ip, &dst.to);
	LOG(L_DBG, "DEBUG: process_stun_msg: decoded request from (%s:%d)\n", ip_addr2a(&ip), 
		su_getport(&dst.to));
#endif
	
	/* send STUN response */
	if (msg_send(&dst, msg_res.msg.buf.s, msg_res.msg.buf.len) != 0) {
		goto error;
	}
	
#ifdef EXTRA_DEBUG
	LOG(L_DBG, "DEBUG: process_stun_msg: send response\n");
#endif
	clean_memory(&msg_req, &msg_res, unknown);
	return 0;
	
error:
#ifdef EXTRA_DEBUG
	LOG(L_DBG, "DEBUG: process_stun_msg: failed to decode request\n");
#endif
	clean_memory(&msg_req, &msg_res, unknown);
	return FATAL_ERROR;
}

/*
 * stun_parse_header():
 * 			- req: request from host that should be processed
 * 			- error_code: indication of any protocol error
 * 
 * This function ensures parsing of incoming header.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */

static int stun_parse_header(struct stun_msg* req, USHORT_T* error_code)
{
	
	if (sizeof(req->hdr) > req->msg.buf.len) {
		if(req->msg.buf.len==4 && *((int*)req->msg.buf.s)==0) {
			/* likely the UDP ping 0000 */
			return FATAL_ERROR;
		}
		/* the received message does not contain whole header */
		LM_DBG("incomplete header of STUN message\n");
		/* Any better solution? IMHO it's not possible to send error response
		 * because the transaction ID is not available.
		 */
		return FATAL_ERROR;
	}
	
	memcpy(&req->hdr, req->msg.buf.s, sizeof(struct stun_hdr));
	req->hdr.type = ntohs(req->hdr.type);
	
	/* the SER supports only Binding Request right now */ 
	if (req->hdr.type != BINDING_REQUEST) {
		LOG(L_INFO, "INFO: stun_parse_header: unsupported type of STUN message: %x\n", 
					req->hdr.type);
		/* resending of same message is not welcome */
		*error_code = GLOBAL_FAILURE_ERR;
	}
	
	req->hdr.len = ntohs(req->hdr.len);
	
	/* check if there is correct magic cookie */
	req->old = (req->hdr.id.magic_cookie == htonl(MAGIC_COOKIE)) ? 0 : 1;

#ifdef EXTRA_DEBUG
	LOG(L_DBG, "DEBUG: stun_parse_header: request is old: %i\n", req->old);
#endif
	return 0;
}

/*
 * stun_parse_body():
 * 			- req: request from host that should be processed
 * 			- unknown: this is a link list header of attributes 
 * 					   that are unknown to SER; defaul value is NULL
 * 			- error_code: indication of any protocol error
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int stun_parse_body(
				struct stun_msg* req,
				struct stun_unknown_att** unknown,
				USHORT_T* error_code)
{
	int not_parsed;
	struct stun_attr attr;
	USHORT_T attr_size;
	UINT_T padded_len;
	struct stun_unknown_att*	tmp_unknown;
	struct stun_unknown_att*	body;
	char*	buf;
	
	attr_size = sizeof(struct stun_attr);
	buf = &req->msg.buf.s[sizeof(struct stun_hdr)];
	
	/* 
	 * Mark the body lenght as unparsed.
	 */
	not_parsed = req->msg.buf.len - sizeof(struct stun_hdr);
	
	if (not_parsed != req->hdr.len) {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: stun_parse_body: body too short to be valid\n");
#endif
		*error_code = BAD_REQUEST_ERR;
		return 0; 
	}
	
	tmp_unknown = *unknown;
	body = NULL;
	
	while (not_parsed > 0 && *error_code == RESPONSE_OK) {
		memset(&attr, 0, attr_size);
		
		/* check if there are 4 bytes for attribute type and its value */
		if (not_parsed < 4) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: stun_parse_body: attribute header short to be valid\n");
#endif
			*error_code = BAD_REQUEST_ERR;
			continue;
		}
		
		memcpy(&attr, buf, attr_size);
		
		buf += attr_size;
		not_parsed -= attr_size;
		
		/* check if there is enought unparsed space for attribute's value */
		if (not_parsed < ntohs(attr.len)) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: stun_parse_body: remaining message is shorter then attribute length\n");
#endif
			*error_code = BAD_REQUEST_ERR;
			continue;
		}
		
		/* check if the attribute is known to the server */
		switch (ntohs(attr.type)) {			
			case REALM_ATTR:
			case NONCE_ATTR:
			case MAPPED_ADDRESS_ATTR:
			case XOR_MAPPED_ADDRESS_ATTR:
			case ALTERNATE_SERVER_ATTR:
			case RESPONSE_ADDRESS_ATTR:
			case SOURCE_ADDRESS_ATTR:
			case REFLECTED_FROM_ATTR:		
			case CHANGE_REQUEST_ATTR:
			case CHANGED_ADDRESS_ATTR:
				padded_len = ntohs(attr.len);
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_parse_body: known attributes\n");
#endif
				break;
			
			/* following attributes must be padded to 4 bytes */
			case USERNAME_ATTR:
			case ERROR_CODE_ATTR:
			case UNKNOWN_ATTRIBUTES_ATTR:
			case SOFTWARE_ATTR:
				padded_len = PADDED_TO_FOUR(ntohs(attr.len));
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_parse_body: padded to four\n");
#endif
				break;

			/* MESSAGE_INTEGRITY must be padded to sixty four bytes*/
			case MESSAGE_INTEGRITY_ATTR:
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_parse_body: message integrity attribute found\n");
#endif
				padded_len = PADDED_TO_SIXTYFOUR(ntohs(attr.len));
				break;
			
			case FINGERPRINT_ATTR:
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_parse_body: fingerprint attribute found\n");
#endif
				padded_len = SHA_DIGEST_LENGTH;

				if (not_parsed > SHA_DIGEST_LENGTH) {
#ifdef EXTRA_DEBUG
					LOG(L_DBG, "DEBUG: stun_parse_body: fingerprint is not the last attribute\n");
#endif
					/* fingerprint must be last parameter in request */
					*error_code = BAD_REQUEST_ERR;
					continue;
				}
				break;
			
			default:
				/* 
				 * the attribute is uknnown to the server
				 * let see if it's necessary to generate error response 
				 */
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: low endian: attr - 0x%x   const - 0x%x\n", ntohs(attr.type), MANDATORY_ATTR);
		    LOG(L_DBG, "DEBUG: big endian: attr - 0x%x   const - 0x%x\n", attr.type, htons(MANDATORY_ATTR));
#endif
				if (ntohs(attr.type) <= MANDATORY_ATTR) {
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_parse_body: mandatory unknown attribute found - 0x%x\n", ntohs(attr.type));
#endif		
					tmp_unknown = stun_alloc_unknown_attr(attr.type);
					if (tmp_unknown == NULL) {
						return FATAL_ERROR;
					}
					if (*unknown == NULL) {
						*unknown = body = tmp_unknown;
					} else {
						if(body==NULL) {
							body = *unknown;
						}
						body->next = tmp_unknown;
						body = body->next;
					}
				}
#ifdef EXTRA_DEBUG
        else {
				  LOG(L_DBG, "DEBUG: stun_parse_body: optional unknown attribute found - 0x%x\n", ntohs(attr.type));
        }
#endif
				padded_len = ntohs(attr.len);
				break;
		}
		
		/* check if there is enough unparsed space for the padded attribute
		   (the padded length might be greater then the attribute length)
		 */
		if (not_parsed < padded_len) {
			break;
		}
		buf += padded_len;
		not_parsed -= padded_len;
	}  /* while */
	
	/*
	 * The unknown attribute error code must set after parsing of whole body
	 * because it's necessary to obtain all of unknown attributes! 
	 */
	if (*error_code == RESPONSE_OK && *unknown != NULL) {
		*error_code = UNKNOWN_ATTRIBUTE_ERR;
	} 
	
	return 0;
}

/*
 * stun_create_response():
 * 			- req: original request from host
 * 			- res: this will represent response to host
 * 			- ri: information about request, necessary because of IP 
 * 				  address and port 
 *			- unknown: link list of unknown attributes
 * 			- error_code: indication of any protocol error
 * 
 * The function stun_create_response ensures creating response to a host.
 * The type of response depends on value of error_code parameter.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory  
 */

static int stun_create_response(
						struct stun_msg* req,
						struct stun_msg* res,
						struct receive_info* ri,
						struct stun_unknown_att* unknown, 
						UINT_T error_code)
{
	/*
	 * Alloc some space for response.
	 * Optimalization? - maybe it would be better to use biggish static array.
	 */
	res->msg.buf.s = (char *) pkg_malloc(sizeof(char)*STUN_MSG_LEN);
	if (res->msg.buf.s == NULL) {
		LOG(L_ERR, "ERROR: STUN: out of memory\n");
		return FATAL_ERROR;
	}
	
	memset(res->msg.buf.s, 0, sizeof(char)*STUN_MSG_LEN); 
	res->msg.buf.len = 0;
	res->msg.empty = STUN_MSG_LEN;
	
	/* use magic cookie and transaction ID from request */
	memcpy(&res->hdr.id, &req->hdr.id, sizeof(struct transaction_id));
	/* the correct body length will be added ASAP it will be known */ 
	res->hdr.len = htons(0);
	
	if (error_code == RESPONSE_OK) {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: stun_create_response: creating normal response\n");
#endif
		res->hdr.type = htons(BINDING_RESPONSE);
		
		/* copy header into msg buffer */
		if (buf_copy(&res->msg, (void *) &res->hdr, 
					sizeof(struct stun_hdr)) != 0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: stun_create_response: failed to copy buffer\n");
#endif
			return FATAL_ERROR;
		}

		/* 
		 * If the SER received message in old format, then the body will 
		 * contain MAPPED-ADDRESS attribute instead of XOR-MAPPED-ADDRESS
		 * attribute.
		 */		
		if (req->old == 0) {
			if (stun_add_address_attr(res, ri->src_ip.af, ri->src_port, 
						  ri->src_ip.u.addr32, XOR_MAPPED_ADDRESS_ATTR, 
						  XOR) != 0) {
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_create_response: failed to add address\n");
#endif
				return FATAL_ERROR;
			}
		}
		else {
			if (stun_add_address_attr(res, ri->src_ip.af, ri->src_port, 
						ri->src_ip.u.addr32, MAPPED_ADDRESS_ATTR, !XOR) != 0) {
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_create_response: failed to add address\n");
#endif
				return FATAL_ERROR;
			}
		}
	}
	else {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: stun_create_response: creating error response\n");
#endif
		res->hdr.type = htons(BINDING_ERROR_RESPONSE);
		
		if (buf_copy(&res->msg, (void *) &res->hdr, 
								sizeof(struct stun_hdr)) != 0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: stun_create_response: failed to copy buffer\n");
#endif
			return FATAL_ERROR;
		}
		
		if (add_error_code(res, error_code) != 0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: stun_create_response: failed to add error code\n");
#endif
			return FATAL_ERROR;
		}
		
		if (unknown != NULL) {
			if (add_unknown_attr(res, unknown) != 0) {
#ifdef EXTRA_DEBUG
				LOG(L_DBG, "DEBUG: stun_create_response: failed to add unknown attribute\n");
#endif
				return FATAL_ERROR;
			}
		} 
	}
	
	if (req->old == 0) {
		/* 
		 * add optional information about server; attribute SOFTWARE is part of 
		 * rfc5389.txt
		 * */
		if (stun_add_common_text_attr(res, SOFTWARE_ATTR, SERVER_HDR, PAD4)!=0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: stun_create_response: failed to add common text attribute\n");
#endif
			return FATAL_ERROR;
		}
	}	
	
	res->hdr.len = htons(res->msg.buf.len - sizeof(struct stun_hdr));
	memcpy(&res->msg.buf.s[sizeof(USHORT_T)], (void *) &res->hdr.len,
	       sizeof(USHORT_T));
	
	return 0;
}

/*
 * add_unknown_attr()
 * 			- res: response representation
 * 			- unknown: link list of unknown attributes
 * 
 * The function add_unknown_attr ensures copy of link list of unknown 
 * attributes into response buffer.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 * 
 */
static int add_unknown_attr(struct stun_msg* res, struct stun_unknown_att* unknown)
{
	struct stun_attr attr;
	struct stun_unknown_att* tmp_unknown;
	UINT_T		counter;
	USHORT_T	orig_len;

	counter = 0;
	orig_len = res->msg.buf.len;
	tmp_unknown = unknown;
	
	attr.type = htons(UNKNOWN_ATTRIBUTES_ATTR);
	attr.len = htons(0);
	
	if (buf_copy(&res->msg, (void *) &attr, sizeof(struct stun_attr)) != 0) {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: add_unknown_attr: failed to copy buffer\n");
#endif
		return FATAL_ERROR;
	}
	
	while (tmp_unknown != NULL) {
		if (buf_copy(&res->msg, (void *)&tmp_unknown->type, 
								sizeof(USHORT_T)) != 0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: add_unknown_attr: failed to copy unknown attribute\n");
#endif
			return FATAL_ERROR;
		}
		tmp_unknown = tmp_unknown->next;
		++counter;
	}
	
	attr.len = htons(res->msg.buf.len - orig_len);
	memcpy(&res->msg.buf.s[orig_len], (void *)&attr, sizeof(struct stun_attr));
	
	/* check if there is an odd number of unknown attributes and if yes, 
	 * repeat one of them because of padding to 32
	 */
	if (counter/2 != 0 && unknown != NULL) {
		if (buf_copy(&res->msg, (void *)&unknown->type, sizeof(USHORT_T))!=0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: add_unknown_attr: failed to padd\n");
#endif
			return FATAL_ERROR;
		}
	}	
	
	return 0;
}

/*
 * add_error_code()
 * 			- res: response representation
 * 			- error_code: value of error type
 * 
 * The function add_error_code ensures copy of link list of unknown 
 * attributes into response buffer.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int add_error_code(struct stun_msg* res, USHORT_T error_code)
{
	struct stun_attr attr;
	USHORT_T	orig_len;
	USHORT_T	two_bytes;
	int			text_pad;
	char		err[2];
	
	orig_len = res->msg.buf.len;
	text_pad = 0;
	
	/* the type and length will be copy as last one because of unknown length*/
	if (res->msg.buf.len < sizeof(struct stun_attr)) {
		if (reallock_buffer(&res->msg, sizeof(struct stun_attr)) != 0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: add_error_code: failed to reallocate buffer\n");
#endif
			return FATAL_ERROR;
		}
	}
	res->msg.buf.len += sizeof(struct stun_attr);
	res->msg.empty -= sizeof(struct stun_attr);
	
	/* first two bytes are empty */
	two_bytes = 0x0000;
	
	if (buf_copy(&res->msg, (void *) &two_bytes, sizeof(USHORT_T)) != 0) {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: add_error_code: failed to copy buffer\n");
#endif
		return FATAL_ERROR;
	}
	
	err[0] = error_code / 100;
	err[1] = error_code % 100;
	if (buf_copy(&res->msg, (void *) err, sizeof(UCHAR_T)*2) != 0) {
		return FATAL_ERROR;
	}
	
	switch (error_code) {
		case TRY_ALTERNATE_ERR:
			text_pad = copy_str_to_buffer(res, TRY_ALTERNATE_TXT, PAD4); 
			break;
		case BAD_REQUEST_ERR:
			text_pad = copy_str_to_buffer(res, BAD_REQUEST_TXT, PAD4); 
			break;
		case UNAUTHORIZED_ERR:
			text_pad = copy_str_to_buffer(res, UNAUTHORIZED_TXT, PAD4); 
			break;
		case UNKNOWN_ATTRIBUTE_ERR:
			text_pad = copy_str_to_buffer(res, UNKNOWN_ATTRIBUTE_TXT, PAD4);
			break;
		case STALE_CREDENTIALS_ERR:
			text_pad = copy_str_to_buffer(res, STALE_CREDENTIALS_TXT, PAD4); 
			break;
		case INTEGRITY_CHECK_ERR:
			text_pad = copy_str_to_buffer(res, INTEGRITY_CHECK_TXT, PAD4); 
			break;
		case MISSING_USERNAME_ERR:
			text_pad = copy_str_to_buffer(res, MISSING_USERNAME_TXT, PAD4); 
			break;
		case USE_TLS_ERR:
			text_pad = copy_str_to_buffer(res, USE_TLS_TXT, PAD4); 
			break;
		case MISSING_REALM_ERR:
			text_pad = copy_str_to_buffer(res, MISSING_REALM_TXT, PAD4); 
			break;
		case MISSING_NONCE_ERR:
			text_pad = copy_str_to_buffer(res, MISSING_NONCE_TXT, PAD4); 
			break;
		case UNKNOWN_USERNAME_ERR:
			text_pad = copy_str_to_buffer(res, UNKNOWN_USERNAME_TXT, PAD4); 
			break;
		case STALE_NONCE_ERR:
			text_pad = copy_str_to_buffer(res, STALE_NONCE_TXT, PAD4);
			break;
		case SERVER_ERROR_ERR:
			text_pad = copy_str_to_buffer(res, SERVER_ERROR_TXT, PAD4); 
			break;
		case GLOBAL_FAILURE_ERR:
			text_pad = copy_str_to_buffer(res, GLOBAL_FAILURE_TXT, PAD4); 
			break;
		default:
			LOG(L_ERR, "ERROR: STUN: Unknown error code.\n");
			break;
	}
	if (text_pad < 0) {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: add_error_code: text_pad is negative\n");
#endif
		goto error;
	}
	attr.type = htons(ERROR_CODE_ATTR);
	/* count length of "value" field -> without type and lehgth field */
	attr.len = htons(res->msg.buf.len - orig_len - 
					 text_pad - sizeof(struct stun_attr));
	memcpy(&res->msg.buf.s[orig_len], (void *)&attr, sizeof(struct stun_attr));
	
	return 0;

error:
	return FATAL_ERROR;
}

/*
 * copy_str_to_buffer()
 * 			- res: response representation
 * 			- data: text data, in our case almost text representation of error
 * 			- pad: the size of pad (for how much bytes the string should be 
 * 				   padded
 * 
 * The function copy_str_to_buffer ensures copy of text buffer into response
 * buffer.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int copy_str_to_buffer(struct stun_msg* res, const char* data, UINT_T pad)
{
	USHORT_T	pad_len;
	UINT_T		data_len;
	UCHAR_T		empty[pad];
	
	data_len = strlen(data);
	memset(&empty, 0, pad);
	
	pad_len = (pad - data_len%pad) % pad;
	
	if (buf_copy(&res->msg, (void *) data, sizeof(UCHAR_T)*data_len) != 0) {
#ifdef EXTRA_DEBUG
		LOG(L_DBG, "DEBUG: copy_str_to_buffer: failed to copy buffer\n");
#endif
		return FATAL_ERROR;
	}
	
	if (pad_len != 0) {
		if (buf_copy(&res->msg, &empty, pad_len) != 0) {
#ifdef EXTRA_DEBUG
			LOG(L_DBG, "DEBUG: copy_str_to_buffer: failed to pad\n");
#endif
			return FATAL_ERROR;
		}	
	}

	return pad_len;	
}

/*
 * stun_add_address_attr()
 * 			- res: response representation
 * 			- af: address family
 * 			- port: port
 * 			- ip_addr: represent both IPv4 and IPv6, the differences is in 
 * 			length  
 * 			- type: type of attribute
 * 			- do_xor: if the port should be XOR-ed or not.
 * 
 * The function stun_add_address_attr ensures copy of any IP attribute into
 * response buffer.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int stun_add_address_attr(struct stun_msg* res, 
						UINT_T		af,
						USHORT_T	port,
						UINT_T*		ip_addr,
						USHORT_T	type,
						int do_xor)
{
	struct stun_attr attr;
	int		 ip_struct_len;
	UINT_T	 id[IP_ADDR];
	int i;
	
	ip_struct_len = 0;
	attr.type = htons(type);
	res->ip_addr.port = htons((do_xor) ? (port ^ MAGIC_COOKIE_2B) : port);
	switch(af) {
		case AF_INET:
			ip_struct_len = sizeof(struct stun_ip_addr) - 3*sizeof(UINT_T);
			res->ip_addr.family = htons(IPV4_FAMILY);
			memcpy(res->ip_addr.ip, ip_addr, IPV4_LEN);
			res->ip_addr.ip[0] = (do_xor) ? 
				res->ip_addr.ip[0] ^ htonl(MAGIC_COOKIE) : res->ip_addr.ip[0];
			break;
		case AF_INET6:
			ip_struct_len = sizeof(struct stun_ip_addr);
			res->ip_addr.family = htons(IPV6_FAMILY);
			memcpy(&res->ip_addr.ip, ip_addr, IPV6_LEN);
			memcpy(id, &res->hdr.id, sizeof(struct transaction_id));
			for (i=0; i<IP_ADDR; i++) {
				res->ip_addr.ip[i] = (do_xor) ? 
							res->ip_addr.ip[i] ^ id[i] : res->ip_addr.ip[i];
			}
			break;
		default:
			break;
	}
	
	attr.len = htons(ip_struct_len);
	
	/* copy type and attribute's length */ 
	if (buf_copy(&res->msg, (void *) &attr, sizeof(struct stun_attr)) != 0) {
		return FATAL_ERROR;
	}
	
	/* copy family, port and IP */
	if (buf_copy(&res->msg, (void *) &res->ip_addr, ip_struct_len) != 0) {
		return FATAL_ERROR;
	}

	return 0;
}

/*
 * stun_alloc_unknown_attr()
 * 			- type: type of unknown attribute
 * 
 * The function stun_alloc_unknown_attr ensures allocationg new element for
 * the link list of unknown attributes.
 * 
 * Return value: pointer to new element of link list in positive case
 * 				 NULL if there is some enviroment error such as insufficiency
 * 						of memory
 */
static struct stun_unknown_att* stun_alloc_unknown_attr(USHORT_T type)
{
	struct stun_unknown_att* attr;

	attr = (struct stun_unknown_att *) pkg_malloc(sizeof(struct stun_unknown_att));
	if (attr == NULL) {
		LOG(L_ERR, "ERROR: STUN: out of memory\n");
		return NULL;
	}
	
	attr->type = type;
	attr->next = NULL;
	
	return attr;
}

/*
 * stun_delete_unknown_attrs()
 * 			- unknown: link list of unknown attributes
 * 
 * The function stun_delete_unknown_attrs ensures deleting of link list
 * 
 * Return value: none
 */
static void stun_delete_unknown_attrs(struct stun_unknown_att* unknown)
{
	struct stun_unknown_att* tmp_unknown;
	
	if (unknown == NULL) {
		return;
	}
	
	while(unknown->next) {
		tmp_unknown = unknown->next;
		unknown->next = tmp_unknown->next;
		pkg_free(tmp_unknown);		
	}
	pkg_free(unknown);
}

/*
 * buf_copy()
 * 			- msg: buffer where the data will be copy to
 * 			- source: source data buffer
 * 			- len: number of bytes that should be copied
 * 
 * The function buf_copy copies "len" bytes from source into msg buffer
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int buf_copy(struct stun_buffer* msg, void* source, UINT_T len)
{
	if (msg->empty < len) {
		if (reallock_buffer(msg, len) != 0) {
			return FATAL_ERROR;
		}
	}
	
	memcpy(&msg->buf.s[msg->buf.len], source, len);
	msg->buf.len += len;
	msg->empty -= len;
	
	return 0;
}

/*
 * reallock_buffer()
 * 			- buffer: original buffer
 * 			- len: represents minimum of bytes that must be available after 
 * 					reallocation
 * 
 * The function reallock_buffer reallocks buffer. New buffer's length will be 
 * original length plus bigger from len and STUN_MSG_LEN constant.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int reallock_buffer(struct stun_buffer* buffer, UINT_T len)
{
	char*	tmp_buf;
	UINT_T	new_len;
	
	new_len = (STUN_MSG_LEN < len) ? STUN_MSG_LEN+len : STUN_MSG_LEN;
	
	tmp_buf = (char *) pkg_realloc(buffer->buf.s, 
								   buffer->buf.len + buffer->empty + new_len);
	if (tmp_buf == 0) {
		LOG(L_ERR, "ERROR: STUN: out of memory\n");
		return FATAL_ERROR;
	}
	
	buffer->buf.s = tmp_buf;
	buffer->empty += new_len;

	return 0;
}

/*
 * clean_memory()
 * 			- res: structure representing response message
 * 			- unknown: link list of unknown attributes
 * 
 * The function clean_memory should free dynamic allocated memory.
 * 
 * Return value: none
 */
static void clean_memory(struct stun_msg* req,
				struct stun_msg* res,	struct stun_unknown_att* unknown)
{
#ifdef DYN_BUF
	pkg_free(req->msg.buf.s);
#endif

	if (res->msg.buf.s != NULL) {
		pkg_free(res->msg.buf.s);
	}
	stun_delete_unknown_attrs(unknown);
}

/*
 * stun_add_common_text_attr()
 * 			- res: structure representing response
 * 			- type: type of attribute
 * 			- value: attribute's value
 * 			- pad: size of pad
 * 
 * The function stun_add_common_text_attr copy attribute with string value 
 * into response buffer.
 * 
 * Return value:	0	if there is no environment error
 * 					-1	if there is some enviroment error such as insufficiency
 * 						of memory
 */
static int stun_add_common_text_attr(struct stun_msg* res, 
							  USHORT_T type, 
							  char* value, 
							  USHORT_T pad)
{
	struct stun_attr attr;
	
	if (value == NULL) {
		LOG(L_INFO, "INFO: stun_add_common_text_attr: value is NULL\n");
		return 0;
	}
	
	attr.type = htons(type);
	attr.len = htons(strlen(value));
	
	if (buf_copy(&res->msg, (void *) &attr, sizeof(struct stun_attr)) != 0) {
		return FATAL_ERROR;
	}
	
	if (copy_str_to_buffer(res, value, pad) < 0) {
		return FATAL_ERROR;
	}
	
	return 0;
	
}
