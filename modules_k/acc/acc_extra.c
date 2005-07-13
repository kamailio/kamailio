/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2004-10-28  first version (ramona)
 *  2005-05-30  acc_extra patch commited (ramona)
 *  2005-07-13  acc_extra specification moved to use pseudo-variables (bogdan)
 */



#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../dprint.h"
#include "../../ut.h"
#include "../../items.h"
#include "../../mem/mem.h"
#include "acc_extra.h"

#define EQUAL '='
#define SEPARATOR ';'

/* static arrays used for faster and simpler int to str conversion */

/* here we copy the strings returned by int2str (which uses a static buffer) */
static char int_buf[INT2STR_MAX_LEN*MAX_ACC_EXTRA];
/* str's with all extra values; first MAX_ACC_EXTRA elements point into 
 * int_buf[] and are used for int2str vals; second MAX_ACC_EXTRA are just 
 * containers for the normal str vals */
static str  str_buf[2*MAX_ACC_EXTRA];

static char *static_detector = 0;


void init_acc_extra()
{
	int i;
	for (i=0; i<MAX_ACC_EXTRA; i++)
	{
		str_buf[i].s = int_buf + i*INT2STR_MAX_LEN;
		str_buf[i].len = 0;
	}
	for (i=MAX_ACC_EXTRA;i<2*MAX_ACC_EXTRA; i++)
	{
		str_buf[i].s = 0;
		str_buf[i].len = 0;
	}
	/* ugly trick to get the address of the static buffer */
	static_detector = int2str( (unsigned long)3, &i) + i;
}


struct acc_extra *parse_acc_extra(char *extra_str)
{
	struct acc_extra *head;
	struct acc_extra *tail;
	struct acc_extra *extra;
	char *foo;
	char *s;
	int  xl_flags;
	int  n;

	n = 0;
	head = 0;
	extra = 0;
	tail = 0;
	s = extra_str;
	xl_flags = XL_THROW_ERROR | XL_DISABLE_COLORS;

	if (s==0)
	{
		LOG(L_ERR,"ERROR:acc:parse_acc_extra: null string received\n");
		goto error;
	}

	while (*s)
	{
		/* skip white spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s==0)
			goto parse_error;
		if (n==MAX_ACC_EXTRA)
		{
			LOG(L_ERR,"ERROR:acc:parse_acc_extra: too many extras -> please "
				"increase the internal buffer\n");
			goto error;
		}
		extra = (struct acc_extra*)pkg_malloc(sizeof(struct acc_extra));
		if (extra==0)
		{
			LOG(L_ERR,"ERROR:acc:parse_acc_extra: no more pkg mem 1\n");
			goto error;
		}
		memset( extra, 0, sizeof(struct acc_extra));

		/* link the new extra at the end */
		if (tail==0)
		{
			head = extra;
		} else {
			tail->next = extra;
		}
		tail = extra;
		n++;

		/* get name */
		foo = s;
		while (*s && !isspace((int)*s) && EQUAL!=*s)  s++;
		if (*s==0)
			goto parse_error;
		if (*s==EQUAL)
		{
			extra->name.len = (s++) - foo;
		} else {
			extra->name.len = (s++) - foo;
			/* skip spaces */
			while (*s && isspace((int)*s))  s++;
			if (*s!=EQUAL)
				goto parse_error;
			s++;
		}
		extra->name.s = foo;

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;

		/* get value type */
		if ( (foo=xl_parse_spec( s, &extra->spec, xl_flags))==0 )
			goto parse_error;
		s = foo;

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s && (*(s++)!=SEPARATOR || *s==0))
			goto parse_error;
	}

	/* go throught all extras and make the names null terminated */
	for( extra=head ; extra ; extra=extra->next)
		extra->name.s[extra->name.len] = 0;

	return head;
parse_error:
	LOG(L_ERR,"ERROR:acc:parse_acc_extra: parse failed in <%s> "
		"around position %d\n",extra_str, s-extra_str);
error:
	LOG(L_ERR,"acc:parse_acc_extra: error\n");
	destroy_extras(head);
	return 0;
}



void destroy_extras( struct acc_extra *extra)
{
	struct acc_extra *foo;

	while (extra)
	{
		foo = extra;
		extra = extra->next;
		pkg_free(foo);
	}
}


/* extra name is moved as string part of an attribute; str.len will contain an
 * index to the corresponding attribute
 */
int extra2attrs( struct acc_extra *extra, struct attr *attrs, int offset)
{
	int i;

	for(i=0 ; extra && i<MAX_ACC_EXTRA ; i++, extra=extra->next)
	{
		attrs[offset+i].n = extra->name.s;
		extra->name.s =0;
		extra->name.len = offset + i;
	}
	return i;
}


/* converts the name of the extra from str to integer 
 * and stores it over str.len ; str.s is freed and made zero
 */
int extra2int( struct acc_extra *extra )
{
	unsigned int ui;
	int i;

	for( i=0 ; extra&&i<MAX_ACC_EXTRA ; i++,extra=extra->next )
	{
		if (str2int( &extra->name, &ui)!=0)
		{
			LOG(L_ERR,"ERROR:acc:extra2int: <%s> is not number\n",
				extra->name.s);
			return -1;
		}
		pkg_free( extra->name.s );
		extra->name.s = 0;
		extra->name.len = (int)ui;
	}
	return 0;
}



#define set_acc( _n, _name, _vals) \
	do {\
		attr_arr[_n] = _name; \
		val_arr[_n] = _vals; \
		*attr_len += attr_arr[_n].len; \
		*val_len += val_arr[_n]->len; \
		(_n)++; \
	} while(0)

int extra2strar( struct acc_extra *extra, /* extra list to account */
		struct sip_msg *rq, /* accounted message */
		int *attr_len,  /* total length of accounted attribute names */
		int *val_len, /* total length of accounted values */
		str *attr_arr,
		str **val_arr)
{
	str value;
	int    n;
	int    p;
	int    r;

	n = 0;
	p = 0;
	r = MAX_ACC_EXTRA;

	while (extra) 
	{
		/* get the value */
		if (xl_get_spec_value( rq, &extra->spec, &value)!=0)
		{
			LOG(L_ERR,"ERROR:acc:extra2strar: failed to get '%.*s'\n",
				extra->name.len,extra->name.s);
		}

		/* check for overflow */
		if (n==MAX_ACC_EXTRA) 
		{
			LOG(L_WARN,"WARNING:acc:extra2strar: array to short "
				"-> ommiting extras for accounting\n");
			goto done;
		}

		/* set the value into the acc buffer */
		if (value.s+value.len==static_detector)
		{
			memcpy(str_buf[p].s, value.s, value.len);
			str_buf[p].len = value.len;
			set_acc( n, extra->name, &str_buf[p] );
			p++;
		} else {
			str_buf[r] = value;
			set_acc( n, extra->name, &str_buf[r] );
			r++;
		}

		extra = extra->next;
	}

done:
	return n;
}

