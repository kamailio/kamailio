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
 */



#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../dprint.h"
#include "../../ut.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../parser/parse_hname2.h"
#include "acc_extra.h"


#define C1 '='
#define C2 '/'
#define C3 ';'

#define AVP_TYPE_S    "avp"
#define AVP_TYPE_LEN  (sizeof(AVP_TYPE_S)-1)
#define HDR_TYPE_S    "hdr"
#define HDR_TYPE_LEN  (sizeof(HDR_TYPE_S)-1)


static str na={"n/a", 3};
/* static arrays used for faster and simpler int to str conversion */
static char int_buf[INT2STR_MAX_LEN*MAX_ACC_EXTRA];
static str  str_buf[MAX_ACC_EXTRA];



void init_acc_extra()
{
	int i;
	for (i=0; i<MAX_ACC_EXTRA; i++)
	{
		str_buf[i].s = int_buf + i*INT2STR_MAX_LEN;
	}
}


/*
void print_extras( struct acc_extra *extra)
{
	DBG("----- start extra list ------ \n");
	while (extra)
	{
		DBG("print_extras: flg=(%d) <%s>=%.*s/%d\n",extra->flags,extra->name.s,
			extra->sval.len,extra->sval.s, extra->ival);
		extra = extra->next;
	}
}
*/

#define _HDR_OPTIMIZATION
struct acc_extra *parse_acc_extra(char *extra_str, int allowed_flags)
{
	struct hdr_field  hdr;
	struct acc_extra *head;
	struct acc_extra *extra;
	char *foo;
	char *s;
	char bkp;
	str  val;
	int  n;


	n = 0;
	head = 0;
	extra = 0;
	s = extra_str;

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
		/* link the new extra */
		extra->next = head;
		head = extra;
		n++;

		/* get name */
		foo = s;
		while (*s && !isspace((int)*s) && C1!=*s)  s++;
		if (*s==0)
			goto parse_error;
		if (*s==C1)
		{
			extra->name.len = (s++) - foo;
		} else {
			extra->name.len = (s++) - foo;
			/* skip spaces */
			while (*s && isspace((int)*s))  s++;
			if (*s!=C1)
				goto parse_error;
			s++;
		}
		extra->name.s = (char*)pkg_malloc(extra->name.len+1);
		if (extra->name.s==0)
		{
			LOG(L_ERR,"ERROR:acc:parse_acc_extra: no more pkg mem 2\n");
			goto error;
		}
		memcpy(extra->name.s, foo, extra->name.len);
		extra->name.s[extra->name.len] = 0;

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;

		/* get value type */
		if (strncasecmp(s,HDR_TYPE_S,HDR_TYPE_LEN)==0)
		{
			extra->flags = ACC_EXTRA_HEADER;
			s += HDR_TYPE_LEN;
		} 
		else
			if (strncasecmp(s,AVP_TYPE_S,AVP_TYPE_LEN)==0)
			{
				extra->flags = ACC_EXTRA_AVP;
				s += AVP_TYPE_LEN;
			}

		if (*(s++)!=C2 || !*s)
			goto parse_error;
		/* get the value */
		val.s = s;
		for( ; *s && *s!=C2 && *s!=C3; s++ );
		if (*s==0)
			val.len = strlen(val.s);
		else
			val.len = s - val.s;
		if (val.len==0)
			goto parse_error;

		/* if AVP -> parse value */
		if (extra->flags&ACC_EXTRA_AVP)
		{
			if ( val.s[0] && val.s[1]==':')
			{
				switch (val.s[0])
				{
					case 's':
					case 'S':
						break;
					case 'i':
					case 'I':
						extra->flags |= ACC_EXTRA_IS_INT;
						break;
					default:
						LOG(L_ERR,"ERROR:acc:parse_acc_extra: unknown "
							"avp type (%c)\n", *foo);
						goto error;
				}
				val.s += 2;
				val.len -= 2;
				if (val.len == 0)
					goto parse_error;
			}
			/* if type int, convert it also as integer */
			if (extra->flags & ACC_EXTRA_IS_INT && 
				str2int(&val, (unsigned int*)&extra->ival)!=0)
			{
				LOG(L_ERR,"ERROR:acc:parse_acc_extra: avp name <%.*s> is "
					"not ID as type says(%d)\n", val.len, val.s, extra->flags);
				goto error;
			}
		} else {
			/* do a terrible hack just to be able to use parse_hnames */
			bkp = val.s[val.len];
			val.s[val.len] = ':';
			val.len++;
			/* header name - try to parse it and see if it's a known header */
			if (parse_hname2(val.s, val.s+val.len,&hdr)==0)
			{
				LOG(L_ERR,"ERROR:acc:parse_acc_extra: bug??\n");
				goto error;
			}
			val.len--;
			val.s[val.len] = bkp;
			extra->ival = hdr.type;
			if (extra->ival!=HDR_OTHER)
				LOG(L_INFO,"INFO:acc:parse_acc_extra: optimazing by using "
					"hdr type (%d) instead of <%.*s>\n",
					extra->ival,val.len,val.s);
		}
		/* get the value as string */
		extra->sval.s = (char*)pkg_malloc(val.len + 1);
		if (extra->sval.s==0)
		{
			LOG(L_ERR,"ERROR:acc:parse_acc_extra: no more pkg mem 3\n");
			goto error;
		}
		extra->sval.len = val.len;
		memcpy(extra->sval.s, val.s, val.len);
		extra->sval.s[extra->sval.len] = 0;

		/* are any flags? */
		if (*s==C2)
		{
			for(s++ ; *s && !isspace((int)*s) && *s!=C3 ; s++)
				switch (*s)
				{
					case 'g': case 'G':
						extra->flags|=(ACC_EXTRA_GLOBAL&allowed_flags);
						break;
					default:
						LOG(L_ERR,"ERROR:acc:parse_acc_extra: unknown "
							"flag (%c)\n",*s);
						goto error;
				}
		}

		/* skip spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s && (*(s++)!=C3 || *s==0))
			goto parse_error;
	}

	/* print_extras(head); */
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
		
		if (foo->name.s)
			pkg_free(foo->name.s);
		if (foo->sval.s)
			pkg_free(foo->sval.s);
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


static inline struct hdr_field* search_hdr(struct hdr_field *hdr,
												struct acc_extra *extra)
{
	if (extra->ival==HDR_OTHER)
	{
		while (hdr)
		{
			if (extra->sval.len==hdr->name.len &&
			strncasecmp(extra->sval.s, hdr->name.s, hdr->name.len)==0)
				return hdr;
			hdr = hdr->next;
		}
	} else {
		while (hdr)
		{
			if (extra->ival==hdr->type)
				return hdr;
			hdr = hdr->next;
		}
	}
	return hdr;
}



#define test_overflow( _n, _jumper) \
	do{\
		if ((_n)==MAX_ACC_EXTRA) \
		{ \
			LOG(L_WARN,"WARNING:acc:extra2strar: array to short " \
				"-> ommiting extras for accounting\n"); \
			goto _jumper; \
		}\
	} while(0)

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
	struct hdr_field *hdr;
	struct usr_avp   *avp;
	unsigned short   avp_type;
	int_str          avp_name;
	int_str          avp_val;
	int    hdr_parsed;
	char  *p;
	int    m;
	int    n;

	n = 0;
	m = 0;
	hdr_parsed = 0;

	while (extra) 
	{
		if (extra->flags & ACC_EXTRA_AVP)
		{
			/* account an AVP */
			if (extra->flags&ACC_EXTRA_IS_INT)
			{
				avp_type = 0;
				avp_name.n = extra->ival;
			} else {
				avp_type = AVP_NAME_STR;
				avp_name.s = &(extra->sval);
			}
			avp = search_first_avp( avp_type, avp_name, &avp_val);
			if (!avp)
			{
				test_overflow( n, done);
				set_acc( n, extra->name, &na );
			} else {
				do
				{
					test_overflow( n, done);
					if (avp->flags & AVP_VAL_STR)
					{
						set_acc( n, extra->name, avp_val.s);
					} else {
						p = int2str((unsigned long)avp_val.n, &str_buf[m].len);
						memcpy(str_buf[m].s, p, str_buf[m].len);
						set_acc( n, extra->name, &str_buf[m]);
						m++;
					}
					if (!(extra->flags&ACC_EXTRA_GLOBAL))
						break;
				} while ((avp=search_next_avp(avp, &avp_val))!=0);
			}
		} else {
			/* account a HEADER - first parse them all */
			if (!hdr_parsed && ++hdr_parsed && parse_headers(rq, HDR_EOH, 0)!=0)
			{
				LOG(L_ERR,"ERROR:acc:extra2strar: failed to parse headers ->"
					" skipping hdr extras\n");
			}
			hdr = search_hdr(rq->headers, extra);
			if (!hdr)
			{
				test_overflow(n, done);
				set_acc(n, extra->name, &na);
			} else {
				do
				{
					test_overflow(n, done);
					set_acc(n, extra->name, &hdr->body);
					if (!(extra->flags & ACC_EXTRA_GLOBAL))
						break;
				} while ((hdr=search_hdr(hdr->next, extra))!=0);
			}
		}

		extra = extra->next;
	}

done:
	return n;
}

