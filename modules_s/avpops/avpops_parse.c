/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * AVPOPS SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * AVPOPS SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2004-10-04  first version (ramona)
 *  2004-11-11  DB scheme added (ramona)
 *  2004-11-17  aligned to new AVP core global aliases (ramona)
 */



#include <stdlib.h>
#include <ctype.h>

#include "../../ut.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "avpops_parse.h"


#define SCHEME_UUID_COL          "uuid_col"
#define SCHEME_UUID_COL_LEN      (sizeof(SCHEME_UUID_COL)-1)
#define SCHEME_USERNAME_COL      "username_col"
#define SCHEME_USERNAME_COL_LEN  (sizeof(SCHEME_USERNAME_COL)-1)
#define SCHEME_DOMAIN_COL        "domain_col"
#define SCHEME_DOMAIN_COL_LEN    (sizeof(SCHEME_DOMAIN_COL)-1)
#define SCHEME_VALUE_COL         "value_col"
#define SCHEME_VALUE_COL_LEN     (sizeof(SCHEME_VALUE_COL)-1)
#define SCHEME_TABLE             "table"
#define SCHEME_TABLE_LEN         (sizeof(SCHEME_TABLE)-1)
#define SCHEME_VAL_TYPE          "value_type"
#define SCHEME_VAL_TYPE_LEN      (sizeof(SCHEME_VAL_TYPE)-1)
#define SCHEME_INT_TYPE          "integer"
#define SCHEME_INT_TYPE_LEN      (sizeof(SCHEME_INT_TYPE)-1)
#define SCHEME_STR_TYPE          "string"
#define SCHEME_STR_TYPE_LEN      (sizeof(SCHEME_STR_TYPE)-1)


char *parse_avp_attr(char *s, struct fis_param *attr, char end)
{
	unsigned int uint;
	str tmp;

	/*DBG("s=%p s=%c(%d)\n",s,*s,*s);*/
	/* search for type identifier */
	if ( s[0] && s[1]==':' )
	{
		switch (s[0])
		{
			case 'i':
			case 'I':
				attr->flags |= AVPOPS_VAL_INT;
				break;
			case 's':
			case 'S':
				attr->flags |= AVPOPS_VAL_STR;
				break;
			default:
				LOG(L_ERR,"ERROR:avpops:parse_avp_attr: invalid type '%c'\n",
					s[0]);
				goto error;
		}
		s += 2;
	}
	/* search for the avp name */
	tmp.s = s;
	while ( *s && *s!=end && !isspace((int)*s)) s++;
	tmp.len = s - tmp.s;
	if (tmp.len==0)
	{
		attr->flags |= AVPOPS_VAL_NONE;
	} else {
		if ( attr->flags&AVPOPS_VAL_INT)
		{
			/* convert to ID (int) */
			if ( str2int( &tmp, &uint)==-1 )
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_attr: attribute is not "
					"int as type says <%s>\n", tmp.s);
				goto error;
			}
			attr->val.n = (int)uint;
		} else {
			/* duplicate name as str NULL terminated */
			attr->val.s = (str*)pkg_malloc( sizeof(str) + tmp.len + 1 );
			if (attr->val.s==0)
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_attr: no more pkg mem\n");
				goto error;
			}
			attr->val.s->s = ((char*)attr->val.s) + sizeof(str);
			attr->val.s->len = tmp.len;
			memcpy( attr->val.s->s, tmp.s, tmp.len);
			attr->val.s->s[attr->val.s->len] = 0;
		}
	}

	return s;
error:
	return 0;
}



int parse_avp_db(char *s, struct db_param *dbp, int allow_scheme)
{
	unsigned long ul;
	str   tmp;
	char  c;
	char  have_scheme;
	int   type;

	/* parse the attribute name - check first if it's not an alias */
	if ( *s=='$')
	{
		tmp.s = ++s;
		/* is an avp alias -> see where it ends */
		if ( (s=strchr(tmp.s, '/'))!=0 )
		{
			c = *s;
			tmp.len = s - tmp.s;
		} else {
			c = 0;
			tmp.len = strlen(tmp.s);
		}
		if (tmp.len==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_db: empty alias in <%s>\n", s);
			goto error;
		}
		/* search the alias */
		if ( lookup_avp_galias( &tmp, &type, &dbp->a.val)!=0 )
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_db: unknow alias"
				"\"%s\"\n", tmp.s);
			goto error;
		}
		dbp->a.flags = (type&AVP_NAME_STR)?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
	} else {
		if ( (s=parse_avp_attr( s, &(dbp->a), '/'))==0 )
			goto error;
		if (*s!=0 && *s!='/')
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_db: parse error arround "
				"<%s>\n",s);
			goto error;
		}
	}
	dbp->a.flags |= AVPOPS_VAL_AVP;

	/* optimize asn keep the attribute name as str also to
	 * speed up db querie builds */
	if (!(dbp->a.flags&AVPOPS_VAL_NONE))
	{
		if (dbp->a.flags&AVPOPS_VAL_STR)
		{
			dbp->sa = *dbp->a.val.s;
		} else {
			ul = (unsigned long)dbp->a.val.n;
			tmp.s = int2str( ul, &(tmp.len) );
			dbp->sa.s = (char*)pkg_malloc( tmp.len + 1 );
			if (dbp->sa.s==0)
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_db: no more pkg mem\n");
				goto error;
			}
			memcpy( dbp->sa.s, tmp.s, tmp.len);
			dbp->sa.len = tmp.len;
			dbp->sa.s[dbp->sa.len] = 0;
		}
	}

	/* is there a table name ? */
	if (s && *s)
	{
		s++;
		if (*s=='$')
		{
			if (allow_scheme==0)
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_db: function doesn't "
					"support DB schemes\n");
				goto error;
			}
			if (dbp->a.flags&AVPOPS_VAL_NONE)
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_db: inconsistent usage of "
					"DB scheme without complet specification of AVP name\n");
				goto error;
			}
			have_scheme = 1;
			s++;
		} else {
			have_scheme = 0;
		}
		tmp.s = s;
		tmp.len = 0;
		while ( *s ) s++;
		tmp.len = s - tmp.s;
		if (tmp.len==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_av_dbp: empty scheme/table name\n");
			goto error;
		}
		if (have_scheme)
		{
			dbp->scheme = avp_get_db_scheme( tmp.s );
			if (dbp->scheme==0) 
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_db: scheme <%s> not found\n",
					tmp.s);
				goto error;
			}
			/* update scheme flags with AVP name type*/
			dbp->scheme->db_flags|=dbp->a.flags&AVPOPS_VAL_STR?AVP_NAME_STR:0;
		} else {
			/* duplicate table as str NULL terminated */
			dbp->table = (char*)pkg_malloc( tmp.len + 1 );
			if (dbp->table==0)
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_db: no more pkg mem\n");
				goto error;;
			}
			memcpy( dbp->table, tmp.s, tmp.len);
			dbp->table[tmp.len] = 0;
		}
	}

	return 0;
error:
	return -1;
}


struct fis_param* parse_intstr_value(char *p, int len)
{
	struct fis_param *vp;
	unsigned int uint;
	str val_str;
	int flags;

	if (p==0 || len==0)
			goto error;

	if (*(p+1)==':')
	{
		if (*p=='i' || *p=='I')
			flags = AVPOPS_VAL_INT;
		else if (*p=='s' || *p=='S')
			flags = AVPOPS_VAL_STR;
		else
		{
			LOG(L_ERR,"ERROR:avpops:parse_intstr_value: unknown value type "
				"<%c>\n",*p);
			goto error;
		}
		p += 2;
		len -= 2;
		if (*p==0 || len<=0 )
		{
			LOG(L_ERR,"ERROR:avpops:parse_intstr_value: parse error arround "
				"<%.*s>\n",len,p);
			goto error;
		}
	} else {
		flags = AVPOPS_VAL_STR;
	}
	/* get the value */
	vp = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
	if (vp==0)
	{
		LOG(L_ERR,"ERROR:avpops:parse_intstr_value: no more pkg mem\n");
		goto error;;
	}
	memset( vp, 0, sizeof(struct fis_param));
	vp->flags = flags;
	val_str.s = p;
	val_str.len = len;
	if (flags&AVPOPS_VAL_INT) {
		/* convert the value to integer */
		if ( str2int( &val_str, &uint)==-1 )
		{
			LOG(L_ERR,"ERROR:avpops:parse_intstr_value: value is not int "
				"as type says <%.*s>\n", val_str.len, val_str.s);
			goto error;
		}
		vp->val.n = (int)uint;
	} else {
		/* duplicate the value as string */
		vp->val.s = (str*)pkg_malloc( sizeof(str) + val_str.len +1 );
		if (vp->val.s==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_intstr_value: no more pkg mem\n");
			goto error;
		}
		vp->val.s->s = ((char*)vp->val.s) + sizeof(str);
		vp->val.s->len = val_str.len;
		memcpy( vp->val.s->s, val_str.s, val_str.len);
		vp->val.s->s[vp->val.s->len] = 0;
	}

	return vp;
error:
	return 0;
}


struct fis_param* parse_check_value(char *s)
{
	struct fis_param *vp;
	int  flags;
	char *p;
	char *t;
	int len;
	int type;
	str alias;

	flags = 0;
	vp = 0;

	if ( (p=strchr(s,'/'))==0 || p-s!=2 )
		goto parse_error;
	/* get the operation */
	if (strncasecmp(s,"eq",2)==0)
	{
		flags |= AVPOPS_OP_EQ;
	} else if (strncasecmp(s,"lt",2)==0) {
		flags |= AVPOPS_OP_LT;
	} else if (strncasecmp(s,"gt",2)==0) {
		flags |= AVPOPS_OP_GT;
	} else if (strncasecmp(s,"re",2)==0) {
		flags |= AVPOPS_OP_RE;
	} else {
		LOG(L_ERR,"ERROR:avpops:parse_check_value: unknown operation "
			"<%.*s>\n",2,s);
		goto error;
	}
	/* get the value */
	if (*(++p)==0)
		goto parse_error;
	if ( (t=strchr(p,'/'))==0)
		len = strlen(p);
	else
		len = t-p;

	if (*p=='$')
	{
		if (*(++p)==0 || (--len)==0)
			goto parse_error;
		/* struct for value */
		vp = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
		if (vp==0) {
			LOG(L_ERR,"ERROR:avpops:parse_check_value: no more pkg mem\n");
			goto error;
		}
		memset( vp, 0, sizeof(struct fis_param));
		/* variable -> which one? */
		if ( (strncasecmp(p,"ruri"  ,len)==0 && (flags|=AVPOPS_USE_RURI))
		  || (strncasecmp(p,"from"  ,len)==0 && (flags|=AVPOPS_USE_FROM))
		  || (strncasecmp(p,"to"    ,len)==0 && (flags|=AVPOPS_USE_TO))
		  || (strncasecmp(p,"src_ip",len)==0 && (flags|=AVPOPS_USE_SRC_IP)))
		{
			flags |= AVPOPS_VAL_NONE;
		} else {
			alias.s = p;
			alias.len = len;
			if ( lookup_avp_galias( &alias, &type, &vp->val)!=0 )
			{
				LOG(L_ERR,"ERROR:avpops:parse_check_value: unknown "
					"variable/alias <%.*s>\n",len,p);
				goto error;
			}
			flags |= AVPOPS_VAL_AVP |
				((type&AVP_NAME_STR)?AVPOPS_VAL_STR:AVPOPS_VAL_INT);
			DBG("flag==%d\n",flags);
		}
		p += len;
	} else {
		/* value is explicitly given */
		if ( (vp=parse_intstr_value(p,len))==0) {
			LOG(L_ERR,"ERROR:avpops:parse_check_value: unable to "
				"parse value\n");
			goto error;
		}
		/* go over */
		p += len;
	}

	/* any flags */
	if (*p!=0 )
	{
		if (*p!='/' || *(++p)==0)
			goto parse_error;
		while (*p)
		{
			switch (*p)
			{
				case 'g':
				case 'G':
					flags|=AVPOPS_FLAG_ALL;
					break;
				case 'i':
				case 'I':
					flags|=AVPOPS_FLAG_CI;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:parse_check_value: unknown flag "
						"<%c>\n",*p);
					goto error;
			}
			p++;
		}
	}

	vp->flags |= flags;
	return vp;
parse_error:
	LOG(L_ERR,"ERROR:avpops:parse_check_value: parse error in <%s> pos %ld\n",
		s,(long)(p-s));
error:
	if (vp) pkg_free(vp);
	return 0;
}


#define  duplicate_str(_p, _str, _error) \
	do { \
		_p = (char*)pkg_malloc(_str.len+1); \
		if (_p==0) \
		{ \
			LOG(L_ERR,"ERROR:avpops:parse_avp_sb_scheme: " \
				"no more pkg memory\n");\
			goto _error; \
		} \
		memcpy( _p, _str.s, _str.len); \
		_p[_str.len] = 0; \
	}while(0)

int parse_avp_db_scheme( char *s, struct db_scheme *scheme)
{
	str foo;
	str bar;
	char *p;

	if (s==0 || *s==0)
		goto error;
	p = s;

	/*parse the name */
	while (*p && isspace((int)*p)) p++;
	foo.s = p;
	while (*p && *p!=':' && !isspace((int)*p)) p++;
	if (foo.s==p || *p==0)
		/* missing name or empty scheme */
		goto parse_error;
	foo.len = p - foo.s;
	/* dulicate it */
	duplicate_str( scheme->name, foo, error);

	/* parse the ':' separator */
	while (*p && isspace((int)*p)) p++;
	if (*p!=':')
		goto parse_error;
	p++;
	while (*p && isspace((int)*p)) p++;
	if (*p==0)
		goto parse_error;

	/* set as default value type string */
	scheme->db_flags = AVP_VAL_STR;

	/* parse the attributes */
	while (*p)
	{
		/* get the attribute name */
		foo.s = p;
		while (*p && *p!='=' && !isspace((int)*p)) p++;
		if (p==foo.s || *p==0)
			/* missing attribute name */
			goto parse_error;
		foo.len = p - foo.s;

		/* parse the '=' separator */
		while (*p && isspace((int)*p)) p++;
		if (*p!='=')
			goto parse_error;
		p++;
		while (*p && isspace((int)*p)) p++;
		if (*p==0)
			goto parse_error;

		/* parse the attribute value */
		bar.s = p;
		while (*p && *p!=';' && !isspace((int)*p)) p++;
		if (p==bar.s)
			/* missing attribute value */
			goto parse_error;
		bar.len = p - bar.s;

		/* parse the ';' separator, if any */
		while (*p && isspace((int)*p)) p++;
		if (*p!=0 && *p!=';')
			goto parse_error;
		if (*p==';') p++;
		while (*p && isspace((int)*p)) p++;

		/* identify the attribute */
		if ( foo.len==SCHEME_UUID_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_UUID_COL, foo.len) )
		{
			duplicate_str( scheme->uuid_col, bar, error);
		} else
		if ( foo.len==SCHEME_USERNAME_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_USERNAME_COL, foo.len) )
		{
			duplicate_str( scheme->username_col, bar, error);
		} else
		if ( foo.len==SCHEME_DOMAIN_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_DOMAIN_COL, foo.len) )
		{
			duplicate_str( scheme->domain_col, bar, error);
		} else
		if ( foo.len==SCHEME_VALUE_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_VALUE_COL, foo.len) )
		{
			duplicate_str( scheme->value_col, bar, error);
		} else
		if ( foo.len==SCHEME_TABLE_LEN && 
		!strncasecmp( foo.s, SCHEME_TABLE, foo.len) )
		{
			duplicate_str( scheme->table, bar, error);
		} else
		if ( foo.len==SCHEME_VAL_TYPE_LEN && 
		!strncasecmp( foo.s, SCHEME_VAL_TYPE, foo.len) )
		{
			if ( bar.len==SCHEME_INT_TYPE_LEN &&
			!strncasecmp( bar.s, SCHEME_INT_TYPE, bar.len) )
				scheme->db_flags &= (~AVP_VAL_STR);
			else if ( bar.len==SCHEME_STR_TYPE_LEN &&
			!strncasecmp( bar.s, SCHEME_STR_TYPE, bar.len) )
				scheme->db_flags = AVP_VAL_STR;
			else
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_sb_scheme: unknown "
					"value type <%.*s>\n",bar.len,bar.s);
				goto error;
			}
		} else {
			LOG(L_ERR,"ERROR:avpops:parse_avp_sb_scheme: unknown "
				"attribute <%.*s>\n",foo.len,foo.s);
			goto error;
		}
	} /* end while */

	return 0;
parse_error:
	LOG(L_ERR,"ERROR:avpops:parse_avp_sb_scheme: parse error in <%s> "
		"around %ld\n", s, (long)(p-s));
error:
	return -1;
}
