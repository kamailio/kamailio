/*
 * $Id$ 
 *
 * Copyright (c) 2007 iptelorg GmbH
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

/*
 * Converts a RFC 1123 formatted date string to stuct tm
 */
int rfc1123totm (char *stime, struct tm *ttm ) {
	char *ptime = stime;
	unsigned int uval;
	int ires;

	int char2int (char *p, int *t){
		if (*p < '0' || *p > '9' || *(p + 1) < '0' || *(p + 1) > '9') return -1;
		*t = (*p - '0')*10 + *(p + 1) - '0';

		return 0;
	}

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
	if (memcmp(ptime," GMT", strlen(" GMT"))) return -15;

	return 0;
}

char* parse_date(char *buffer, char *end, struct date_body *db)
{
	char *p;
	int i1;

	db->error=PARSE_ERROR;
	p = buffer;

	/* check whether enough characters are available */
	for (i1 = 0; i1 < RFC1123DATELENGTH || p[i1] == '\n' || p + i1 >= end;i1++);
	if (i1 < RFC1123DATELENGTH)
		goto error;

	if (rfc1123totm(buffer,&db->date))
		goto error;

	p+=RFC1123DATELENGTH;

	p=eat_lws_end(p, end);
	/*check if the header ends here*/
	if (p>=end) {
		LOG(L_ERR, "ERROR: parse_date: strange EoHF\n");
		goto error;
	}
	if (*p=='\r' && p+1<end && *(p+1)=='\n') {
		db->error=PARSE_OK;
		return p+2;
	}
	if (*p=='\n') {
		db->error=PARSE_OK;
		return p+1;
	}
	LOG(L_ERR, "ERROR: Date EoL expected\n");
error:
	LOG(L_ERR,"ERROR: parse_date: parse error\n");
	return p;
}


void free_date(struct date_body *db)
{
	pkg_free(db);
}
