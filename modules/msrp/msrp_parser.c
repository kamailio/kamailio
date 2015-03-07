/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../mem/mem.h"

#include "msrp_parser.h"

typedef struct msrp_str_id {
	str sval;
	int ival;
} msrp_str_id_t;


static msrp_str_id_t _msrp_rtypes[] = {
	{ str_init("SEND"),      MSRP_REQ_SEND },
	{ str_init("AUTH"),      MSRP_REQ_AUTH },
	{ str_init("REPORT"),    MSRP_REQ_REPORT },
	{ {0, 0}, 0}
};


static msrp_str_id_t _msrp_htypes[] = {
	{ str_init("From-Path"),            MSRP_HDR_FROM_PATH },
	{ str_init("To-Path"),              MSRP_HDR_TO_PATH },
	{ str_init("Use-Path"),             MSRP_HDR_USE_PATH },
	{ str_init("Message-ID"),           MSRP_HDR_MESSAGE_ID },
	{ str_init("Byte-Range"),           MSRP_HDR_BYTE_RANGE },
	{ str_init("Status"),               MSRP_HDR_STATUS },
	{ str_init("Success-Report"),       MSRP_HDR_SUCCESS_REPORT },
	{ str_init("Content-Type"),         MSRP_HDR_CONTENT_TYPE },
	{ str_init("Authorization"),        MSRP_HDR_AUTH },
	{ str_init("WWW-Authenticate"),     MSRP_HDR_WWWAUTH },
	{ str_init("Authentication-Info"),  MSRP_HDR_AUTHINFO },
	{ str_init("Expires"),              MSRP_HDR_EXPIRES },
	{ {0, 0}, 0}
};


/* */
int msrp_fline_set_rtypeid(msrp_frame_t *mf);
int msrp_hdr_set_type(msrp_hdr_t *hdr);

/**
 *
 */
int msrp_parse_frame(msrp_frame_t *mf)
{
	if(msrp_parse_fline(mf)<0)
	{
		LM_ERR("unable to parse first line\n");
		return -1;
	}
	if(msrp_parse_headers(mf)<0)
	{
		LM_ERR("unable to parse headers\n");
		return -1;
	}
	return 0;
}

/**
 *
 */
int msrp_parse_fline(msrp_frame_t *mf)
{
	char *p;
	char *s;

	mf->fline.buf.s = mf->buf.s;
	s = mf->buf.s;
	p =  q_memchr(mf->fline.buf.s, '\n', mf->buf.len);
	if(p==NULL) {
		LM_ERR("no end of line\n");
		return -1;
	}
	mf->fline.buf.len = p - mf->fline.buf.s + 1;

	if(mf->fline.buf.len<10) {
		LM_ERR("too short for a valid first line [%.*s] (%d)\n",
				mf->fline.buf.len, mf->fline.buf.s, mf->fline.buf.len);
		return -1;
	}
	if(memcmp(mf->fline.buf.s, "MSRP ", 5)!=0) {
		LM_ERR("first line does not start with MSRP [%.*s] (%d)\n",
				mf->fline.buf.len, mf->fline.buf.s, mf->fline.buf.len);
		return -1;
	}
	mf->fline.protocol.s = mf->fline.buf.s;
	mf->fline.protocol.len = 4;
	s += 5;
	p =  q_memchr(s, ' ', mf->fline.buf.s + mf->fline.buf.len - s);
	/* eat whitespaces */
	while (p!=NULL && p==s) {
		s++;
		p =  q_memchr(s, ' ', mf->fline.buf.s + mf->fline.buf.len - s);
	}
	if(p==NULL) {
		LM_ERR("cannot find transaction id in first line [%.*s] (%d)\n",
				mf->fline.buf.len, mf->fline.buf.s, mf->fline.buf.len);
		return -1;
	}
	mf->fline.transaction.s = s;
	mf->fline.transaction.len = p - s;
	s = p+1;
	p =  q_memchr(s, ' ', mf->fline.buf.s + mf->fline.buf.len - s);
	/* eat whitespaces */
	while (p!=NULL && p==s) {
		s++;
		p =  q_memchr(s, ' ', mf->fline.buf.s + mf->fline.buf.len - s);
	}
	if(p==NULL) {
		if(s>=mf->fline.buf.s + mf->fline.buf.len - 2) {
			LM_ERR("cannot method in first line [%.*s] (%d)\n",
					mf->fline.buf.len, mf->fline.buf.s, mf->fline.buf.len);
			return -1;
		}
		mf->fline.rtype.s = s;
		mf->fline.rtype.len = mf->fline.buf.s + mf->fline.buf.len - s;
		trim(&mf->fline.rtype);
		mf->fline.msgtypeid = MSRP_REQUEST;
		goto done;
	}
	mf->fline.rtype.s = s;
	mf->fline.rtype.len = p - s;
	s = p+1;
	mf->fline.rtext.s = s;
	mf->fline.rtext.len = mf->fline.buf.s + mf->fline.buf.len - s;
	trim(&mf->fline.rtext);
	mf->fline.msgtypeid = MSRP_REPLY;

done:
	if(msrp_fline_set_rtypeid(mf)<0)
	{
		LM_ERR("cannot set rtype-id in first line [%.*s] (%d)\n",
			mf->fline.buf.len, mf->fline.buf.s, mf->fline.buf.len);
		return -1;
	}
	LM_DBG("MSRP FLine: [%d] [%.*s] [%.*s] [%.*s] [%d] [%.*s]\n",
			mf->fline.msgtypeid,
			mf->fline.protocol.len, mf->fline.protocol.s,
			mf->fline.transaction.len, mf->fline.transaction.s,
			mf->fline.rtype.len, mf->fline.rtype.s,
			mf->fline.rtypeid,
			mf->fline.rtext.len, mf->fline.rtext.s);
	return 0;
}

/**
 *
 */
int msrp_fline_set_rtypeid(msrp_frame_t *mf)
{
	unsigned int i;
	if(mf->fline.msgtypeid==MSRP_REPLY)
	{
		if(str2int(&mf->fline.rtype, &i)<0)
		{
			LM_ERR("invalid status code [%.*s]\n",
					mf->fline.rtype.len, mf->fline.rtype.s);
			return -1;
		}
		mf->fline.rtypeid = MSRP_REQ_RPLSTART + i;
		return 0;
	}
	if(mf->fline.msgtypeid==MSRP_REQUEST)
	{
		for(i=0; _msrp_rtypes[i].sval.s!=NULL; i++)
		{
			if(mf->fline.rtype.len==_msrp_rtypes[i].sval.len
					&& strncmp(_msrp_rtypes[i].sval.s,
						mf->fline.rtype.s,
						_msrp_rtypes[i].sval.len)==0)
			{
				mf->fline.rtypeid = _msrp_rtypes[i].ival;
				return 0;
			}
		}
		return 0;
	}
	return -1;
}

/**
 *
 */
int msrp_parse_headers(msrp_frame_t *mf)
{
	char *e; /* end of message */
	char *l; /* end of line */
	char *p; /* searched location */
	char *s; /* start for search */
	msrp_hdr_t *hdr;
	msrp_hdr_t *last;
	int fpath = 0; /* From path set */
	int tpath = 0; /* To path set */
	int any = 0; /* Any header set */

	/* already parsed?!? */
	if(mf->headers != NULL)
		return 0;

	last = NULL;
	mf->hbody.s = mf->fline.buf.s + mf->fline.buf.len;
	e = mf->buf.s + mf->buf.len;
	s = mf->hbody.s;
	p = s;
	while(p!=NULL)
	{
		l = q_memchr(s, '\n', e - s);
		if(l==NULL) {
			LM_ERR("broken msrp frame message\n");
			return -1;
		}
		p = q_memchr(s, ':', l - s);
		if(p==NULL)
		{
			/* line without ':' - end of headers */
			if(s[0]=='-')
			{
				mf->endline.len = 7 + mf->fline.transaction.len + 1 + 2;
				/* check if it is end-line (i.e., no msg body) */
				if((l-s+1 == mf->endline.len)
						&& strncmp(s, "-------", 7)==0
						&& strncmp(s+7, mf->fline.transaction.s,
							mf->fline.transaction.len)==0)
				{
					mf->hbody.len = s - mf->hbody.s;
					mf->endline.s = s;
					goto ateoh;
				}
				mf->endline.len = 0;
				LM_ERR("mismatch msrp frame message eoh endline\n");
				return -1;
			} else if (s[0]=='\r' || s[0]=='\n') {
				mf->hbody.len = s - mf->hbody.s;
				mf->mbody.s = l + 1;
				goto ateoh;
			}
			LM_ERR("broken msrp frame message eoh\n");
			return -1;
		}
		/* new header */
		hdr = (msrp_hdr_t*)pkg_malloc(sizeof(msrp_hdr_t));
		if(hdr==NULL)
		{
			LM_ERR("no more pkg\n");
			return -1;
		}
		memset(hdr, 0, sizeof(msrp_hdr_t));
		hdr->buf.s = s;
		hdr->buf.len = l - s + 1;
		hdr->name.s = s;
		hdr->name.len = p - s;
		hdr->body.s = p + 1;
		hdr->body.len = l - p - 1;
		trim(&hdr->body);

		if(last==NULL)
		{
			mf->headers = hdr;
			last = hdr;
		} else {
			last->next = hdr;
			last = hdr;
		}
		msrp_hdr_set_type(hdr);

		/* Checking for well-formed MSRP rfc4975 messages */
		if (hdr->htype == MSRP_HDR_TO_PATH) {
			if (tpath) {
				LM_ERR("broken msrp frame message, Multiple To-Path not allowed.\n");
				return -1;				
			} else if (fpath || any) {
				LM_ERR("broken msrp frame message, To-Path must be the first header.\n");
				return -1;
			} else {
				tpath = 1;
			}
		} else if (hdr->htype == MSRP_HDR_FROM_PATH) {
			if (fpath) {
				LM_ERR("broken msrp frame message, Multiple From-Path not allowed.\n");
				return -1;
			} else if (!tpath || any) {
				LM_ERR("broken msrp frame message, From-Path must be after To-Path.\n");
				return -1;
			} else {
				fpath = 1;
			}
		} else {
			if (!tpath || !fpath) {
				LM_ERR("broken msrp frame message, To-Path and From-Path must be defined before any header.\n");
				return -1;
			} else {
				any = 1;
			}
		}
		
		LM_DBG("MSRP Header: (%p) [%.*s] [%d] [%.*s]\n",
				hdr, hdr->name.len, hdr->name.s, hdr->htype,
				hdr->body.len, hdr->body.s);
		s = l + 1;
	}

	if (!tpath || !fpath) {
		LM_ERR("broken msrp frame message, To-Path and From-Path must be defined.\n");
		return -1;
	}

ateoh:
	if(mf->mbody.s!=NULL)
	{
		/* last header must be Content-Type */

		/* get now body.len and endline */
		mf->endline.len = 7 + mf->fline.transaction.len + 1 + 2;
		mf->endline.s = e - mf->endline.len;
		s = mf->endline.s;
		if(s[-1]!='\n')
		{
			LM_ERR("broken msrp frame message body endline\n");
			return -1;
		}
		if(strncmp(s, "-------", 7)==0
				&& strncmp(s+7, mf->fline.transaction.s,
				mf->fline.transaction.len)==0)
		{
			mf->mbody.len = s - mf->mbody.s;
			LM_DBG("MSRP Body: [%d] [[\n%.*s\n]]\n",
					mf->mbody.len, mf->mbody.len, mf->mbody.s);
			return 0;
		}
		LM_ERR("mismatch msrp frame message body endline\n");
		return -1;

	}
	return 0;
}

/**
 *
 */
int msrp_hdr_set_type(msrp_hdr_t *hdr)
{
	int i;
	if(hdr==NULL)
		return -1;

	for(i=0; _msrp_htypes[i].sval.s!=NULL; i++)
	{
		if(hdr->name.len==_msrp_htypes[i].sval.len
				&& strncmp(_msrp_htypes[i].sval.s,
					hdr->name.s,
					_msrp_htypes[i].sval.len)==0)
		{
			hdr->htype = _msrp_htypes[i].ival;
			return 0;
		}
	}
	return 1;
}

/**
 *
 */
void msrp_destroy_frame(msrp_frame_t *mf)
{
	msrp_hdr_t *hdr;
	msrp_hdr_t *nxt;

	if(mf==NULL || mf->headers==NULL)
		return;

	hdr = mf->headers;
	while(hdr!=NULL)
	{
		nxt = hdr->next;
		if(hdr->parsed.flags&MSRP_DATA_SET)
		{
			if(hdr->parsed.free_fn)
			{
				hdr->parsed.free_fn(hdr->parsed.data);
			}
		}
		pkg_free(hdr);
		hdr = NULL;
		hdr = nxt;
	}

	return;
}

/**
 *
 */
void msrp_free_frame(msrp_frame_t *mf)
{
	msrp_destroy_frame(mf);
	pkg_free(mf);
	return;
}

/**
 *
 */
int msrp_parse_uri(char *start, int len, msrp_uri_t *uri)
{
	char *e;
	char *l;
	char *p;
	char *s;
	str *hook;

	if(start==NULL || uri==NULL || len<8)
	{
		LM_ERR("invalid parameters\n");
		return -1;
	}

	memset(uri, 0, sizeof(msrp_uri_t));
	e = start + len;
	s = start;
	uri->buf.s = s;
	uri->scheme.s = s;

	if(strncasecmp(s, "msrp://", 7)==0) {
		uri->scheme.len = 4;
		uri->scheme_no = MSRP_SCHEME_MSRP;
		s += 7;
	} else if(strncasecmp(s, "msrps://", 8)==0) {
		uri->scheme.len = 5;
		uri->scheme_no = MSRP_SCHEME_MSRPS;
		s += 8;
	} else {
		LM_ERR("invalid scheme in [%.*s]\n", len, start);
		goto error;
	}

	p = q_memchr(s, '@', e - s);
	if(p!=NULL)
	{
		l = q_memchr(s, ';', e - s);
		if(l==NULL || p<l)
		{
			/* user info part */
			uri->userinfo.s = s;
			uri->userinfo.len = p - s;
			uri->user.s = s;
			l = q_memchr(uri->userinfo.s, ';', uri->userinfo.len);
			if(l!=NULL)
			{
				uri->user.len = l - uri->user.s;
			} else {
				l = q_memchr(uri->userinfo.s, ':', uri->userinfo.len);
				if(l!=NULL)
				{
					uri->user.len = l - uri->user.s;
				} else {
					uri->user.len = uri->userinfo.len;
				}
			}
		}
		s = p + 1;
		if(s>=e) goto error;
	}
	hook = &uri->host;
	hook->s = s;
	p = q_memchr(s, ':', e - s);
	if(p!=NULL)
	{
		hook->len = p - hook->s;
		hook = &uri->port;
		s = p+1;
		if(s>=e) goto error;
	}
	hook->s = s;
	p = q_memchr(s, '/', e - s);
	if(p!=NULL)
	{
		hook->len = p - hook->s;
		hook = &uri->session;
		s = p+1;
		if(s>=e) goto error;
	}
	hook->s = s;
	p = q_memchr(s, ';', e - s);
	if(p!=NULL)
	{
		hook->len = p - hook->s;
		hook = &uri->params;
		s = p+1;
		if(s>=e) goto error;
	}
	hook->s = s;
	hook->len = e - hook->s;
	trim(hook);

	if(uri->host.len<=0)
	{
		LM_ERR("bad host part in [%.*s] at [%ld]\n",
				len, start, (long)(s - start));
		goto error;
	}
	if(uri->port.len <= 0)
	{
		uri->port_no = 0;
	} else {
		str2sint(&uri->port, &uri->port_no);
		if(uri->port_no<1 || uri->port_no > ((1<<16)-1))
		{
			LM_ERR("bad port part in [%.*s] at [%ld]\n",
					len, start, (long)(s - start));
			goto error;
		}
	}
	if(uri->params.len > 0)
	{
		uri->proto.s = uri->params.s;
		if(uri->params.len > 3 && strncasecmp(uri->params.s, "tcp", 3)==0) {
			uri->proto.len = 3;
			uri->proto_no = MSRP_PROTO_TCP;
		} else if (uri->params.len > 2 && strncasecmp(uri->params.s, "ws", 2)==0) {
			uri->proto.len = 2;
			uri->proto_no = MSRP_PROTO_WS;
		} else {
			p = q_memchr(uri->params.s, ';', uri->params.len);
			if(p!=NULL) {
				uri->proto.len = p - uri->proto.s;
				uri->params.len = uri->params.s + uri->params.len - p - 1;
				uri->params.s = p + 1;
			} else {
				uri->proto.len = uri->params.len;
				uri->params.s = NULL;
				uri->params.len = 0;
			}
		}
	}

	LM_DBG("MSRP URI: [%.*s] [%.*s] [%.*s] [%.*s] [%.*s] [%.*s] [%.*s]\n",
			uri->scheme.len, uri->scheme.s,
			uri->user.len, (uri->user.s)?uri->user.s:"",
			uri->host.len, uri->host.s,
			uri->port.len, (uri->port.s)?uri->port.s:"",
			uri->session.len, (uri->session.s)?uri->session.s:"",
			uri->proto.len, (uri->proto.s)?uri->proto.s:"",
			uri->params.len, (uri->params.s)?uri->params.s:"");
	return 0;

error:
	LM_ERR("parsing error in [%.*s] at [%ld]\n",
			len, start, (long int)(s - start));
	memset(uri, 0, sizeof(msrp_uri_t));
	return -1;
}

/**
 *
 */
msrp_hdr_t *msrp_get_hdr_by_id(msrp_frame_t *mf, int hdrid)
{
	msrp_hdr_t *hdr;
	for(hdr=mf->headers; hdr; hdr=hdr->next)
		if(hdr->htype==hdrid)
			return hdr;
	return NULL;
}


/**
 *
 */
int msrp_explode_str(str **arr, str *in, str *del)
{
	str *larr;
	int i;
	int j;
	int k;
	int n;

	/* Find number of strings */
	n = 0;
	for(i=0; i<in->len; i++)
	{
		for(j=0; j<del->len; j++)
		{
			if(in->s[i]==del->s[j])
			{
				n++;
				break;
			}
		}
	}
	n++;

	larr = pkg_malloc(n * sizeof(str));
	if(larr==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(larr, 0, n * sizeof(str));

	k = 0;
	if(n==1)
	{
		larr[k].s = in->s;
		larr[k].len = in->len;
		*arr = larr;
		return n;
	}

	larr[k].s = in->s;
	for(i=0; i<in->len; i++)
	{
		for(j=0; j<del->len; j++)
		{
			if(in->s[i]==del->s[j])
			{
				larr[k].len = in->s + i - larr[k].s;
				k++;
				if(k<n)
					larr[k].s = in->s + i + 1;
				break;
			}
		}
	}
	larr[k].len = in->s + i - larr[k].s;

	*arr = larr;

	return n;
}

/**
 *
 */
int msrp_explode_strz(str **arr, str *in, char *del)
{
	str s;

	s.s = del;
	s.len = strlen(s.s);
	return msrp_explode_str(arr, in, &s);
}

void msrp_str_array_destroy(void *data)
{
	str_array_t *arr;
	if(data==NULL)
		return;
	arr = (str_array_t*)data;
	if(arr->list!=NULL)
			pkg_free(arr->list);
	pkg_free(arr);
}

/**
 *
 */
int msrp_parse_hdr_uri_list(msrp_hdr_t *hdr)
{
	str_array_t *arr;
	str s;

	arr = pkg_malloc(sizeof(str_array_t));
	if(arr==NULL)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	memset(arr, 0, sizeof(str_array_t));

	s = hdr->body;
	trim(&s);
	arr->size = msrp_explode_strz(&arr->list, &s, " ");
	hdr->parsed.flags |= MSRP_DATA_SET;
	hdr->parsed.free_fn = msrp_str_array_destroy;
	hdr->parsed.data = arr;
	return 0;
}

/**
 *
 */
int msrp_parse_hdr_from_path(msrp_frame_t *mf)
{
	msrp_hdr_t *hdr;

	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
	if(hdr==NULL)
		return -1;
	if(hdr->parsed.flags&MSRP_DATA_SET)
		return 0;
	return msrp_parse_hdr_uri_list(hdr);
}

/**
 *
 */
int msrp_parse_hdr_to_path(msrp_frame_t *mf)
{
	msrp_hdr_t *hdr;

	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
	if(hdr==NULL)
		return -1;
	if(hdr->parsed.flags&MSRP_DATA_SET)
		return 0;
	return msrp_parse_hdr_uri_list(hdr);
}

/**
 *
 */
int msrp_parse_hdr_expires(msrp_frame_t *mf)
{
	msrp_hdr_t *hdr;
	str hbody;
	int expires;

	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_EXPIRES);
	if(hdr==NULL)
		return -1;
	if(hdr->parsed.flags&MSRP_DATA_SET)
		return 0;
	hbody = hdr->body;
	trim(&hbody);
	if(str2sint(&hbody, &expires)<0) {
		LM_ERR("invalid expires value\n");
		return -1;
	}
	hdr->parsed.flags |= MSRP_DATA_SET;
	hdr->parsed.free_fn = NULL;
	hdr->parsed.data = (void*)(long)expires;

	return 0;
}

/**
 *
 */
int msrp_frame_get_first_from_path(msrp_frame_t *mf, str *sres)
{
	str s = {0};
	msrp_hdr_t *hdr;
	str_array_t *sar;

	if(msrp_parse_hdr_from_path(mf)<0)
		return -1;
	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
	if(hdr==NULL)
		return -1;
	sar = (str_array_t*)hdr->parsed.data;
	s = sar->list[sar->size-1];
	trim(&s);
	*sres = s;
	return 0;
}

/**
 *
 */
int msrp_frame_get_expires(msrp_frame_t *mf, int *expires)
{
	msrp_hdr_t *hdr;

	if(msrp_parse_hdr_expires(mf)<0)
		return -1;
	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_AUTH);
	if(hdr==NULL)
		return -1;
	*expires = (int)(long)hdr->parsed.data;
	return 0;
}

/**
 *
 */
int msrp_frame_get_sessionid(msrp_frame_t *mf, str *sres)
{
	str s = {0};
	msrp_hdr_t *hdr;
	str_array_t *sar;
	msrp_uri_t uri;

	if(msrp_parse_hdr_to_path(mf)<0)
		return -1;
	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
	if(hdr==NULL)
		return -1;
	sar = (str_array_t*)hdr->parsed.data;
	s = sar->list[0];
	trim(&s);
	if(msrp_parse_uri(s.s, s.len, &uri)<0 || uri.session.len<=0)
		return -1;
	s = uri.session;
	trim(&s);
	*sres = s;

	return 0;
}

