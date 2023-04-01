/*
 * lost module LoST response parsing functions
 *
 * Copyright (C) 2022 Wolfgang Kampichler
 * DEC112, FREQUENTIS AG
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
 * \brief Kamailio lost :: response
 * \ingroup lost
 * Module: \ref lost
 */

#ifndef LOST_RESPONSE_H
#define LOST_RESPONSE_H

#define PROP_LANG (const char *)"lang"
#define PROP_MSG (const char *)"message"

#define ROOT_NODE (const char *)"findServiceResponse"
#define DISPNAME_NODE (const char *)"displayName"
#define SERVICE_NODE (const char *)"service"
#define SERVICENR_NODE (const char *)"serviceNumber"
#define WARNINGS_NODE (const char *)"warnings"
#define PATH_NODE (const char *)"via"
#define PATH_NODE_VIA (const char *)"via"
#define MAPP_NODE (const char *)"mapping"
#define MAPP_NODE_URI (const char *)"uri"
#define MAPP_PROP_EXP (const char *)"expires"
#define MAPP_PROP_LUP (const char *)"lastUpdated"
#define MAPP_PROP_SRC (const char *)"source"
#define MAPP_PROP_SID (const char *)"sourceId"

#define RED_NODE (const char *)"redirect"
#define RED_PROP_TAR (const char *)"target"
#define RED_PROP_SRC (const char *)"source"
#define RED_PROP_MSG (const char *)"message"

#define ERRORS_NODE (const char *)"errors"

#define SIP_S (const char *)"sip:"
#define SIPS_S (const char *)"sips:"

#define HELD_RESPONSE_REFERENCE 1
#define HELD_RESPONSE_VALUE 2

typedef struct lost_list
{
	char *value;
	struct lost_list *next;
} s_lost_list_t, *p_lost_list_t;

typedef struct lost_info
{
	char *text;
	char *lang;
} s_lost_info_t, *p_lost_info_t;

typedef struct lost_data
{
	char *expires;
	char *updated;
	char *source;
	char *sourceid;
	char *urn;
	char *number;
	p_lost_info_t name;
} s_lost_data_t, *p_lost_data_t;

typedef struct lost_type
{
	char *type;
	char *target;
	char *source;
	p_lost_info_t info;
} s_lost_type_t, *p_lost_type_t;

typedef struct lost_issue
{
	p_lost_type_t issue;
	struct lost_issue *next;
} s_lost_issue_t, *p_lost_issue_t;

typedef enum lost_cat
{
	RESPONSE,
	ERROR,
	REDIRECT,
	OTHER = -1
} lost_cat_t;

typedef struct lost_fsr
{
	lost_cat_t category;
	p_lost_data_t mapping;
	p_lost_issue_t warnings;
	p_lost_issue_t errors;
	p_lost_type_t redirect;
	p_lost_list_t path;
	p_lost_list_t uri;
} s_lost_fsr_t, *p_lost_fsr_t;

/* read and parse response data */
p_lost_fsr_t lost_parse_findServiceResponse(str);
/* check response to dereferece request */
int lost_check_HeldResponse(xmlNodePtr);
/* appends value to list objects */
int lost_append_response_list(p_lost_list_t *, str);
/* print the response */
void lost_print_findServiceResponse(p_lost_fsr_t);
/* remove response data from memory */
void lost_free_findServiceResponse(p_lost_fsr_t *);

/* uri scheme parsing */
int is_urn(char *);
int is_cid(char *);
int is_http(char *);
int is_https(char *);
int is_cid_laquot(char *);
int is_http_laquot(char *);
int is_https_laquot(char *);

/* list search */
int lost_search_response_list(p_lost_list_t *, char **, const char *);

#endif
