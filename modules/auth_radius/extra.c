/*
 * extra.c Handling of extra attributes (adapted from acc module)
 *
 * Copyright (C) 2004-2006 Voice Sistem SRL
 * Copyright (C) 2008 Juha Heinanen <jh@tutpro.com>
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

#include <string.h>
#include <ctype.h>
#include "../../mem/mem.h"
#include "../../ut.h"
#include "extra.h"


#define EQUAL '='
#define SEPARATOR ';'


/* here we copy the strings returned by int2str (which uses a static buffer) */
static char int_buf[INT2STR_MAX_LEN*MAX_EXTRA];
static char *static_detector = 0;


/* Initialize extra engine */
void init_extra_engine(void)
{
	int i;
	/* ugly trick to get the address of the static buffer */
	static_detector = int2str( (unsigned long)3, &i) + i;
}


/*
 * Parse extra module parameter value to extra_attr list, where each
 * element contains name of attribute and pseudo variable specification.
 */
struct extra_attr *parse_extra_str(char *extra_str)
{
    struct extra_attr *head;
    struct extra_attr *tail;
    struct extra_attr *extra;
    char *foo;
    char *s;
    int  n;
    str stmp;

    n = 0;
    head = 0;
    extra = 0;
    tail = 0;
    s = extra_str;

    if (s==0) {
	LM_ERR("null string received\n");
	goto error;
    }

    while (*s) {
	/* skip white spaces */
	while (*s && isspace((int)*s))  s++;
	if (*s == 0) goto parse_error;
	if (n == MAX_EXTRA) {
	    LM_ERR("too many extras -> please increase the internal buffer\n");
	    goto error;
	}
	extra = (struct extra_attr*)pkg_malloc(sizeof(struct extra_attr));
	if (extra == 0) {
	    LM_ERR("no more pkg memory\n");
	    goto error;
	}
	memset( extra, 0, sizeof(struct extra_attr));

	/* link the new extra at the end */
	if (tail == 0) {
	    head = extra;
	} else {
	    tail->next = extra;
	}
	tail = extra;
	n++;

	/* get name */
	foo = s;
	while (*s && !isspace((int)*s) && EQUAL != *s)  s++;
	if (*s == 0) goto parse_error;
	if (*s==EQUAL) {
	    extra->name.len = (s++) - foo;
	} else {
	    extra->name.len = (s++) - foo;
	    /* skip spaces */
	    while (*s && isspace((int)*s))  s++;
	    if (*s != EQUAL) goto parse_error;
	    s++;
	}
	extra->name.s = foo;

	/* skip spaces */
	while (*s && isspace((int)*s))  s++;

	/* get value type */
	stmp.s = s; stmp.len = strlen(s);
	if ((foo = pv_parse_spec(&stmp, &extra->spec)) == 0 )
	    goto parse_error;
	s = foo;

	/* skip spaces */
	while (*s && isspace((int)*s))  s++;
	if (*s && ((*(s++) != SEPARATOR) || (*s == 0)))
	    goto parse_error;
    }

    /* go throught all extras and make the names null terminated */
    for( extra = head; extra; extra = extra->next)
	extra->name.s[extra->name.len] = 0;

    return head;

parse_error:
    LM_ERR("parse failed in <%s> around position %d\n",
	   extra_str, (int)(long)(s-extra_str));

error:
    LM_ERR("error\n");
    destroy_extras(head);
    return 0;
}


/*
 * Fill attr array name component with names of extra attributes
 * starting from offset. Return number of attributes added.
 */
int extra2attrs(struct extra_attr *extra, struct attr *attrs, int offset)
{
    int i;

    for (i = 0; extra; i++, extra = extra->next) {
	attrs[offset+i].n = extra->name.s;
    }
    return i;
}


/*
 * Get pseudo variable values of extra attributes to val_arr.
 * Return number of values or -1 in case of error.
 */
int extra2strar( struct extra_attr *extra, struct sip_msg *rq, str *val_arr)
{
    pv_value_t value;
    int n;
    int r;

    n = 0;
    r = 0;

    while (extra) {
	/* get the value */
	if (pv_get_spec_value(rq, &extra->spec, &value) != 0) {
	    LM_ERR("failed to get value of extra attribute'%.*s'\n",
		   extra->name.len,extra->name.s);
	}

	/* check for overflow */
	if (n == MAX_EXTRA) {
	    LM_WARN("array too short -> ommiting extras for accounting\n");
	    return -1;
	}

	if(value.flags&PV_VAL_NULL) {
	    /* convert <null> to empty to have consistency */
	    val_arr[n].s = 0;
	    val_arr[n].len = 0;
	} else if (value.flags&PV_VAL_INT) {
	    /* len = -1 denotes int type */
	    val_arr[n].s = (char *)(long)value.ri;
	    val_arr[n].len = -1;
	} else {
	    /* set the value into the acc buffer */
	    if (value.rs.s+value.rs.len == static_detector) {
		val_arr[n].s = int_buf + r*INT2STR_MAX_LEN;
		val_arr[n].len = value.rs.len;
		memcpy(val_arr[n].s, value.rs.s, value.rs.len);
		r++;
	    } else {
		val_arr[n] = value.rs;
	    }
	}
	n++;
	extra = extra->next;
    }

    return n;
}


/* Free memory allocated for extra attributes */
void destroy_extras(struct extra_attr *extra)
{
    struct extra_attr *foo;

    while (extra) {
	foo = extra;
	extra = extra->next;
	pkg_free(foo);
    }
}
