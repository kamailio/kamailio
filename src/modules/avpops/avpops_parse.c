/*
 * Copyright (C) 2004-2006 Voice Sistem SRL
 *
 * This file is part of Kamailio.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
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

struct fis_param *avpops_parse_pvar(char *in)
{
	struct fis_param *ap;
	str s;

	/* compose the param structure */
	ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
	if (ap==0)
	{
		LM_ERR("no more pkg mem\n");
		return NULL;
	}
	memset( ap, 0, sizeof(struct fis_param));
	s.s = in; s.len = strlen(s.s);
	ap->u.sval = pv_cache_get(&s);
	if(ap->u.sval==NULL)
	{
		pkg_free(ap);
		return NULL;
	}

	ap->opd |= AVPOPS_VAL_PVAR;
	ap->type = AVPOPS_VAL_PVAR;
	return ap;
}


int parse_avp_db(char *s, struct db_param *dbp, int allow_scheme)
{
	unsigned long ul;
	str   tmp;
	str   s0;
	char  have_scheme;
	char *p0;
	unsigned int flags;

	tmp.s = s;
	/* parse the attribute name - check first if it's not an alias */
	p0=strchr(tmp.s, '/');
	if(p0!=NULL)
		*p0=0;
	if ( *s!='$')
	{
		if(strlen(s)<1)
		{
			LM_ERR("bad param - expected : $avp(name), *, s or i value\n");
			return E_UNSPEC;
		}
		switch(*s) {
			case 's': case 'S':
				dbp->a.opd = AVPOPS_VAL_NONE|AVPOPS_VAL_STR;
			break;
			case 'i': case 'I':
				dbp->a.opd = AVPOPS_VAL_NONE|AVPOPS_VAL_INT;
			break;
			case '*': case 'a': case 'A':
				dbp->a.opd = AVPOPS_VAL_NONE;
			break;
			default:
				LM_ERR("bad param - expected : *, s or i AVP flag\n");
			return E_UNSPEC;
		}
		/* flags */
		flags = 0;
		if(*(s+1)!='\0')
		{
			s0.s = s+1;
			s0.len = strlen(s0.s);
			if(str2int(&s0, &flags)!=0)
			{
				LM_ERR("error - bad avp flags\n");
				goto error;
			}
		}
		/* no pv to lookup, create one to store flags details */
		dbp->a.u.sval = (pv_spec_t*)pkg_malloc(sizeof(pv_spec_t));
		if(dbp->a.u.sval==NULL)
		{
			LM_ERR("no more pkg\n");
			goto error;
		}
		memset(dbp->a.u.sval, 0, sizeof(pv_spec_t));
		dbp->a.u.sval->pvp.pvn.u.isname.type |= (flags<<8)&0xff00;
		dbp->a.type = AVPOPS_VAL_NONE;
	} else {
		s0.s = s; s0.len = strlen(s0.s);
		dbp->a.u.sval = pv_cache_get(&s0);
		if (dbp->a.u.sval==0 || dbp->a.u.sval->type!=PVT_AVP)
		{
			LM_ERR("bad param - expected : $avp(name) or int/str value\n");
			return E_UNSPEC;
		}
		dbp->a.type = AVPOPS_VAL_PVAR;
	}

	/* optimize and keep the attribute name as str also to
	 * speed up db querie builds */
	if (dbp->a.type == AVPOPS_VAL_PVAR)
	{
		dbp->a.opd = AVPOPS_VAL_PVAR;
		if(pv_has_sname(dbp->a.u.sval))
		{
			dbp->sa.s=(char*)pkg_malloc(
					dbp->a.u.sval->pvp.pvn.u.isname.name.s.len+1);
			if (dbp->sa.s==0)
			{
				LM_ERR("no more pkg mem\n");
				goto error;
			}
			memcpy(dbp->sa.s, dbp->a.u.sval->pvp.pvn.u.isname.name.s.s,
					dbp->a.u.sval->pvp.pvn.u.isname.name.s.len);
			dbp->sa.len = dbp->a.u.sval->pvp.pvn.u.isname.name.s.len;
			dbp->sa.s[dbp->sa.len] = 0;
			dbp->a.opd = AVPOPS_VAL_PVAR|AVPOPS_VAL_STR;
		} else if(pv_has_iname(dbp->a.u.sval)) {
			ul = (unsigned long)dbp->a.u.sval->pvp.pvn.u.isname.name.n;
			tmp.s = int2str( ul, &(tmp.len) );
			dbp->sa.s = (char*)pkg_malloc( tmp.len + 1 );
			if (dbp->sa.s==0)
			{
				LM_ERR("no more pkg mem\n");
				goto error;
			}
			memcpy( dbp->sa.s, tmp.s, tmp.len);
			dbp->sa.len = tmp.len;
			dbp->sa.s[dbp->sa.len] = 0;
			dbp->a.opd = AVPOPS_VAL_PVAR|AVPOPS_VAL_INT;
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
				LM_ERR("function doesn't support DB schemes\n");
				goto error;
			}
			if (dbp->a.opd&AVPOPS_VAL_NONE)
			{
				LM_ERR("inconsistent usage of "
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
			LM_ERR("empty scheme/table name\n");
			goto error;
		}
		if (have_scheme)
		{
			dbp->scheme = avp_get_db_scheme( &tmp );
			if (dbp->scheme==0) 
			{
				LM_ERR("scheme <%s> not found\n", tmp.s);
				goto error;
			}
			/* update scheme flags with AVP name type*/
			dbp->scheme->db_flags|=dbp->a.opd&AVPOPS_VAL_STR?AVP_NAME_STR:0;
		} else {
			/* duplicate table as str */
			pkg_str_dup(&dbp->table, &tmp);
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

	if (len>1 && *(p+1)==':')
	{
		if (*p=='i' || *p=='I')
			flags = AVPOPS_VAL_INT;
		else if (*p=='s' || *p=='S')
			flags = AVPOPS_VAL_STR;
		else
		{
			LM_ERR("unknown value type <%c>\n",*p);
			goto error;
		}
		p += 2;
		len -= 2;
		if (*p==0 || len<=0 )
		{
			LM_ERR("parse error arround <%.*s>\n",len,p);
				goto error;
		}
	} else {
		flags = AVPOPS_VAL_STR;
	}
	/* get the value */
	vp = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
	if (vp==0)
	{
		LM_ERR("no more pkg mem\n");
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
				LM_ERR("value is not hex int as type says <%.*s>\n", 
						val_str.len, val_str.s);
				goto error;
			}
		} else {
			if(str2sint( &val_str, (int*)&uint)==-1)
			{
				LM_ERR("value is not int"
					" as type says <%.*s>\n", val_str.len, val_str.s);
				goto error;
			}
		}
		vp->u.n = (int)uint;
		vp->type = AVPOPS_VAL_INT;
	} else {
		/* duplicate the value as string */
		vp->u.s.s = (char*)pkg_malloc((val_str.len+1)*sizeof(char));
		if (vp->u.s.s==0)
		{
			LM_ERR("no more pkg mem\n");
			goto error;
		}
		vp->u.s.len = val_str.len;
		memcpy(vp->u.s.s, val_str.s, val_str.len);
		vp->u.s.s[vp->u.s.len] = 0;
		vp->type = AVPOPS_VAL_STR;
	}

	return vp;
error:
	return 0;
}


#define  duplicate_str(_p, _str, _error) \
	do { \
		_p.s = (char*)pkg_malloc(_str.len+1); \
		if (_p.s==0) \
		{ \
			LM_ERR("no more pkg memory\n");\
			goto _error; \
		} \
		_p.len = _str.len; \
		memcpy( _p.s, _str.s, _str.len); \
		_p.s[_str.len] = 0; \
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
			if (scheme->uuid_col.s) goto parse_error;
			duplicate_str( scheme->uuid_col, bar, error);
		} else
		if ( foo.len==SCHEME_USERNAME_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_USERNAME_COL, foo.len) )
		{
			if (scheme->username_col.s) goto parse_error;
			duplicate_str( scheme->username_col, bar, error);
		} else
		if ( foo.len==SCHEME_DOMAIN_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_DOMAIN_COL, foo.len) )
		{
			if (scheme->domain_col.s) goto parse_error;
			duplicate_str( scheme->domain_col, bar, error);
		} else
		if ( foo.len==SCHEME_VALUE_COL_LEN && 
		!strncasecmp( foo.s, SCHEME_VALUE_COL, foo.len) )
		{
			if (scheme->value_col.s) goto parse_error;
			duplicate_str( scheme->value_col, bar, error);
		} else
		if ( foo.len==SCHEME_TABLE_LEN && 
		!strncasecmp( foo.s, SCHEME_TABLE, foo.len) )
		{
			if (scheme->table.s) goto parse_error;
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
				LM_ERR("unknown value type <%.*s>\n",bar.len,bar.s);
				goto error;
			}
		} else {
			LM_ERR("unknown attribute <%.*s>\n",foo.len,foo.s);
			goto error;
		}
	} /* end while */

	return 0;
parse_error:
	LM_ERR("parse error in <%s> around %ld\n", s, (long)(p-s));
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
		LM_ERR("unknown operation <%.*s>\n",2,s);
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
		vp = avpops_parse_pvar(p);
		if (vp==0)
		{
			LM_ERR("unable to get pseudo-variable\n");
			goto error;
		}
		if (vp->u.sval->type==PVT_NULL)
		{
			LM_ERR("bad param; expected : $pseudo-variable or int/str value\n");
			goto error;
		}
		opd |= AVPOPS_VAL_PVAR;
		LM_DBG("flag==%d/%d\n", opd, ops);
	} else {
		/* value is explicitly given */
		if ( (vp=parse_intstr_value(p,len))==0) {
			LM_ERR("unable to parse value\n");
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
					LM_ERR("unknown flag <%c>\n",*p);
					goto error;
			}
			p++;
		}
	}

	vp->ops |= ops;
	vp->opd |= opd;
	return vp;
parse_error:
	LM_ERR("parse error in <%s> pos %ld\n", s,(long)(p-s));
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
	} else if (strncasecmp(s,"mod",3)==0) {
		ops |= AVPOPS_OP_MOD;
	} else if (strncasecmp(s,"and",3)==0) {
		ops |= AVPOPS_OP_BAND;
	} else if (strncasecmp(s,"or",2)==0) {
		ops |= AVPOPS_OP_BOR;
	} else if (strncasecmp(s,"xor",3)==0) {
		ops |= AVPOPS_OP_BXOR;
	} else if (strncasecmp(s,"not",3)==0) {
		ops |= AVPOPS_OP_BNOT;
	} else {
		LM_ERR("unknown operation <%.*s>\n",2,s);
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
		vp = avpops_parse_pvar(p);
		if (vp==0)
		{
			LM_ERR("unable to get pseudo-variable\n");
			goto error;
		}
		if (vp->u.sval->type==PVT_NULL)
		{
			LM_ERR("bad param; expected : $pseudo-variable or int/str value\n");
			goto error;
		}
		opd |= AVPOPS_VAL_PVAR;
		LM_DBG("flag==%d/%d\n", opd, ops);
	} else {
		/* value is explicitly given */
		if ( (vp=parse_intstr_value(p,len))==0) {
			LM_ERR("unable to parse value\n");
			goto error;
		}
		if((vp->opd&AVPOPS_VAL_INT)==0) {
			LM_ERR("value must be int\n");
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
					LM_ERR("unknown flag <%c>\n",*p);
					goto error;
			}
			p++;
		}
	}

	vp->ops |= ops;
	vp->opd |= opd;
	return vp;
parse_error:
	LM_ERR("parse error in <%s> pos %ld\n", s,(long)(p-s));
error:
	if (vp) pkg_free(vp);
	return 0;
}

