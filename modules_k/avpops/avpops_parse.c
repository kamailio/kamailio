/*
 * $Id$
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router.
 *
 * AVPOPS OpenSER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * AVPOPS OpenSER-module is distributed in the hope that it will be useful,
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
				attr->opd |= AVPOPS_VAL_INT;
				break;
			case 's':
			case 'S':
				attr->opd |= AVPOPS_VAL_STR;
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
		attr->opd |= AVPOPS_VAL_NONE;
	} else {
		if ( attr->opd&AVPOPS_VAL_INT)
		{
			/* convert to ID (int) */
			if ( str2int( &tmp, &uint)==-1 )
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_attr: attribute is not "
					"int as type says <%s>\n", tmp.s);
				goto error;
			}
			attr->sval.p.val.s = 0;
			attr->sval.p.val.len = (int)uint;
		} else {
			/* duplicate name as str NULL terminated */
			attr->sval.p.val.s = (char*)pkg_malloc(tmp.len + 1);
			if (attr->sval.p.val.s==0)
			{
				LOG(L_ERR,"ERROR:avpops:parse_avp_attr: no more pkg mem\n");
				goto error;
			}
			attr->sval.p.val.len = tmp.len;
			memcpy(attr->sval.p.val.s, tmp.s, tmp.len);
			attr->sval.p.val.s[attr->sval.p.val.len] = 0;
		}
	}

	return s;
error:
	return 0;
}

struct fis_param *avpops_parse_pvar(char *s, int flags)
{
	struct fis_param *ap;
	char *p;

	/* compose the param structure */
	ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
	if (ap==0)
	{
		LOG(L_ERR,"ERROR:avpops:avpops_parse_pvar: no more pkg mem\n");
		return NULL;
	}
	memset( ap, 0, sizeof(struct fis_param));
	p = xl_parse_spec(s, &ap->sval, flags);
	if(p==0)
	{
		pkg_free(ap);
		return NULL;
	}

	ap->opd |= AVPOPS_VAL_PVAR;
	return ap;
}


int parse_avp_db(char *s, struct db_param *dbp, int allow_scheme)
{
	unsigned long ul;
	str   tmp;
	char  have_scheme;
	char *p;
	char *p0;

	tmp.s = s;
	/* parse the attribute name - check first if it's not an alias */
	p0=strchr(tmp.s, '/');
	if(p0!=NULL)
		*p0=0;
	if ( *s!='$')
	{
		if(*s=='\0')
		{
			dbp->a.opd = AVPOPS_VAL_NONE;
		}
		else if (strlen(s)==2 && s[1]==':' && (s[0]=='s' ||s[0]=='S'))
		{
			dbp->a.opd = AVPOPS_VAL_NONE|AVPOPS_VAL_STR;
		} else if ((strlen(s)==2 && s[1]==':' && (s[0]=='i' || s[0]=='I'))) {
			dbp->a.opd = AVPOPS_VAL_NONE|AVPOPS_VAL_INT;
		} else {
			LOG(L_ERR,"ERROR:avops:parse_avp_db: bad param - "
				"expected : $avp(name), s: or i: value\n");
			return E_UNSPEC;
		}
	} else {
		p = xl_parse_spec(s, &dbp->a.sval,
				XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS);
		if (p==0 || *p!='\0' || dbp->a.sval.type!=XL_AVP)
		{
			LOG(L_ERR,"ERROR:avops:parse_avp_db: bad param - "
				"expected : $avp(name) or int/str value\n");
			return E_UNSPEC;
		}
		if(dbp->a.sval.flags&XL_DPARAM)
		{
			dbp->a.opd = AVPOPS_VAL_PVAR;
		} else {
			dbp->a.opd = (dbp->a.sval.p.val.s)?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
		}
	}

	/* optimize asn keep the attribute name as str also to
	 * speed up db querie builds */
	if (!(dbp->a.opd&AVPOPS_VAL_PVAR))
	{
		if (dbp->a.opd&AVPOPS_VAL_STR)
		{
			dbp->sa = dbp->a.sval.p.val;
			dbp->sa.s[dbp->sa.len] = '\0';
		} else {
			ul = (unsigned long)dbp->a.sval.p.val.len;
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

	/* restore '/' */
	if(p0)
		*p0 = '/';
	/* is there a table name ? */
	s = p0;
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
			if (dbp->a.opd&AVPOPS_VAL_NONE)
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
			dbp->scheme->db_flags|=dbp->a.opd&AVPOPS_VAL_STR?AVP_NAME_STR:0;
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
	int uint;
	str val_str;
	int flags;

	if (p==0 || len==0)
			goto error;

	if (len>1 && *(p+1)==':')
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
	vp->opd = flags;
	val_str.s = p;
	val_str.len = len;
	if (flags&AVPOPS_VAL_INT) {
		/* convert the value to integer */
		if(val_str.len>2 && p[0]=='0' && (p[1]=='x' || p[1]=='X'))
		{
			if(hexstr2int(val_str.s+2, val_str.len-2, &uint))
			{
				LOG(L_ERR,"ERROR:avpops:parse_intstr_value: value is not hex"
					" int as type says <%.*s>\n", val_str.len, val_str.s);
				goto error;
			}
		} else {
			if(str2sint( &val_str, &uint)==-1)
			{
				LOG(L_ERR,"ERROR:avpops:parse_intstr_value: value is not int"
					" as type says <%.*s>\n", val_str.len, val_str.s);
				goto error;
			}
		}
		vp->sval.p.val.len = uint;
	} else {
		/* duplicate the value as string */
		vp->sval.p.val.s = (char*)pkg_malloc((val_str.len+1)*sizeof(char));
		if (vp->sval.p.val.s==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_intstr_value: no more pkg mem\n");
			goto error;
		}
		vp->sval.p.val.len = val_str.len;
		memcpy(vp->sval.p.val.s, val_str.s, val_str.len);
		vp->sval.p.val.s[vp->sval.p.val.len] = 0;
	}

	return vp;
error:
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

struct fis_param* parse_check_value(char *s)
{
	struct fis_param *vp;
	int  ops;
	int  opd;
	char *p;
	char *t;
	int len;

	ops = 0;
	opd = 0;
	vp = 0;

	if ( (p=strchr(s,'/'))==0 || (p-s!=2&&p-s!=3) )
		goto parse_error;
	/* get the operation */
	if (strncasecmp(s,"eq",2)==0) {
		ops |= AVPOPS_OP_EQ;
	} else if (strncasecmp(s,"ne",2)==0) {
		ops |= AVPOPS_OP_NE;
	} else if (strncasecmp(s,"lt",2)==0) {
		ops |= AVPOPS_OP_LT;
	} else if (strncasecmp(s,"le",2)==0) {
		ops |= AVPOPS_OP_LE;
	} else if (strncasecmp(s,"gt",2)==0) {
		ops |= AVPOPS_OP_GT;
	} else if (strncasecmp(s,"ge",2)==0) {
		ops |= AVPOPS_OP_GE;
	} else if (strncasecmp(s,"re",2)==0) {
		ops |= AVPOPS_OP_RE;
	} else if (strncasecmp(s,"fm",2)==0) {
		ops |= AVPOPS_OP_FM;
	} else if (strncasecmp(s,"and",3)==0) {
		ops |= AVPOPS_OP_BAND;
	} else if (strncasecmp(s,"or",2)==0) {
		ops |= AVPOPS_OP_BOR;
	} else if (strncasecmp(s,"xor",3)==0) {
		ops |= AVPOPS_OP_BXOR;
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
		/* is variable */
		vp = avpops_parse_pvar(p, XL_THROW_ERROR|XL_DISABLE_COLORS);
		if (vp==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_check_value: unable to get"
					" pseudo-variable\n");
			goto error;
		}
		if (vp->sval.type==XL_NULL)
		{
			LOG(L_ERR,"ERROR:avops:parse_check_value: bad param; "
				"expected : $pseudo-variable or int/str value\n");
			goto error;
		}
		opd |= AVPOPS_VAL_PVAR;
		DBG("flag==%d/%d\n", opd, ops);
	} else {
		/* value is explicitly given */
		if ( (vp=parse_intstr_value(p,len))==0) {
			LOG(L_ERR,"ERROR:avpops:parse_check_value: unable to "
				"parse value\n");
			goto error;
		}
	}

	p = t;
	/* any flags */
	if (p!=NULL && *p!=0)
	{
		if (*p!='/' || *(++p)==0)
			goto parse_error;
		while (*p)
		{
			switch (*p)
			{
				case 'g':
				case 'G':
					ops|=AVPOPS_FLAG_ALL;
					break;
				case 'i':
				case 'I':
					ops|=AVPOPS_FLAG_CI;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:parse_check_value: unknown flag "
						"<%c>\n",*p);
					goto error;
			}
			p++;
		}
	}

	vp->ops |= ops;
	vp->opd |= opd;
	return vp;
parse_error:
	LOG(L_ERR,"ERROR:avpops:parse_check_value: parse error in <%s> pos %ld\n",
		s,(long)(p-s));
error:
	if (vp) pkg_free(vp);
	return 0;
}

struct fis_param* parse_op_value(char *s)
{
	struct fis_param *vp;
	int  ops;
	int  opd;
	char *p;
	char *t;
	int len;

	ops = 0;
	opd = 0;
	vp = 0;

	if ( (p=strchr(s,'/'))==0 || (p-s!=2&&p-s!=3) )
		goto parse_error;
	/* get the operation */
	if (strncasecmp(s,"add",3)==0) {
		ops |= AVPOPS_OP_ADD;
	} else if (strncasecmp(s,"sub",3)==0) {
		ops |= AVPOPS_OP_SUB;
	} else if (strncasecmp(s,"mul",3)==0) {
		ops |= AVPOPS_OP_MUL;
	} else if (strncasecmp(s,"div",3)==0) {
		ops |= AVPOPS_OP_DIV;
		ops |= AVPOPS_OP_FM;
	} else if (strncasecmp(s,"and",3)==0) {
		ops |= AVPOPS_OP_BAND;
	} else if (strncasecmp(s,"or",2)==0) {
		ops |= AVPOPS_OP_BOR;
	} else if (strncasecmp(s,"xor",3)==0) {
		ops |= AVPOPS_OP_BXOR;
	} else if (strncasecmp(s,"not",3)==0) {
		ops |= AVPOPS_OP_BNOT;
	} else {
		LOG(L_ERR,"ERROR:avpops:parse_op_value: unknown operation "
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
		/* is variable */
		vp = avpops_parse_pvar(p, XL_THROW_ERROR|XL_DISABLE_COLORS);
		if (vp==0)
		{
			LOG(L_ERR,"ERROR:avpops:parse_op_value: unable to get"
					" pseudo-variable\n");
			goto error;
		}
		if (vp->sval.type==XL_NULL)
		{
			LOG(L_ERR,"ERROR:avops:parse_op_value: bad param; "
				"expected : $pseudo-variable or int/str value\n");
			goto error;
		}
		opd |= AVPOPS_VAL_PVAR;
		DBG("avops:parse_op_value: flag==%d/%d\n", opd, ops);
	} else {
		/* value is explicitly given */
		if ( (vp=parse_intstr_value(p,len))==0) {
			LOG(L_ERR,"ERROR:avpops:parse_op_value: unable to parse value\n");
			goto error;
		}
		if((vp->opd&AVPOPS_VAL_INT)==0) {
			LOG(L_ERR,"ERROR:avpops:parse_op_value: value must be int\n");
			goto error;
		}
	}

	/* any flags */
	p = t;
	if (p!=0 && *p!=0 )
	{
		if (*p!='/' || *(++p)==0)
			goto parse_error;
		while (*p)
		{
			switch (*p)
			{
				case 'g':
				case 'G':
					ops|=AVPOPS_FLAG_ALL;
					break;
				case 'd':
				case 'D':
					ops|=AVPOPS_FLAG_DELETE;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:parse_op_value: unknown flag "
						"<%c>\n",*p);
					goto error;
			}
			p++;
		}
	}

	vp->ops |= ops;
	vp->opd |= opd;
	return vp;
parse_error:
	LOG(L_ERR,"ERROR:avpops:parse_op_value: parse error in <%s> pos %ld\n",
		s,(long)(p-s));
error:
	if (vp) pkg_free(vp);
	return 0;
}

avpname_list_t* parse_avpname_list(char *s)
{
	avpname_list_t* head = NULL;
	avpname_list_t* al = NULL;
	char *p;
	xl_spec_t spec;

	if(s==NULL)
	{
		LOG(L_ERR, "avpops:parse_avpname_list: error - bad parameters\n");
		return NULL;
	}

	p = s;
	while(*p)
	{
		while(*p && (*p==' '||*p=='\t'||*p==','||*p==';'))
			p++;
		if(*p=='\0')
		{
			if(head==NULL)
				LOG(L_ERR,
				"avpops:parse_avpname_list: error - wrong avp name list [%s]\n",
					s);
			return head;
		}
		p = xl_parse_spec(p, &spec,
				XL_THROW_ERROR|XL_DISABLE_MULTI|XL_DISABLE_COLORS);
		if(p==NULL || spec.type!=XL_AVP)
		{
			LOG(L_ERR,
			"avpops:parse_avpname_list: error - wrong avp name list [%s]!\n",
				s);
			goto error;
		}
		al = (avpname_list_t*)pkg_malloc(sizeof(avpname_list_t));
		if(al==NULL)
		{
			LOG(L_ERR, "avpops:parse_avpname_list: error - no more memory!\n");
			goto error;
		}
		memset(al, 0, sizeof(avpname_list_t));
		memcpy(&al->sname, &spec, sizeof(xl_spec_t));

		if(head==NULL)
			head = al;
		else {
			al->next = head;
			head = al;
		}
	}

	return head;

error:
	while(head)
	{
		al = head;
		head=head->next;
		pkg_free(al);
	}
	return NULL;
}

