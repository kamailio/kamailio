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
 *  2004-11-15  added support for db schemes for avp_db_load (ramona)
 *  2004-11-17  aligned to new AVP core global aliases (ramona)
 *  2005-01-30  "fm" (fast match) operator added (ramona)
 *  2005-01-30  avp_copy (copy/move operation) added (ramona)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../parser/parse_hname2.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../error.h"
#include "avpops_parse.h"
#include "avpops_impl.h"
#include "avpops_db.h"


MODULE_VERSION

/* modules param variables */
static char *DB_URL        = 0;  /* database url */
static char *DB_TABLE      = 0;  /* table */
static int  use_domain     = 0;  /* if domain should be use for avp matching */
static char *db_columns[6] = {"uid","name","value",
                              "type","username","domain"};


static int avpops_init(void);
static int avpops_child_init(int rank);

static int register_galiases( modparam_t type, void* val);
static int fixup_db_load_avp(void** param, int param_no);
static int fixup_db_delete_avp(void** param, int param_no);
static int fixup_db_store_avp(void** param, int param_no);
static int fixup_write_avp(void** param, int param_no);
static int fixup_delete_avp(void** param, int param_no);
static int fixup_pushto_avp(void** param, int param_no);
static int fixup_check_avp(void** param, int param_no);
static int fixup_copy_avp(void** param, int param_no);

static int w_dbload_avps(struct sip_msg* msg, char* source, char* param);
static int w_dbstore_avps(struct sip_msg* msg, char* source, char* param);
static int w_dbdelete_avps(struct sip_msg* msg, char* source, char* param);
static int w_write_avps(struct sip_msg* msg, char* source, char* param);
static int w_delete_avps(struct sip_msg* msg, char* param, char *foo);
static int w_pushto_avps(struct sip_msg* msg, char* destination, char *param);
static int w_check_avps(struct sip_msg* msg, char* param, char *check);
static int w_copy_avps(struct sip_msg* msg, char* param, char *check);
static int w_print_avps(struct sip_msg* msg, char* foo, char *bar);



/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"avp_db_load", w_dbload_avps, 2, fixup_db_load_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_db_store", w_dbstore_avps, 2, fixup_db_store_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_db_delete", w_dbdelete_avps, 2, fixup_db_delete_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_write", w_write_avps, 2, fixup_write_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_delete", w_delete_avps, 1, fixup_delete_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_pushto", w_pushto_avps, 2, fixup_pushto_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_check", w_check_avps, 2, fixup_check_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_copy",  w_copy_avps, 2,  fixup_copy_avp,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{"avp_print", w_print_avps, 0, 0,
									REQUEST_ROUTE|FAILURE_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"avp_url",           PARAM_STRING, &DB_URL         },
	{"avp_table",         PARAM_STRING, &DB_TABLE       },
	{"avp_aliases",       PARAM_STRING|PARAM_USE_FUNC, (void*)register_galiases },
	{"use_domain",        PARAM_INT, &use_domain     },
	{"uid_column",        PARAM_STRING, &db_columns[0]  },
	{"name_column",       PARAM_STRING, &db_columns[1]  },
	{"value_column",      PARAM_STRING, &db_columns[2]  },
	{"type_column",       PARAM_STRING, &db_columns[3]  },
	{"username_column",   PARAM_STRING, &db_columns[4]  },
	{"domain_column",     PARAM_STRING, &db_columns[5]  },
	{"db_scheme",         PARAM_STRING|PARAM_USE_FUNC, (void*)avp_add_db_scheme },
	{0, 0, 0}
};


struct module_exports exports = {
	"avpops",
	cmds,     /* Exported functions */
	0,        /* RPC methods */
	params,   /* Exported parameters */
	avpops_init, /* Module initialization function */
	(response_function) 0,
	(destroy_function) 0,
	0,
	(child_init_function) avpops_child_init /* per-child init function */
};



static int register_galiases( modparam_t type, void* val)
{

	if (val!=0 && ((char*)val)[0]!=0)
	{
		if ( add_avp_galias_str((char*)val)!=0 )
			return -1;
	}

	return 0;
}


static int avpops_init(void)
{
	LOG(L_INFO,"AVPops - initializing\n");

	/* if DB_URL defined -> bind to a DB module */
	if (DB_URL!=0)
	{
		/* check AVP_TABLE param */
		if (DB_TABLE==0)
		{
			LOG(L_CRIT,"ERROR:avpops_init: \"AVP_DB\" present but "
				"\"AVP_TABLE\" found empty\n");
			goto error;
		}
		/* bind to the DB module */
		if (avpops_db_bind(DB_URL)<0)
			goto error;
	}

	init_store_avps( db_columns );

	return 0;
error:
	return -1;
}


static int avpops_child_init(int rank)
{
	/* init DB only if enabled */
	if (DB_URL==0)
		return 0;
	/* skip main process and TCP manager process */
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;
	/* init DB connection */
	return avpops_db_init(DB_URL, DB_TABLE, db_columns);
}



static struct fis_param *get_attr_or_alias(char *s)
{
	struct fis_param *ap;
	char *p;
	int type;
	str alias;

	/* compose the param structure */
	ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
	if (ap==0)
	{
		LOG(L_ERR,"ERROR:avpops:get_attr_or_alias: no more pkg mem\n");
		goto error;
	}
	memset( ap, 0, sizeof(struct fis_param));

	if (*s=='$')
	{
		/* alias */
		alias .s = s+1;
		alias.len = strlen(alias.s);
		if (lookup_avp_galias( &alias, &type, &ap->val)==-1)
		{
			LOG(L_ERR,"ERROR:avpops:get_attr_or_alias: unknow alias"
				"\"%s\"\n", s+1);
			goto error;
		}
		ap->flags |= (type&AVP_NAME_STR)?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
	} else {
		if ( (p=parse_avp_attr( s, ap, 0))==0 || *p!=0)
		{
			LOG(L_ERR,"ERROR:avpops:get_attr_or_alias: failed to parse "
				"attribute name <%s>\n", s);
			goto error;
		}
	}
	ap->flags |= AVPOPS_VAL_AVP;
	return ap;
error:
	return 0;
}


static int fixup_db_avp(void** param, int param_no, int allow_scheme)
{
	struct fis_param *sp;
	struct db_param  *dbp;
	int flags;
	str alias;
	char *s;
	char *p;

	flags=0;
	if (DB_URL==0)
	{
		LOG(L_ERR,"ERROR:avpops:fixup_db_avp: you have to config a db url "
			"for using avp_db_xxx functions\n");
		return E_UNSPEC;
	}

	s = (char*)*param;
	if (param_no==1)
	{
		/* prepare the fis_param structure */
		sp = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
		if (sp==0) {
			LOG(L_ERR,"ERROR:avpops:fixup_db_avp: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( sp, 0, sizeof(struct fis_param));

		if (*s!='$')
		{
			/* is a constant string -> use it as uid*/
			sp->flags = AVPOPS_VAL_STR;
			sp->val.s.s = pkg_malloc(strlen(s)+1);
			if (sp->val.s.s==0) {
				LOG(L_ERR,"ERROR:avpops:fixup_db_avp: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			sp->val.s.len = strlen(s);
			strcpy(sp->val.s.s,s);
		} else {
			/* is a variable $xxxxx */
			s++;
			if ( (p=strchr(s,'/'))!=0)
				*(p++) = 0;
			if ( (!strcasecmp( "from", s) && (flags|=AVPOPS_USE_FROM))
			|| (!strcasecmp( "to", s) && (flags|=AVPOPS_USE_TO))
			|| (!strcasecmp( "ruri", s) && (flags|=AVPOPS_USE_RURI)) )
			{
				/* check for extra flags/params */
				if (p&&(!strcasecmp("domain",p)&&!(flags|=AVPOPS_FLAG_DOMAIN)))
				{
					LOG(L_ERR,"ERROR:avpops:fixup_db_avp: unknow flag "
						"<%s>\n",p);
					return E_UNSPEC;
				}
				memset( sp, 0, sizeof(struct fis_param));
				sp->flags = flags|AVPOPS_VAL_NONE;
			} else {
				/* can be only an AVP alias */
				alias .s = s;
				alias.len = strlen(alias.s);
				if ( p || lookup_avp_galias( &alias, &flags, &sp->val)==-1 )
				{
					LOG(L_ERR,"ERROR:avpops:fixup_db_avp: source/flags \"%s\""
						" unknown!\n",s);
					return E_UNSPEC;
				}
				sp->flags = AVPOPS_VAL_AVP |
					((flags&AVP_NAME_STR)?AVPOPS_VAL_STR:AVPOPS_VAL_INT);
			}
		}
		pkg_free(*param);
		*param=(void*)sp;
	} else if (param_no==2) {
		/* compose the db_param structure */
		dbp = (struct db_param*)pkg_malloc(sizeof(struct db_param));
		if (dbp==0)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_db_avp: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( dbp, 0, sizeof(struct db_param));
		if ( parse_avp_db( s, dbp, allow_scheme)!=0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_db_avp: parse failed\n");
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)dbp;
	}

	return 0;
}


static int fixup_db_load_avp(void** param, int param_no)
{
	return fixup_db_avp( param, param_no, 1/*allow scheme*/);
}


static int fixup_db_delete_avp(void** param, int param_no)
{
	return fixup_db_avp( param, param_no, 0/*no scheme*/);
}


static int fixup_db_store_avp(void** param, int param_no)
{
	return fixup_db_avp( param, param_no, 0/*no scheme*/);
}


static int fixup_write_avp(void** param, int param_no)
{
	struct hdr_field hdr;
	struct fis_param *ap;
	int  flags;
	int  len;
	char *s;
	char *p;

	flags=0;
	s = (char*)*param;
	ap = 0 ;

	if (param_no==1)
	{
		if ( *s=='$' )
		{
			/* is variable */
			if ((++s)==0)
			{
				LOG(L_ERR,"ERROR:avops:fixup_write_avp: bad param 1; "
					"expected : $[from|to|ruri|hdr] or int/str value\n");
				return E_UNSPEC;
			}
			if ( (p=strchr(s,'/'))!=0)
				*(p++) = 0;
			if ( (!strcasecmp( "from", s) && (flags|=AVPOPS_USE_FROM))
				|| (!strcasecmp( "to", s) && (flags|=AVPOPS_USE_TO))
				|| (!strcasecmp( "ruri", s) && (flags|=AVPOPS_USE_RURI))
				|| (!strcasecmp( "src_ip", s) && (flags|=AVPOPS_USE_SRC_IP))
				|| (!strcasecmp( "dst_ip", s) && (flags|=AVPOPS_USE_DST_IP))
				|| (!strncasecmp( "hdr", s, 3) && (flags|=AVPOPS_USE_HDRREQ)) )
			{
				ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
				if (ap==0)
				{
					LOG(L_ERR,"ERROR:avpops:fixup_write_avp: no more "
						"pkg mem\n");
					return E_OUT_OF_MEM;
				}
				memset( ap, 0, sizeof(struct fis_param));
				/* any falgs ? */
				if(p && !((flags&
				(AVPOPS_USE_SRC_IP|AVPOPS_USE_DST_IP|AVPOPS_USE_HDRREQ))==0
				&& ((!strcasecmp("username",p) && (flags|=AVPOPS_FLAG_USER)) ||
				(!strcasecmp("domain", p) && (flags|=AVPOPS_FLAG_DOMAIN)))) )
				{
					LOG(L_ERR,"ERROR:avpops:fixup_write_avp: flag \"%s\""
						" unknown!\n", p);
					return E_UNSPEC;
				}
				if (flags&AVPOPS_USE_HDRREQ)
				{
					len = strlen(s);
					if (len<6 || s[3]!='[' || s[len-1]!=']')
					{
						LOG(L_ERR,"ERROR:avpops:fixup_write_avp: invalid hdr "
							"specificatoin \"%s\"\n",s);
						return E_UNSPEC;
					}
					s[len-1] = ':';
					/* parse header name */
					if (parse_hname2( s+4, s+len, &hdr)==0) {
						LOG(L_ERR,"BUG:avpops:fixup_write_avp: parse header "
							"failed\n");
						return E_UNSPEC;
					}
					if (hdr.type==HDR_OTHER_T) {
						/* duplicate hdr name */
						len -= 5; /*hdr[]*/
						ap->val.s.s = pkg_malloc(len+1);
						if (ap->val.s.s==0)
						{
							LOG(L_ERR,"ERROR:avpops:fixup_write_avp: no more "
								"pkg mem\n");
							return E_OUT_OF_MEM;
						}
						ap->val.s.len = len;
						memcpy( ap->val.s.s, s+4, len);
						ap->val.s.s[len] = 0;
						DBG("DEBUF:avpops:fixup_write_avp: hdr=<%s>\n",
							ap->val.s.s);
					} else {
						ap->val.n = hdr.type;
						flags |= AVPOPS_VAL_INT;
					}
				}
				ap->flags = flags|AVPOPS_VAL_NONE;
			} else {
				LOG(L_ERR,"ERROR:avpops:fixup_write_avp: source \"%s\""
					" unknown!\n", s);
				return E_UNSPEC;
			}
		} else {
			/* is value */
			if ( (ap=parse_intstr_value(s,strlen(s)))==0 )
			{
				LOG(L_ERR,"ERROR:avops:fixup_write_avp: bad param 1; "
					"expected : $[from|to|ruri] or int/str value\n");
				return E_UNSPEC;
			}
		}
	} else if (param_no==2) {
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_write_avp: bad attribute name"
				"/alias <%s>\n", s);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->flags&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_write_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}


static int fixup_delete_avp(void** param, int param_no)
{
	struct fis_param *ap;
	char *p;

	if (param_no==1) {
		/* attribute name / alias */
		if ( (p=strchr((char*)*param,'/'))!=0 )
			*(p++)=0;
		if ( (ap=get_attr_or_alias((char*)*param))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_delete_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p)
			{
				case 'g':
				case 'G':
					ap->flags|=AVPOPS_FLAG_ALL;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_delete_avp: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
		/* force some flags: if no avp name is given, force "all" flag */
		if (ap->flags&AVPOPS_VAL_NONE)
			ap->flags |= AVPOPS_FLAG_ALL;

		pkg_free(*param);
		*param=(void*)ap;
	}

	return 0;
}


static int fixup_pushto_avp(void** param, int param_no)
{
	struct fis_param *ap;
	char *s;
	char *p;

	s = (char*)*param;
	ap = 0;

	if (param_no==1)
	{
		if ( *s!='$' || (++s)==0)
		{
			LOG(L_ERR,"ERROR:avops:fixup_pushto_avp: bad param 1; expected : "
				"$[ruri|hdr_name|..]\n");
			return E_UNSPEC;
		}
		/* compose the param structure */
		ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
		if (ap==0)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( ap, 0, sizeof(struct fis_param));

		if ( (p=strchr((char*)*param,'/'))!=0 )
			*(p++)=0;
		if (!strcasecmp( "ruri", s))
		{
			ap->flags = AVPOPS_VAL_NONE|AVPOPS_USE_RURI;
			if ( p && !(
				(!strcasecmp("username",p) && (ap->flags|=AVPOPS_FLAG_USER)) ||
				(!strcasecmp("domain",p) && (ap->flags|=AVPOPS_FLAG_DOMAIN)) ))
			{
				LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: unknown "
					" ruri flag \"%s\"!\n",p);
				return E_UNSPEC;
			}
		} else {
			/* what's the hdr destination ? request or reply? */
			if ( p==0 )
			{
				ap->flags = AVPOPS_USE_HDRREQ;
			} else {
				if (!strcasecmp( "request", p))
					ap->flags = AVPOPS_USE_HDRREQ;
				else if (!strcasecmp( "reply", p))
					ap->flags = AVPOPS_USE_HDRRPL;
				else
				{
					LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: header "
						"destination \"%s\" unknown!\n",p);
					return E_UNSPEC;
				}
			}
			/* copy header name */
			ap->val.s.s = pkg_malloc(strlen(s) + 1 );
			if (ap->val.s.s==0)
			{
				LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			ap->val.s.len = strlen(s);
			strcpy( ap->val.s.s, s);
		}
	} else if (param_no==2) {
		/* attribute name / alias */
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->flags&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p) {
				case 'g':
				case 'G':
					ap->flags|=AVPOPS_FLAG_ALL;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}



static int fixup_check_avp(void** param, int param_no)
{
	struct fis_param *ap;
	regex_t* re;
	char *s;

	s = (char*)*param;
	ap = 0;

	if (param_no==1)
	{
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_check_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->flags&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_check_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
	} else if (param_no==2) {
		if ( (ap=parse_check_value(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_check_avp: failed to parse "
				"checked value \n");
			return E_UNSPEC;
		}
		/* if REGEXP op -> compile the expresion */
		if (ap->flags&AVPOPS_OP_RE)
		{
			if ( (ap->flags&AVPOPS_VAL_STR)==0 )
			{
				LOG(L_ERR,"ERROR:avpops:fixup_check_avp: regexp operation "
					"requires string value\n");
				return E_UNSPEC;
			}
			re = pkg_malloc(sizeof(regex_t));
			if (re==0)
			{
				LOG(L_ERR,"ERROR:avpops:fixup_check_avp: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			DBG("DEBUG:avpops:fixup_check_avp: compiling regexp <%s>\n",
				ap->val.s.s);
			if (regcomp(re, ap->val.s.s, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
			{
				pkg_free(re);
				LOG(L_ERR,"ERROR:avpops:fixip_check_avp: bad re <%s>\n",
					ap->val.s.s);
				return E_BAD_RE;
			}
			/* free the string and link the regexp */
			pkg_free(ap->val.s.s);
			ap->val.s.s = (char*)re;
		} else if (ap->flags&AVPOPS_OP_FM) {
			if ( !( ap->flags&AVPOPS_VAL_AVP ||
			(!(ap->flags&AVPOPS_VAL_AVP) && ap->flags&AVPOPS_VAL_STR) ) )
			{
				LOG(L_ERR,"ERROR:avpops:fixup_check_avp: fast_match operation "
					"requires string value or avp name/alias (%d)\n",
					ap->flags);
				return E_UNSPEC;
			}
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}


static int fixup_copy_avp(void** param, int param_no)
{
	struct fis_param *ap;
	char *s;
	char *p;

	s = (char*)*param;
	ap = 0;
	p = 0;

	if (param_no==2)
	{
		/* avp / flags */
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;
	}

	if ( (ap=get_attr_or_alias(s))==0 )
	{
		LOG(L_ERR,"ERROR:avpops:fixup_copy_avp: bad attribute name"
			"/alias <%s>\n", (char*)*param);
		return E_UNSPEC;
	}
	/* attr name is mandatory */
	if (ap->flags&AVPOPS_VAL_NONE)
	{
		LOG(L_ERR,"ERROR:avpops:fixup_copy_avp: you must specify "
			"a name for the AVP\n");
		return E_UNSPEC;
	}

	if (param_no==2)
	{
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p) {
				case 'g':
				case 'G':
					ap->flags|=AVPOPS_FLAG_ALL;
					break;
				case 'd':
				case 'D':
					ap->flags|=AVPOPS_FLAG_DELETE;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_copy_avp: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}


static int w_dbload_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_dbload_avps ( msg, (struct fis_param*)source,
								(struct db_param*)param, use_domain);
}


static int w_dbstore_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_dbstore_avps ( msg, (struct fis_param*)source,
								(struct db_param*)param, use_domain);
}


static int w_dbdelete_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_dbdelete_avps ( msg, (struct fis_param*)source,
								(struct db_param*)param, use_domain);
}


static int w_write_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_write_avp ( msg, (struct fis_param*)source,
								(struct fis_param*)param);
}


static int w_delete_avps(struct sip_msg* msg, char* param, char* foo)
{
	return ops_delete_avp ( msg, (struct fis_param*)param);
}


static int w_pushto_avps(struct sip_msg* msg, char* destination, char *param)
{
	return ops_pushto_avp ( msg, (struct fis_param*)destination,
								(struct fis_param*)param);
}


static int w_check_avps(struct sip_msg* msg, char* param, char *check)
{
	return ops_check_avp ( msg, (struct fis_param*)param,
								(struct fis_param*)check);
}

static int w_copy_avps(struct sip_msg* msg, char* name1, char *name2)
{
	return ops_copy_avp ( msg, (struct fis_param*)name1,
								(struct fis_param*)name2);
}

static int w_print_avps(struct sip_msg* msg, char* foo, char *bar)
{
	return ops_print_avp();
}


