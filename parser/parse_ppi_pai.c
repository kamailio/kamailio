/*
 * Copyright (C) 2013 Hugh Waite
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

/** Parser :: Parse P-Asserted-Identity: header.
 * @file
 * @ingroup parser
 */

#include "parse_ppi_pai.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "msg_parser.h"
#include "parse_uri.h"
#include "../ut.h"
#include "../mem/mem.h"

/* 
 * A P-Asserted-Identity or P-Preferred-Identity header value is an addr-spec or name-addr
 * There can be only one of any URI scheme (sip(s), tel etc), which may occur on separate
 * headers or can be comma separated in a single header.
 * RFC3325 only mentions sip(s) and tel schemes, but there is no reason why other schemes
 * cannot be used in the future.
 */

/*!
 *
 */
#define NUM_PAI_BODIES 10
int parse_pai_ppi_body(char *buf, int len, p_id_body_t **body)
{
	static to_body_t uri_b[NUM_PAI_BODIES]; /* Temporary storage */
	int num_uri = 0;
	char *tmp;
	int i;
	memset(uri_b, 0, NUM_PAI_BODIES * sizeof(to_body_t));

	tmp = parse_addr_spec(buf, buf+len, &uri_b[num_uri], 1);
	if (uri_b[num_uri].error == PARSE_ERROR)
	{
		LM_ERR("Error parsing PAI/PPI body %u '%.*s'\n", num_uri, len, buf);
		return -1;
	}
	/* should be no header params, but in case there are, free them */
	free_to_params(&uri_b[num_uri]);
	num_uri++;
	while ((*tmp == ',') && (num_uri < NUM_PAI_BODIES))
	{
		tmp++;
		tmp = parse_addr_spec(tmp, buf+len, &uri_b[num_uri], 1);
		if (uri_b[num_uri].error == PARSE_ERROR)
		{
			LM_ERR("Error parsing PAI/PPI body %u '%.*s'\n", num_uri, len, buf);
			return -1;
		}
		/* should be no header params, but in case there are, free them */
		free_to_params(&uri_b[num_uri]);
		num_uri++;
	}
	if (num_uri >= NUM_PAI_BODIES)
	{
		LM_WARN("Too many bodies in PAI/PPI header '%.*s'\n", len, buf);
		LM_WARN("Ignoring bodies beyond %u\n", NUM_PAI_BODIES);
	}
	*body = pkg_malloc(sizeof(p_id_body_t) + num_uri * sizeof(to_body_t));
	if (*body == NULL)
	{
		LM_ERR("No pkg memory for pai/ppi body\n");
		return -1;
	}
	memset(*body, 0, sizeof(p_id_body_t));
	(*body)->id = (to_body_t*)((char*)(*body) + sizeof(p_id_body_t));
	(*body)->num_ids = num_uri;
	for (i=0; i< num_uri; i++)
	{
		memcpy(&(*body)->id[i], &uri_b[i], sizeof(to_body_t));
	}
	return 0;
}

int free_pai_ppi_body(p_id_body_t *pid_b)
{
	if (pid_b != NULL)
	{
		pkg_free(pid_b);
	}
	return 0;
}

/*!
 * \brief Parse all P-Asserted-Identity headers
 * \param msg The SIP message structure
 * \return 0 on success, -1 on failure
 */
int parse_pai_header(struct sip_msg* const msg)
{
	p_id_body_t *pai_b;
	p_id_body_t **prev_pid_b;
	hdr_field_t *hf;
	void **vp;

	if ( !msg->pai )
	{
		if (parse_headers(msg, HDR_PAI_F, 0) < 0)
		{
			LM_ERR("Error parsing PAI header\n");
			return -1;
		}
		if ( !msg->pai )
			/* No PAI headers */
			return -1;
	}

	if ( msg->pai->parsed )
		return 0;

	vp = &msg->pai->parsed;
	prev_pid_b = (p_id_body_t**)vp;

	for (hf = msg->pai; hf != NULL; hf = next_sibling_hdr(hf))
	{
		if (parse_pai_ppi_body(hf->body.s, hf->body.len, &pai_b) < 0)
		{
			return -1;
		}
		hf->parsed = (void*)pai_b;
		*prev_pid_b = pai_b;
		prev_pid_b = &pai_b->next;

		if (parse_headers(msg, HDR_PAI_F, 1) < 0)
		{
			LM_ERR("Error looking for subsequent PAI header");
			return -1;
		}
	}
	return 0;
}

/*!
 * \brief Parse all P-Preferred-Identity headers
 * \param msg The SIP message structure
 * \return 0 on success, -1 on failure
 */
int parse_ppi_header(struct sip_msg* const msg)
{
	p_id_body_t *ppi_b, *prev_pidb;
	hdr_field_t *hf;

	if ( !msg->ppi )
	{
		if (parse_headers(msg, HDR_PPI_F, 0) < 0)
		{
			LM_ERR("Error parsing PPI header\n");
			return -1;
		}
		if ( !msg->ppi )
			/* No PPI headers */
			return -1;
	}

	if ( msg->ppi->parsed )
		return 0;

	if (parse_pai_ppi_body(msg->ppi->body.s, msg->ppi->body.len, &ppi_b) < 0)
	{
		return -1;
	}
	msg->ppi->parsed = (void*)ppi_b;

	if (parse_headers(msg, HDR_PPI_F, 1) < 0)
	{
		LM_ERR("Error looking for subsequent PPI header");
		return -1;
	}
	prev_pidb = ppi_b;
	hf = msg->ppi;

	if ((hf = next_sibling_hdr(hf)) != NULL)
	{
		if (parse_pai_ppi_body(hf->body.s, hf->body.len, &ppi_b) < 0)
		{
			return -1;
		}
		hf->parsed = (void*)ppi_b;

		if (parse_headers(msg, HDR_PPI_F, 1) < 0)
		{
			LM_ERR("Error looking for subsequent PPI header");
			return -1;
		}
		prev_pidb->next = ppi_b;
		prev_pidb = ppi_b;
	}
	return 0;
}

