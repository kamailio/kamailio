/* 
 * Copyright (C) 2005 iptelorg GmbH
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

#ifndef __XCAP_CLIENT_H
#define __XCAP_CLIENT_H

#include <cds/sstr.h>
#include <xcap/xcap_result_codes.h>

typedef struct {
	/* "prefix" for XCAP query */
	str_t xcap_root;
	/** username for authentication */
	str_t auth_user;
	/** password used for authentication */
	str_t auth_pass;
	/** Accept unverifiable peers (ignore information 
	 * stored in certificate and trust a certificate
	 * without know CA). */
	int enable_unverified_ssl_peer;
} xcap_query_params_t;

typedef enum {
	xcap_doc_pres_rules,
	xcap_doc_im_rules,
	xcap_doc_rls_services,
	xcap_doc_resource_lists
} xcap_document_type_t;

char *xcap_uri_for_users_document(xcap_document_type_t doc_type,
		const str_t *username, 
		const str_t*filename,
		xcap_query_params_t *params);

char *xcap_uri_for_global_document(xcap_document_type_t doc_type,
		const str_t *filename,
		xcap_query_params_t *params);

/** Sends a XCAP query to the destination and using parameters from 
 * query variable a returns received data in output variables buf
 * and bsize. */
/* URI is absolute HTTP/HTTPS uri for the query  */
int xcap_query(const char *uri, xcap_query_params_t *params, 
		char **buf, int *bsize);

typedef int (*xcap_query_func)(const char *uri, 
		xcap_query_params_t *params, 
		char **buf, int *bsize);

void free_xcap_params_content(xcap_query_params_t *params);
int dup_xcap_params(xcap_query_params_t *dst, xcap_query_params_t *src);

/* counts the length for data buffer storing values of
 * xcap parameter members */
int get_inline_xcap_buf_len(xcap_query_params_t *params);

/* copies structure into existing buffer */
int dup_xcap_params_inline(xcap_query_params_t *dst, xcap_query_params_t *src, char *data_buffer);

int str2xcap_params(xcap_query_params_t *dst, const str_t *src);
int xcap_params2str(str_t *dst, xcap_query_params_t *src);

#endif
