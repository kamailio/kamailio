/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 *
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*! \file
 * \brief Parser :: Diversion header
 *
 * \ingroup parser
 */


#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "../ut.h"
#include "../mem/mem.h"
#include "parse_diversion.h"
#include "parse_from.h"
#include "parse_to.h"
#include "msg_parser.h"

/*! \brief
 * This method is used to parse DIVERSION header.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 *
 * limitations: it parses only the first occurrence
 */
#define NUM_DIVERSION_BODIES 10
int parse_diversion_body(char *buf, int len, diversion_body_t **body)
{
	static to_body_t uri_b[NUM_DIVERSION_BODIES]; /* Temporary storage */
	int num_uri = 0;
	int body_len = 0;
	char *tmp;
	int i;
	to_param_t *params;

	memset(uri_b, 0, NUM_DIVERSION_BODIES * sizeof(to_body_t));

	tmp = parse_addr_spec(buf, buf + len, &uri_b[num_uri], 1);
	if(uri_b[num_uri].error == PARSE_ERROR) {
		LM_ERR("Error parsing Diversion body %u '%.*s'\n", num_uri, len, buf);
		return -1;
	}

	/* id.body should contain all info including uri and params */
	body_len = uri_b[num_uri].body.len;

	/* Loop over all params */
	params = uri_b[num_uri].param_lst;
	while(params) {
		body_len +=
				params->name.len + params->value.len + 2; // 2 for '=' and ';'
		params = params->next;
	}

	uri_b[num_uri].body.len = body_len;

	num_uri++;
	while(*tmp == ',' && (num_uri < NUM_DIVERSION_BODIES)) {
		tmp++;
		while(tmp < buf + len && (*tmp == ' ' || *tmp == '\t'))
			tmp++;

		if(tmp >= buf + len) {
			LM_ERR("no content after comma when parsing Diversion body %u "
				   "'%.*s'\n",
					num_uri, len, buf);
			// Free params already allocated
			while(num_uri >= 0) {
				free_to_params(&uri_b[num_uri]);
				num_uri--;
			}
			return -1;
		}

		if((tmp < buf + len - 1 && *tmp == '\n')
				|| (tmp < buf + len - 2 && *tmp == '\r'
						&& *(tmp + 1) == '\n')) {
			if(*tmp == '\n') {
				tmp++;
			} else {
				tmp += 2;
			}
			if(*tmp != ' ' && *tmp != '\t') {
				// TODO: Check if this is the correct error message
				LM_ERR("no space after EOL when parsing Diversion body %u "
					   "'%.*s'\n",
						num_uri, len, buf);
				// Free params already allocated
				while(num_uri >= 0) {
					free_to_params(&uri_b[num_uri]);
					num_uri--;
				}
				return -1;
			}
			tmp++;
		}
		/* Parse next body */
		tmp = parse_addr_spec(tmp, buf + len, &uri_b[num_uri], 1);
		if(uri_b[num_uri].error == PARSE_ERROR) {
			LM_ERR("Error parsing Diversion body %u '%.*s'\n", num_uri, len,
					buf);
			// Free params already allocated
			while(num_uri >= 0) {
				free_to_params(&uri_b[num_uri]);
				num_uri--;
			}
			return -1;
		}

		/* id.body should contain all info including uri and params */
		body_len = uri_b[num_uri].body.len;

		/* Loop over all params */
		params = uri_b[num_uri].param_lst;
		while(params) {
			body_len += params->name.len + params->value.len
						+ 2; /*  2 for '=' and ';' */
			params = params->next;
		}

		uri_b[num_uri].body.len = body_len;

		num_uri++;
	}
	if(num_uri >= NUM_DIVERSION_BODIES) {
		LM_WARN("Too many bodies in Diversion header '%.*s'\n", len, buf);
		LM_WARN("Ignoring bodies beyond %u\n", NUM_DIVERSION_BODIES);
	}
	*body = pkg_malloc(sizeof(diversion_body_t) + num_uri * sizeof(to_body_t));
	if(*body == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(*body, 0, sizeof(diversion_body_t));
	(*body)->id = (to_body_t *)((char *)(*body) + sizeof(diversion_body_t));
	(*body)->num_ids = num_uri;
	for(i = 0; i < num_uri; i++) {
		memcpy(&(*body)->id[i], &uri_b[i], sizeof(to_body_t));
	}
	return 0;
}

int parse_diversion_header(struct sip_msg *msg)
{
	diversion_body_t *diversion_b;
	diversion_body_t **prev_diversion_body;
	hdr_field_t *hf;
	void **vp;

	if(!msg->diversion) {
		if(parse_headers(msg, HDR_DIVERSION_F, 0) < 0) {
			LM_ERR("Error parsing Diversion header\n");
			return -1;
		}

		if(!msg->diversion) {
			/* Diversion header not found */
			LM_DBG("Diversion header not found\n");
			return -1;
		}
	}
	/* maybe the header is already parsed! */
	if(msg->diversion->parsed)
		return 0;

	vp = &msg->diversion->parsed;
	/* 	Set it as the first header in the list */
	prev_diversion_body = (diversion_body_t **)vp;

	/* Loop through all the Diversion headers */
	for(hf = msg->diversion; hf != NULL; hf = next_sibling_hdr(hf)) {
		if(parse_diversion_body(hf->body.s, hf->body.len, &diversion_b) < 0) {
			LM_ERR("Error parsing Diversion header\n");
			return -1;
		}

		hf->parsed = (void *)diversion_b;
		*prev_diversion_body = diversion_b;
		prev_diversion_body = &diversion_b->next;

		if(parse_headers(msg, HDR_DIVERSION_F, 1) < 0) {
			LM_ERR("Error looking for subsequent Diversion header\n");
			return -1;
		}
	}
	return 0;
}

int free_diversion_body(diversion_body_t *div_b)
{
	int i;

	if(div_b == NULL) {
		return -1;
	}
	for(i = 0; i < div_b->num_ids; i++) {
		/* Free to_body pointer parameters */
		if(div_b->id[i].param_lst) {
			free_to_params(&(div_b->id[i]));
		}
	}
	pkg_free(div_b);

	return 0;
}

/*! \brief
 * Get the value of a given diversion parameter
 */
str *get_diversion_param(struct sip_msg *msg, str *name)
{
	struct to_param *params;

	if(parse_diversion_header(msg) < 0) {
		LM_ERR("could not get diversion parameter\n");
		return 0;
	}

	to_body_t *diversion =
			get_diversion(msg)->id; /* This returns the first entry */
	params = diversion->param_lst;

	while(params) {
		if((params->name.len == name->len)
				&& (strncmp(params->name.s, name->s, name->len) == 0)) {
			return &params->value;
		}
		params = params->next;
	}

	return 0;
}
