/*
 * $Id$
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of SIP Express Router.
 *
 * UAC SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2005-01-31  first version (ramona)
 */


#include <ctype.h>

#include "../../parser/parse_from.h"
#include "../../mem/mem.h"
#include "../../data_lump.h"
#include "../../modules/tm/h_table.h"
#include "../../modules/tm/tm_load.h"

#include "from.h"
#define  FL_FROM_ALTERED  (1<<31)

extern str from_param;
extern int from_restore_mode;
extern struct tm_binds uac_tmb;

static char enc_table64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz0123456789+/";

static int dec_table64[256];

#define text3B64_len(_l)   (( ((_l) / 3) + ((_l) % 3 ? 1 : 0) ) << 2)

void init_from_replacer()
{
	int i;

	for( i=0 ; i<256 ; i++)
		dec_table64[i] = -1;
	for ( i=0 ; i<64; i++)
		dec_table64[(unsigned char)enc_table64[i]] = i;
	}


static inline int encode_from( str *src, str *dst )
{
	static char buf[text3B64_len(MAX_URI_SIZE)];
	int  idx;
	int  left;
	int  block;
	int  i,r;
	char *p;

	dst->len = text3B64_len( src->len );
	dst->s = buf;
	if (dst->len>text3B64_len(MAX_URI_SIZE))
	{
		LOG(L_ERR,"ERROR:uac:encode_from: uri too long\n");
		return -1;
	}

	for ( idx=0, p=buf ; idx<src->len ; idx+=3)
	{
		left = src->len - idx - 1;
		left = (left>1? 2 : left);

		/* Collect 1 to 3 bytes to encode */
		block = 0;
		for ( i=0,r= 16 ; i<=left ; i++,r-=8 )
		{
			block += ((unsigned char)src->s[idx+i]) << r;
		}

		/* Encode into 2-4 chars appending '=' if not enough data left.*/
		*(p++) = enc_table64[(block >> 18) & 0x3f];
		*(p++) = enc_table64[(block >> 12) & 0x3f];
		*(p++) = left > 0 ? enc_table64[(block >> 6) & 0x3f] : '-';
		*(p++) = left > 1 ? enc_table64[block & 0x3f] : '-';
	}
	return 0;
}


static inline int decode_from( str *src , str *dst)
{
	static char buf[MAX_URI_SIZE];
	int block;
	int n;
	int idx;
	int end;
	int i,j;
	signed char c;

	/* Count '-' at end and disregard them */
	for( n=0,i=src->len-1; src->s[i]=='-'; i--)
		n++;

	dst->len = ((src->len * 6) >> 3) - n;
	dst->s = buf;
	if (dst->len>MAX_URI_SIZE)
	{
		LOG(L_ERR,"ERROR:uac:decode_from: uri too long\n");
		return -1;
	}

	end = src->len - n;
	for ( i=0,idx=0 ; i<end ; idx+=3 )
	{
		/* Assemble three bytes into an int from four "valid" characters */
		block = 0;
		for ( j=0; j<4 && i<end ; j++)
		{
			c = dec_table64[(unsigned char)src->s[i++]];
			if ( c<0 )
			{
				LOG(L_ERR,"ERROR:uac:decode_from: invalid base64 string "
					"\"%.*s\"\n",src->len,src->s);
				return -1;
			}
			block += c << (18 - 6*j);
		}

		/* Add the bytes */
		for ( j=0,n=16 ; j<3 && idx+j< dst->len; j++,n-=8 )
			buf[idx+j] = (char) ((block >> n) & 0xff);
	}

	return 0;

}


/* 
 * if display name does not exist, then from_dsp is ignored
 */
int replace_from( struct sip_msg *msg, str *from_dsp, str *from_uri)
{
	struct to_body *from;
	struct lump* l;
	str replace;
	char *p;
	str param;
	int offset;

	/* parse original from hdr */
	if (parse_from_header(msg)!=0 )
	{
		LOG(L_ERR,"ERROR:uac:replace_from: failed to find/parse FROM hdr\n");
		goto error;
	}
	from = (struct to_body*)msg->from->parsed;
	/* some validity checks */
	if (from->param_lst==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: broken FROM hdr; no tag param\n");
		goto error;
	}

	/* first deal with display name */
	if (from_dsp && from->display.len)
	{
		/* must be replaced/ removed */
		l = 0;
		/* there is already a display -> remove it */
		DBG("DEBUG:uac:replace_from: removing display [%.*s]\n",
			from->display.len,from->display.s);
		/* build del lump */
		l = del_lump( msg, from->display.s-msg->buf, from->display.len, 0);
		if (l==0)
		{
			LOG(L_ERR,"ERROR:uac:replace_from: display del lump failed\n");
			goto error;
		}
		/* some new display to set? */
		if (from_dsp->s)
		{
			if (l==0)
			{
				/* add anchor just before uri's "<" */
				offset = from->uri.s - msg->buf;
				while( msg->buf[offset]!='<')
				{
					offset--;
					if (from->body.s>msg->buf+offset)
					{
						LOG(L_ERR,"ERROR:uac:replace_from: no <> and there"
								" is dispaly name\n");
						goto error;
					}
				}
				if ( (l=anchor_lump( msg, offset, 0, 0))==0)
				{
					LOG(L_ERR,"ERROR:uac:replace_from: display anchor lump "
						"failed\n");
					goto error;
				}
			}
			p = pkg_malloc( from_dsp->len);
			if (p==0)
			{
				LOG(L_ERR,"ERROR:uac:replace_from: no more pkg mem\n");
				goto error;
			}
			memcpy( p, from_dsp->s, from_dsp->len); 
			if (insert_new_lump_after( l, p, from_dsp->len, 0)==0)
			{
				LOG(L_ERR,"ERROR:uac:replace_from: insert new "
					"display lump failed\n");
				pkg_free(p);
				goto error;
			}
		}
	}

	/* now handle the URI */
	DBG("DEBUG:uac:replace_from: uri to replace [%.*s]\n",
		from->uri.len, from->uri.s);
	DBG("DEBUG:uac:replace_from: replacement uri is [%.*s]\n",
		from_uri->len, from_uri->s);

	/* build del/add lumps */
	if ((l=del_lump( msg, from->uri.s-msg->buf, from->uri.len, 0))==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: del lump failed\n");
		goto error;
	}
	p = pkg_malloc( from_uri->len);
	if (p==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: no more pkg mem\n");
		goto error;
	}
	memcpy( p, from_uri->s, from_uri->len); 
	if (insert_new_lump_after( l, p, from_uri->len, 0)==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: insert new lump failed\n");
		pkg_free(p);
		goto error;
	}

	if (from_restore_mode==FROM_NO_RESTORE)
		return 0;

	/*add parameter lump */
	if (encode_from( &from->uri , &replace)<0 )
	{
		LOG(L_ERR,"ERROR:uac:replace_from: failed to encode uri\n");
		goto error;
	}
	DBG("encode is=<%.*s> len=%d\n",replace.len,replace.s,replace.len);
	offset = from->last_param->value.s+from->last_param->value.len-msg->buf;
	if ( (l=anchor_lump( msg, offset, 0, 0))==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: anchor lump failed\n");
		goto error;
	}
	param.len = 1+from_param.len+1+replace.len;
	param.s = (char*)pkg_malloc(param.len);
	if (param.s==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: no more pkg mem\n");
		goto error;
	}
	p = param.s;
	*(p++) = ';';
	memcpy( p, from_param.s, from_param.len);
	p += from_param.len;
	*(p++) = '=';
	memcpy( p, replace.s, replace.len);
	p += replace.len;
	if (insert_new_lump_after( l, param.s, param.len, 0)==0)
	{
		LOG(L_ERR,"ERROR:uac:replace_from: insert new lump failed\n");
		pkg_free(param.s);
		goto error;
	}
	msg->msg_flags |= FL_FROM_ALTERED;

	return 0;
error:
	return -1;
}


/*
 * return  0 - replaced
 *        -1 - not replaced or error
 */
int restore_from( struct sip_msg *msg, int is_req)
{
	struct to_body *ft_hdr;
	struct to_param *param;
	struct lump* l;
	str replace;
	str restore;
	str del;
	char *p;

	/* for replies check the from, for requests check to! */
	if (!is_req)
	{
		/* parse original from hdr */
		if (parse_from_header(msg)!=0 )
		{
			LOG(L_ERR,"ERROR:uac:restore_from: failed to find/parse "
				"FROM hdr\n");
			goto failed;
		}
		ft_hdr = (struct to_body*)msg->from->parsed;
	} else {
		if ( !msg->to && (parse_headers(msg,HDR_TO_F,0)==-1 || !msg->to))
		{
			LOG(L_ERR,"ERROR:uac:restore_from: bad msg or missing TO hdr\n");
			goto failed;
		}
		ft_hdr = (struct to_body*)msg->to->parsed;
	}

	/* check if it has the param */
	for( param=ft_hdr->param_lst ; param ; param=param->next )
		if (param->name.len==from_param.len &&
		strncmp(param->name.s, from_param.s, from_param.len)==0)
			break;

	if (param==0)
		goto failed;

	/* determin what to replace */
	replace.s = ft_hdr->uri.s;
	replace.len = ft_hdr->uri.len;
	DBG("DEBUG:uac:restore_from: replacing [%.*s]\n",
		replace.len, replace.s);

	/* build del/add lumps */
	if ((l=del_lump( msg, replace.s-msg->buf, replace.len, 0))==0)
	{
		LOG(L_ERR,"ERROR:uac:restore_from: del lump failed\n");
		goto failed;
	}

	/* calculate the restore from */
	if (decode_from( &param->value, &restore)<0 )
	{
		LOG(L_ERR,"ERROR:uac:restore_from: failed to dencode uri\n");
		goto failed;
	}
	DBG("DEBUG:uac:restore_from: replacement is [%.*s]\n",
		replace.len, replace.s);

	p = pkg_malloc( restore.len);
	if (p==0)
	{
		LOG(L_ERR,"ERROR:uac:restore_from: no more pkg mem\n");
		goto failed;
	}
	memcpy( p, restore.s, restore.len);
	if (insert_new_lump_after( l, p, restore.len, 0)==0)
	{
		LOG(L_ERR,"ERROR:uac:restore_from: insert new lump failed\n");
		pkg_free(p);
		goto failed;
	}

	/* delete parameter */
	del.s = param->name.s;
	while ( *del.s!=';')  del.s--;
	del.len = (int)(long)(param->value.s + param->value.len - del.s);
	DBG("DEBUG:uac:restore_from: deleting [%.*s]\n",del.len,del.s);
	if ((l=del_lump( msg, del.s-msg->buf, del.len, 0))==0) 
	{
		LOG(L_ERR,"ERROR:uac:restore_from: del lump failed\n");
		goto failed;
	}

	return 0;
failed:
	return -1;
}



/************************** TMCB functions ******************************/

static int rst_from = 1;
static int rst_to = 2;

void correct_reply(struct cell* t, int type, struct tmcb_params *p);

void tr_checker(struct cell* t, int type, struct tmcb_params *param)
{
	DBG("---------------------- inside tr_checker\n");
	if ( t && param->req )
	{
		DBG("*************** marker **************\n");
		/* is the request marked with FROM altered flag? */
		if (param->req->msg_flags&FL_FROM_ALTERED) {
			/* need to put back in replies the FROM from UAS */
			/* in callback we need FROM to be parsed- it's already done 
			 * by replace_from() function */
			DBG("*************** marker **************\n");
			if ( uac_tmb.register_tmcb( 0, t, TMCB_RESPONSE_IN,
						correct_reply, (void*)&rst_from, 0)!=1 )
			{
				LOG(L_ERR,"ERROR:uac:tr_checker: failed to install "
					"TM callback\n");
				return;
			}
		} else {
			/* check if the request contains the restore_from tag */
			if ( restore_from( param->req , 1)==0 )
			{
				/* in callback we need TO to be parsed- it's already done 
				 * by restore_from() function */
				/* restore in req performed -> replace in reply */
				if ( uac_tmb.register_tmcb( 0, t, TMCB_RESPONSE_IN,
							correct_reply, (void*)&rst_to, 0)!=1 )
				{
					LOG(L_ERR,"ERROR:uac:tr_checker: failed to install "
						"TM callback\n");
					return;
				}
			}
		}
	}
}


/* take the original FROM URI from UAS and put it in reply */
void correct_reply(struct cell* t, int type, struct tmcb_params *p)
{
	struct lump* l;
	struct to_param *param;
	struct sip_msg *req;
	str src;
	str dst;
	str dsrc;
	str del;

	DBG("---------------------- inside correct_reply\n");

	if ( !t || !t->uas.request || !p->rpl )
		return;

	req = t->uas.request;

	if ( (**(int**)p->param)==rst_from )
	{
		/* copy FROM uri : req -> rpl */
		if ( !req->from || !req->from->parsed)
		{
			LOG(L_CRIT,"BUG:uac:correct_reply: FROM is not already parsed\n");
			return;
		}
		/* parse FROM in reply */
		if (parse_from_header( p->rpl )!=0 )
		{
			LOG(L_ERR,"ERROR:uac:correct_reply: failed to find/parse "
				"FROM hdr\n");
			return;
		}
		/* remove the parameter */
		param=((struct to_body*)p->rpl->from->parsed)->param_lst;
		for( ; param ; param=param->next )
		{
			DBG("***** param=<%.*s>=<%.*s>,%p\n",param->name.len,param->name.s,
					param->value.len,param->value.s,param->next);
			if (param->name.len==from_param.len &&
			strncmp(param->name.s, from_param.s, from_param.len)==0)
			{
				del.s = param->name.s;
				while ( *del.s!=';')  del.s--;
				del.len = (int)(long)(param->value.s+param->value.len-del.s);
				DBG("DEBUG:uac:correct_reply: deleting [%.*s]\n",
					del.len,del.s);
				if ((l=del_lump( p->rpl, del.s-p->rpl->buf, del.len, 0))==0)
					LOG(L_ERR,"ERROR:uac:correct_reply: del lump failed\n");
				break;
			}
		}

		src = ((struct to_body*)req->from->parsed)->uri;
		dst = ((struct to_body*)p->rpl->from->parsed)->uri;
	} else {
		/* copy TO uri : req -> rpl */
		if ( !req->to || !req->to->parsed)
		{
			LOG(L_CRIT,"BUG:uac:correct_reply: TO is not already parsed\n");
			return;
		}
		/* parse TO in reply */
		if (!p->rpl->to && (parse_headers(p->rpl,HDR_TO_F,0)==-1||!p->rpl->to))
		{
			LOG(L_ERR,"ERROR:uac:correct_reply: failed to find/parse "
				"TO hdr\n");
			return;
		}
		src = ((struct to_body*)req->to->parsed)->uri;
		dst = ((struct to_body*)p->rpl->to->parsed)->uri;
	}

	DBG("DEBUG:correct_reply: replacing <%.*s> with <%.*s>\n",
			dst.len,dst.s, src.len,src.s);

	/* duplicate the src data */
	dsrc.len = src.len;
	dsrc.s = (char*)pkg_malloc(dsrc.len);
	if (dsrc.s==0) {
		LOG(L_ERR,"ERROR:uac:correct_reply: no more pkg mem\n");
		return;
	}
	memcpy( dsrc.s, src.s, src.len);
	/* build del/add lumps */
	if ((l=del_lump( p->rpl, dst.s-p->rpl->buf, dst.len, 0))==0)
	{
		LOG(L_ERR,"ERROR:uac:correct_reply: del lump failed\n");
		return;
	}
	if (insert_new_lump_after( l, dsrc.s, dsrc.len, 0)==0)
	{
		LOG(L_ERR,"ERROR:uac:correct_reply: insert new lump failed\n");
		pkg_free( dsrc.s );
		return;
	}

	return;
}

