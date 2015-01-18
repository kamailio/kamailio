/*
 * Copyright (c) 2007 iptelorg GmbH
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

/*! \file
 * \brief Parser :: Date header
 *
 * \ingroup parser
 */



#include <string.h>
#include "parse_date.h"
#include "parse_def.h"
#include "parser_f.h"  /* eat_space_end and so on */
#include "../mem/mem.h"

/*
 * Parse Date header field
 */

#define READ(val) \
(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))

inline static int char2int (char *p, int *t)
{
	if (*p < '0' || *p > '9' || *(p + 1) < '0' || *(p + 1) > '9')
		return -1;
	*t = (*p - '0')*10 + *(p + 1) - '0';

	return 0;
}

/*! \brief
 * Converts a RFC 1123 formatted date string to stuct tm
 */
static int rfc1123totm (char *stime, struct tm *ttm ) {
	char *ptime = stime;
	unsigned int uval;
	int ires;

	uval = READ(ptime);
	ptime+=4;
	switch (uval) {
		/* Sun, */
		case 0x2c6e7553: ttm->tm_wday = 0; break;
		/* Mon, */
		case 0x2c6e6f4d: ttm->tm_wday = 1; break;
		/* Tue, */
		case 0x2c657554: ttm->tm_wday = 2; break;
		/* Wed, */
		case 0x2c646557: ttm->tm_wday = 3; break;
		/* Thu, */
		case 0x2c756854: ttm->tm_wday = 4; break;
		/* Fri, */
		case 0x2c697246: ttm->tm_wday = 5; break;
		/* Sat, */
		case 0x2c746153: ttm->tm_wday = 6; break;
		default: return -2;
	}

	if (*(ptime++)!=' ') return -3;


	if (char2int(ptime,&ttm->tm_mday) || ttm->tm_mday > 31) return -4;
	ptime+=2;

	if (*(ptime++) != ' ') return -5;

	uval = READ(ptime);
	ptime+=4;
	switch (uval) {
		/* Jan, */
		case 0x206e614a: ttm->tm_mon = 0; break;
		/* Feb, */
		case 0x20626546: ttm->tm_mon = 1; break;
		/* Mar, */
		case 0x2072614d: ttm->tm_mon = 2; break;
		/* Apr, */
		case 0x20727041: ttm->tm_mon = 3; break;
		/* May, */
		case 0x2079614d: ttm->tm_mon = 4; break;
		/* Jun, */
		case 0x206e754a: ttm->tm_mon = 5; break;
		/* Jul, */
		case 0x206c754a: ttm->tm_mon = 6; break;
		/* Aug, */
		case 0x20677541: ttm->tm_mon = 7; break;
		/* Sep, */
		case 0x20706553: ttm->tm_mon = 8; break;
		/* Oct, */
		case 0x2074634f: ttm->tm_mon = 9; break;
		/* Nov, */
		case 0x20766f4e: ttm->tm_mon = 10; break;
		/* Dec, */
		case 0x20636544: ttm->tm_mon = 11; break;
		default: return -6;
	}

	if (char2int(ptime,&ires)) return -7;
	ptime+=2;
	if (char2int(ptime,&ttm->tm_year)) return -8;
	ptime+=2;
	ttm->tm_year+=(ires-19)*100;

	if (*(ptime++) != ' ') return -9;

	if (char2int(ptime,&ttm->tm_hour) || ttm->tm_hour > 23) return -10;
	ptime+=2;
	if (*(ptime++) != ':') return -11;

	if (char2int(ptime,&ttm->tm_min) || ttm->tm_min > 59) return -12;
	ptime+=2;
	if (*(ptime++) != ':') return -13;

	if (char2int(ptime,&ttm->tm_sec) || ttm->tm_sec > 59) return -14;
	ptime+=2;

	/* " GMT" */
	uval = READ(ptime);
	if ((uval | 0x20202020) != 0x746d6720) return -15;

	return 0;
}

void parse_date(char *buffer, char *end, struct date_body *db)
{
	db->error=PARSE_ERROR;

	/* check whether enough characters are available */
	if (end - buffer < RFC1123DATELENGTH)
		goto error;

	if (rfc1123totm(buffer,&db->date))
		goto error;

	db->error=PARSE_OK;
	return ;
error:
	LOG(L_ERR,"ERROR: parse_date: parse error\n");
	return ;
}

int parse_date_header(struct sip_msg *msg)
{
	struct date_body* date_b;


	if ( !msg->date && (parse_headers(msg,HDR_DATE_F,0)==-1 || !msg->date) ) {
		LOG(L_ERR,"ERROR:parse_date_header: bad msg or missing DATE header\n");
		goto error;
	}

	/* maybe the header is already parsed! */
	if (msg->date->parsed)
		return 0;

	date_b=pkg_malloc(sizeof(*date_b));
	if (date_b==0){
		LOG(L_ERR, "ERROR:parse_date_header: out of memory\n");
		goto error;
	}
	memset(date_b, 0, sizeof(*date_b));

	parse_date(msg->date->body.s,
			   msg->date->body.s + msg->date->body.len+1,
			   date_b);
	if (date_b->error==PARSE_ERROR){
		free_date(date_b);
		goto error;
	}
	msg->date->parsed=(void*)date_b;

	return 0;
error:
	return -1;
}

void free_date(struct date_body *db)
{
	pkg_free(db);
}
