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
 */



#include <stdlib.h>
#include <ctype.h>

#include "../../ut.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "avpops_parse.h"
#include "avpops_aliases.h"


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



int parse_avp_db(char *s, struct db_param *dbp)
{
	unsigned long ul;
	str tmp;

	/* parse the attribute name */
	if ( (s=parse_avp_attr( s, &(dbp->a), '/'))==0 )
		goto error;
	if (*s!=0 && *s!='/') {
		LOG(L_ERR,"ERROR:avpops:parse_avp_db: parse error arround <%s>\n",s);
		goto error;
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
			if (dbp->sa.s==0) {
				LOG(L_ERR,"ERROR:avpops:parse_avp_db: no more pkg mem\n");
				goto error;
			}
			memcpy( dbp->sa.s, tmp.s, tmp.len);
			dbp->sa.len = tmp.len;
			dbp->sa.s[dbp->sa.len] = 0;
		}
	}

	/* is there a table name ? */
	if ( *s )
	{
		s++;
		tmp.s = s;
		tmp.len = 0;
		while ( *s ) s++;
		tmp.len = s - tmp.s;
		if (tmp.len==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_av_dbp: empty table name\n");
			goto error;
		}
		/* duplicate table as str NULL terminated */
		dbp->table.s = (char*)pkg_malloc( tmp.len + 1 );
		if (dbp->table.s==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_db: no more pkg mem\n");
			goto error;;
		}
		dbp->table.len = tmp.len;
		memcpy( dbp->table.s, tmp.s, tmp.len);
		dbp->table.s[dbp->table.len] = 0;
	}

	return 0;
error:
	return -1;
}



int  parse_avp_aliases(char *s, char c1, char c2)
{
	char *alias;
	struct fis_param *attr;
	
	if (s==0)
	{
		LOG(L_ERR,"ERROR:avpops:parse_avp_aliases: null string received\n");
		goto error;
	}

	while (*s)
	{
		/* skip spaces */
		while (*s && isspace((int)*s))  s++;
		if (*s==0)
			return 0;
		/* get alias name */
		alias = s;
		while (*s && !isspace((int)*s) && c1!=*s )  s++;
		if (*s==0)
			goto parse_error;
		if (*s==c1)
		{
			*(s++) = 0; /*make alias null terminated */
		} else {
			*(s++) = 0; /*make alias null terminated */
			/* skip spaces */
			while (*s && isspace((int)*s))  s++;
			if (*s!=c1)
				goto parse_error;
			s++;
		}
		/* skip spaces */
		while (*s && isspace((int)*s))  s++;	
		/* get avp attribute name */
		attr = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
		if (attr==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_aliases: no more pkg mem\n");
			goto error;
		}
		memset( attr, 0, sizeof(struct fis_param));
		if ( (s=parse_avp_attr( s, attr, c2))==0)
			goto parse_error;
		if (*s==c2)
			s++;
		if (attr->flags&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_aliases: avp must have a name\n");
			goto error;
		}
		/* add alias */
		if (add_avp_alias( alias, attr)!=0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_avp_aliases: failed to add alias\n");
			goto error;
		}
		/* skip spaces */
		while (*s && isspace((int)*s))  s++;	
	}

	return 0;
parse_error:
	LOG(L_ERR,"ERROR:avops:parse_avp_aliases: parse failed in <%s>\n",alias);
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
	char foo;
	char *p;
	char *t;
	int len;

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
		flags |= AVPOPS_VAL_NONE;
		/* variable -> which one? */
		if ( (strncasecmp(p,"ruri"  ,len)==0 && (flags|=AVPOPS_USE_RURI))
		  || (strncasecmp(p,"from"  ,len)==0 && (flags|=AVPOPS_USE_FROM))
		  || (strncasecmp(p,"to"    ,len)==0 && (flags|=AVPOPS_USE_TO))
		  || (strncasecmp(p,"src_ip",len)==0 && (flags|=AVPOPS_USE_SRC_IP)))
		{
			vp = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
			if (vp==0) {
				LOG(L_ERR,"ERROR:avpops:parse_check_value: no more pkg mem\n");
				goto error;
			}
			memset( vp, 0, sizeof(struct fis_param));
			flags |= AVPOPS_VAL_NONE;
		} else {
			foo = p[len];
			p[len] = 0; /* make the string null terminated */
			vp = lookup_avp_alias(p);
			p[len] = foo;
			if ( vp==0 ) {
				LOG(L_ERR,"ERROR:avpops:parse_check_value: unknown "
					"variable/alias <%.*s>\n",len,p);
				goto error;
			}
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
	LOG(L_ERR,"ERROR:avpops:parse_check_value: parse error in <%s> pos %d\n",
		s,p-s);
error:
	if (vp) pkg_free(vp);
	return 0;
}


