/*
 * SDP parser helpers
 *
 * Copyright (C) 2008-2009 SOMA Networks, INC.
 * Copyright (C) 2010 VoIP Embedded, Inc
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *  1. Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE FREEBSD PROJECT ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE FREEBSD PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdlib.h>
#include "../../ut.h"
#include "../msg_parser.h"
#include "../parser_f.h"
#include "../parse_hname2.h"
#include "sdp.h"


static struct {
	const char *s;
	int len;
	int is_rtp;
} sup_ptypes[] = {
	{.s = "rtp/avp",   .len = 7, .is_rtp = 1},
	{.s = "udptl",     .len = 5, .is_rtp = 0},
	{.s = "rtp/avpf",  .len = 8, .is_rtp = 1},
	{.s = "rtp/savp",  .len = 8, .is_rtp = 1},
	{.s = "rtp/savpf", .len = 9, .is_rtp = 1},
	{.s = "udp",       .len = 3, .is_rtp = 0},
	{.s = "udp/bfcp",  .len = 8, .is_rtp = 0},
	{.s = NULL,        .len = 0, .is_rtp = 0}
};


#define READ(val) \
	(*(val + 0) + (*(val + 1) << 8) + (*(val + 2) << 16) + (*(val + 3) << 24))
#define advance(_ptr,_n,_str,_error) \
	do{\
		if ((_ptr)+(_n)>(_str).s+(_str).len)\
			goto _error;\
		(_ptr) = (_ptr) + (_n);\
	}while(0);
#define one_of_16( _x , _t ) \
	(_x==_t[0]||_x==_t[15]||_x==_t[8]||_x==_t[2]||_x==_t[3]||_x==_t[4]\
	||_x==_t[5]||_x==_t[6]||_x==_t[7]||_x==_t[1]||_x==_t[9]||_x==_t[10]\
	||_x==_t[11]||_x==_t[12]||_x==_t[13]||_x==_t[14])
#define one_of_8( _x , _t ) \
	(_x==_t[0]||_x==_t[7]||_x==_t[1]||_x==_t[2]||_x==_t[3]||_x==_t[4]\
	||_x==_t[5]||_x==_t[6])

int get_mixed_part_delimiter(str* body, str *mp_delimiter)
{
	static unsigned int boun[16] = {
	        0x6e756f62,0x4e756f62,0x6e556f62,0x4e556f62,
		0x6e754f62,0x4e754f62,0x6e554f62,0x4e554f62,
		0x6e756f42,0x4e756f42,0x6e556f42,0x4e556f42,
		0x6e754f42,0x4e754f42,0x6e554f42,0x4e554f42};
	static unsigned int dary[16] = {
	        0x79726164,0x59726164,0x79526164,0x59526164,
		0x79724164,0x59724164,0x79524164,0x59524164,
		0x79726144,0x59726144,0x79526144,0x59526144,
		0x79724144,0x59724144,0x79524144,0x59524144};
	str str_type;
	unsigned int  x;
	char          *p;


	/* LM_DBG("<%.*s>\n",body->len,body->s); */
	p = str_type.s = body->s;
	str_type.len = body->len;
	while (*p!=';' && p<(body->s+body->len))
		advance(p,1,str_type,error);
	p++;
	str_type.s = p;
	str_type.len = body->len - (p - body->s);
	/* LM_DBG("string to parse: <%.*s>\n",str_type.len,str_type.s); */
	/* skip spaces and tabs if any */
	while (*p==' ' || *p=='\t')
		advance(p,1,str_type,error);
	advance(p,4,str_type,error);
	x = READ(p-4);
	if (!one_of_16(x,boun))
		goto other;
	advance(p,4,str_type,error);
	x = READ(p-4);
	if (!one_of_16(x,dary))
		goto other;
	
	/* skip spaces and tabs if any */
	while (*p==' ' || *p=='\t')
		advance(p,1,str_type,error);
	if (*p!='=') {
		LM_ERR("parse error: no = found after boundary field\n");
		goto error;
	}
	advance(p,1,str_type,error);
	while ((*p==' ' || *p=='\t') && p+1<str_type.s+str_type.len)
		advance(p,1,str_type,error);
	mp_delimiter->len = str_type.len - (int)(p-str_type.s);
	mp_delimiter->s = p;
	/* check if the boundary value is enclosed in quotes */
	if(*p=='"' || *p=='\'') {
		if(mp_delimiter->s[mp_delimiter->len-1]==*p) {
			mp_delimiter->s = p+1;
			mp_delimiter->len -= 2;
			if(mp_delimiter->len<=0) {
				LM_ERR("invalid boundary field value\n");
				goto error;
			}
		} else {
			LM_ERR("missing closing quote in boundary field value\n");
			goto error;
		}
	}
	return 1;

error:  
	return -1;
other:  
	LM_DBG("'boundary' parsing error\n");
	return -1;
}



/**
 * rfc4566:
 * a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
 */
int extract_rtpmap(str *body,
	str *rtpmap_payload, str *rtpmap_encoding, str *rtpmap_clockrate, str *rtpmap_parmas)
{
	char *cp, *cp1;
	int len;

	if (strncasecmp(body->s, "a=rtpmap:", 9) !=0) {
		/*LM_DBG("We are not pointing to an a=rtpmap: attribute =>`%.*s'\n", body->len, body->s); */
		return -1;
	}

	cp1 = body->s;

	rtpmap_payload->s = cp1 + 9; /* skip `a=rtpmap:' */
	rtpmap_payload->len = eat_line(rtpmap_payload->s, body->s + body->len -
	          rtpmap_payload->s) - rtpmap_payload->s;
	trim_len(rtpmap_payload->len, rtpmap_payload->s, *rtpmap_payload);
	len = rtpmap_payload->len;

	/* */
	cp = eat_token_end(rtpmap_payload->s, rtpmap_payload->s + rtpmap_payload->len);
	rtpmap_payload->len = cp - rtpmap_payload->s;
	if (rtpmap_payload->len <= 0 || cp == rtpmap_payload->s) {
		LM_ERR("no encoding in `a=rtpmap'\n");
		return -1;
	}
	len -= rtpmap_payload->len;
	rtpmap_encoding->s = cp;
	cp = eat_space_end(rtpmap_encoding->s, rtpmap_encoding->s + len);
	len -= cp - rtpmap_encoding->s;
	if (len <= 0 || cp == rtpmap_encoding->s) {
		LM_ERR("no encoding in `a=rtpmap:'\n");
		return -1;
	}

	rtpmap_encoding->s = cp;
	cp1 = (char*)ser_memmem(cp, "/", len, 1);
	if(cp1==NULL) {
		LM_ERR("invalid encoding in `a=rtpmap' at [%.*s]\n", len, cp);
		return -1;
	}
	len -= cp1 - cp;
	rtpmap_encoding->len = cp1 - cp;

	cp = cp1+1;  /* skip '/' */
	len--;
	rtpmap_clockrate->s = cp;
	cp1 = (char*)ser_memmem(cp, "/", len, 1);
	if(cp1==NULL) {
		/* no encoding parameters */
		rtpmap_clockrate->len = len;
		rtpmap_parmas->s = NULL;
		rtpmap_parmas->len = 0;
		return 0;
	}
	rtpmap_clockrate->len = cp1 - cp;
	len -= cp1 - cp;
	rtpmap_parmas->s = cp1 + 1;  /* skip '/' */
	rtpmap_parmas->len = len - 1;
	return 0;
}

int extract_fmtp( str *body, str *fmtp_payload, str *fmtp_string )
{
	char *cp, *cp1;
	int len;

	if (strncasecmp(body->s, "a=fmtp:", 7) !=0) {
		/*LM_DBG("We are not pointing to an a=fmtp: attribute =>`%.*s'\n", body->len, body->s); */
		return -1;
	}

	cp1 = body->s;

	fmtp_payload->s = cp1 + 7; /* skip `a=fmtp:' */
	fmtp_payload->len = eat_line(fmtp_payload->s, body->s + body->len -
		fmtp_payload->s) - fmtp_payload->s;
	trim_len(fmtp_payload->len, fmtp_payload->s, *fmtp_payload);
	len = fmtp_payload->len;

	/* */
	cp = eat_token_end(fmtp_payload->s, fmtp_payload->s + fmtp_payload->len);
	fmtp_payload->len = cp - fmtp_payload->s;
	if (fmtp_payload->len <= 0 || cp == fmtp_payload->s) {
		LM_ERR("no encoding in `a=fmtp:'\n");
		return -1;
	}
	len -= fmtp_payload->len;
	fmtp_string->s = cp;
	cp = eat_space_end(fmtp_string->s, fmtp_string->s + len);
	len -= cp - fmtp_string->s;
	if (len <= 0 || cp == fmtp_string->s) {
		LM_ERR("no encoding in `a=fmtp:'\n");
		return -1;
	}

	fmtp_string->s = cp;

	fmtp_string->len = eat_line(fmtp_string->s, body->s + body->len -
		fmtp_string->s) - fmtp_string->s;
	trim_len(fmtp_string->len, fmtp_string->s, *fmtp_string);

	return 0;
}


/**
 * Allocate a new ice attribute
 */
static inline sdp_ice_attr_t *add_sdp_ice(sdp_stream_cell_t* _stream)
{
	sdp_ice_attr_t *ice_attr;
	int len;

	len = sizeof(sdp_ice_attr_t);
	ice_attr = (sdp_ice_attr_t *)pkg_malloc(len);
	if (ice_attr == NULL) {
	    LM_ERR("No memory left\n");
	    return NULL;
	}
	memset( ice_attr, 0, len);

	/* Insert the new ice attribute */
	ice_attr->next = _stream->ice_attr;
	_stream->ice_attr = ice_attr;
	_stream->ice_attrs_num++;

	return ice_attr;
}


int extract_candidate(str *body, sdp_stream_cell_t *stream)
{
    char *space, *start;
    int len, fl;
    sdp_ice_attr_t *ice_attr;

    if ((body->len < 12) || (strncasecmp(body->s, "a=candidate:", 12) != 0)) {
	/*LM_DBG("We are not pointing to an a=candidate: attribute =>`%.*s'\n", body->len, body->s); */
	return -1;
    }
    
    start = body->s + 12;
    len = body->len - 12;

    space = memchr(start, 32, len);
    if ((space == NULL) || (space - start + 3 > len) || !isdigit(*(space + 1)))  {
	LM_ERR("no component in `a=candidate'\n");
	return -1;
    }

    fl = space - start;
    
    start = space + 1;
    len = len - (space - start + 1);
    space = memchr(start, 32, len);
    if (space == NULL) {
	LM_ERR("no component in `a=candidate'\n");
	return -1;
    }

    ice_attr = add_sdp_ice(stream);
    if (ice_attr == NULL) {
	LM_ERR("failed to add ice attribute\n");
	return -1;
    }

    /* currently only foundation and component-id are parsed */
    /* if needed, parse more */
    ice_attr->foundation.s = body->s + 12;
    ice_attr->foundation.len = fl;
    ice_attr->component_id = strtol(start, (char **)NULL, 10);

    return 0;
}


/* generic method for attribute extraction
 * field must has format "a=attrname:" */
int extract_field(str *body, str *value, str field)
{
	if (strncmp(body->s, field.s, field.len < body->len ? field.len : body->len) !=0) {
		/*LM_DBG("We are not pointing to an %.* attribute =>`%.*s'\n", field.len, field.s, body->len, body->s); */
		return -1;
	}

	value->s = body->s + field.len; /* skip `a=attrname:' */
	value->len = eat_line(value->s, body->s + body->len -
	          value->s) - value->s;
	trim_len(value->len, value->s, *value);

	return 0;
}


int extract_ptime(str *body, str *ptime)
{
	static const str field = str_init("a=ptime:");
	return extract_field(body, ptime, field);
}

int extract_accept_types(str *body, str *accept_types)
{
	static const str field = str_init("a=accept-types:");
	return extract_field(body, accept_types, field);
}

int extract_accept_wrapped_types(str *body, str *accept_wrapped_types)
{
	static const str field = str_init("a=accept-wrapped-types:");
	return extract_field(body, accept_wrapped_types, field);
}

int extract_max_size(str *body, str *max_size)
{
	static const str field = str_init("a=max-size:");
	return extract_field(body, max_size, field);
}

int extract_path(str *body, str *path)
{
	static const str field = str_init("a=path:");
	return extract_field(body, path, field);
}

int extract_rtcp(str *body, str *rtcp)
{
	static const str field = str_init("a=rtcp:");
	return extract_field(body, rtcp, field);
}

int extract_sendrecv_mode(str *body, str *sendrecv_mode, int *is_on_hold)
{
	char *cp1;

	cp1 = body->s;
	if ( !( (strncasecmp(cp1, "a=sendrecv", 10) == 0) ||
		(strncasecmp(cp1, "a=recvonly", 10) == 0))) {
		if ((strncasecmp(cp1, "a=inactive", 10) == 0) ||
		    (strncasecmp(cp1, "a=sendonly", 10) == 0) ) {
			*is_on_hold = RFC3264_HOLD;
		} else {
			return -1;
		}
	}

	sendrecv_mode->s = body->s + 2; /* skip `a=' */
	sendrecv_mode->len = 8; /* we know the length and therefore we don't need to overkill */
	/*
	sendrecv_mode->len = eat_line(sendrecv_mode->s, body->s + body->len -
		sendrecv_mode->s) - sendrecv_mode->s;
	trim_len(sendrecv_mode->len, sendrecv_mode->s, *sendrecv_mode);
	*/

	return 0;
}

int extract_bwidth(str *body, str *bwtype, str *bwwitdth)
{
	char *cp, *cp1;
	int len;

	cp1 = NULL;
	for (cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = (char*)ser_memmem(cp, "b=", len, 2);
		if (cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if (cp1 == NULL)
		return -1;

	bwtype->s = cp1 + 2;
	bwtype->len = eat_line(bwtype->s, body->s + body->len - bwtype->s) - bwtype->s;
	trim_len(bwtype->len, bwtype->s, *bwtype);

	cp = bwtype->s;
	len = bwtype->len;
	cp1 = (char*)ser_memmem(cp, ":", len, 1);
	len -= cp1 - cp;
	if (len <= 0) {
		LM_ERR("invalid encoding in `b=%.*s'\n", bwtype->len, bwtype->s);
		return -1;
	}
	bwtype->len = cp1 - cp;

	/* skip ':' */
	bwwitdth->s = cp1 + 1;
	bwwitdth->len = len - 1;
	
	return 0;
}

int extract_mediaip(str *body, str *mediaip, int *pf, char *line)
{
	char *cp, *cp1;
	int len;

	cp1 = NULL;
	for (cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = (char*)ser_memmem(cp, line, len, 2);
		if (cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if (cp1 == NULL)
		return -1;

	mediaip->s = cp1 + 2;
	mediaip->len = eat_line(mediaip->s, body->s + body->len - mediaip->s) - mediaip->s;
	trim_len(mediaip->len, mediaip->s, *mediaip);
	if (mediaip->len == 0) {
		LM_ERR("no [%s] line in SDP\n",line);
		return -1;
	}

	/* search reverse for IP[4|6] in c=/o= line */
	cp = (char*)ser_memrmem(mediaip->s, " IP", mediaip->len, 3);
	if (cp == NULL) {
		LM_ERR("no `IP[4|6]' in `%s' field\n",line);
		return -1;
	}
	/* safety checks:
	 * - for lenght, at least 6: ' IP[4|6] x...'
	 * - white space after
	 */
	if(cp + 6 > mediaip->s + mediaip->len && cp[4]!=' ') {
		LM_ERR("invalid content for `%s' line\n",line);
		return -1;
	}
	switch(cp[3]) {
		case '4':
			*pf = AF_INET;
		break;
		case '6':
			*pf = AF_INET6;
		break;
		default:
			LM_ERR("invalid addrtype IPx for `%s' line\n",line);
			return -1;
	}
	cp += 5;

	/* next token is the IP address */
	cp = eat_space_end(cp, mediaip->s + mediaip->len);
	len = eat_token_end(cp, mediaip->s + mediaip->len) - cp;
	mediaip->s = cp;
	mediaip->len = len;

	if (mediaip->len == 0) {
		LM_ERR("no `IP[4|6]' address in `%s' field\n",line);
		return -1;
	}

	LM_DBG("located IP address [%.*s] in `%s' field\n",
			mediaip->len, mediaip->s, line);
	return 1;
}

int extract_media_attr(str *body, str *mediamedia, str *mediaport, str *mediatransport, str *mediapayload, int *is_rtp)
{
	char *cp, *cp1;
	int len, i;

	cp1 = NULL;
	for (cp = body->s; (len = body->s + body->len - cp) > 0;) {
		cp1 = (char*)ser_memmem(cp, "m=", len, 2);
		if (cp1 == NULL || cp1[-1] == '\n' || cp1[-1] == '\r')
			break;
		cp = cp1 + 2;
	}
	if (cp1 == NULL) {
		LM_ERR("no `m=' in SDP\n");
		return -1;
	}
	mediaport->s = cp1 + 2; /* skip `m=' */
	mediaport->len = eat_line(mediaport->s, body->s + body->len -
	  mediaport->s) - mediaport->s;
	trim_len(mediaport->len, mediaport->s, *mediaport);

	mediapayload->len = mediaport->len;
	mediamedia->s = mediaport->s;


	/* Skip media supertype and spaces after it */
	cp = eat_token_end(mediaport->s, mediaport->s + mediaport->len);
	mediaport->len -= cp - mediaport->s;
	mediamedia->len = mediapayload->len - mediaport->len;
	if (mediaport->len <= 0 || cp == mediaport->s) {
		LM_ERR("no port in `m='\n");
		return -1;
	}
	mediaport->s = cp;

	cp = eat_space_end(mediaport->s, mediaport->s + mediaport->len);
	mediaport->len -= cp - mediaport->s;
	if (mediaport->len <= 0 || cp == mediaport->s) {
		LM_ERR("no port in `m='\n");
		return -1;
	}

	/* Extract port */
	mediaport->s = cp;
	cp = eat_token_end(mediaport->s, mediaport->s + mediaport->len);
	mediatransport->len = mediaport->len - (cp - mediaport->s);
	if (mediatransport->len <= 0 || cp == mediaport->s) {
		LM_ERR("no port in `m='\n");
		return -1;
	}
	mediatransport->s = cp;
	mediaport->len = cp - mediaport->s;

	/* Skip spaces after port */
	cp = eat_space_end(mediatransport->s, mediatransport->s + mediatransport->len);
	mediatransport->len -= cp - mediatransport->s;
	if (mediatransport->len <= 0 || cp == mediatransport->s) {
		LM_ERR("no protocol type in `m='\n");
		return -1;
	}
	/* Extract protocol type */
	mediatransport->s = cp;
	cp = eat_token_end(mediatransport->s, mediatransport->s + mediatransport->len);
	if (cp == mediatransport->s) {
		LM_ERR("no protocol type in `m='\n");
		return -1;
	}
	mediatransport->len = cp - mediatransport->s;

	mediapayload->s = mediatransport->s + mediatransport->len;
	mediapayload->len -= mediapayload->s - mediamedia->s;
	cp = eat_space_end(mediapayload->s, mediapayload->s + mediapayload->len);
	mediapayload->len -= cp - mediapayload->s;
	mediapayload->s = cp;

	for (i = 0; sup_ptypes[i].s != NULL; i++)
		if (mediatransport->len == sup_ptypes[i].len &&
		    strncasecmp(mediatransport->s, sup_ptypes[i].s, mediatransport->len) == 0) {
			*is_rtp = sup_ptypes[i].is_rtp;
			return 0;
		}
	/* Unproxyable protocol type. Generally it isn't error. */
	return 0;
}


/*
 * Auxiliary for some functions.
 * Returns pointer to first character of found line, or NULL if no such line.
 */

char *find_sdp_line(char* p, char* plimit, char linechar)
{
	static char linehead[3] = "x=";
	char *cp, *cp1;
	linehead[0] = linechar;
	/* Iterate through body */
	cp = p;
	for (;;) {
		if (cp >= plimit)
			return NULL;
		cp1 = ser_memmem(cp, linehead, plimit-cp, 2);
		if (cp1 == NULL)
			return NULL;
		/*
		 * As it is body, we assume it has previous line and we can
		 * lookup previous character.
		 */
		if (cp1[-1] == '\n' || cp1[-1] == '\r')
			return cp1;
		/*
		 * Having such data, but not at line beginning.
		 * Skip them and reiterate. ser_memmem() will find next
		 * occurence.
		 */
		if (plimit - cp1 < 2)
			return NULL;
		cp = cp1 + 2;
	}
}


/* This function assumes p points to a line of requested type. */
char * find_next_sdp_line(char* p, char* plimit, char linechar, char* defptr)
{
	char *t;
	if (p >= plimit || plimit - p < 3)
		return defptr;
	t = find_sdp_line(p + 2, plimit, linechar);
	return t ? t : defptr;
}


/* Find first SDP line starting with linechar. Return defptr if not found */
char * find_first_sdp_line(char* pstart, char* plimit, char linechar,
		char* defptr)
{
	char *t;
	if (pstart >= plimit || plimit - pstart < 3)
		return defptr;
	t = find_sdp_line(pstart, plimit, linechar);
	return t ? t : defptr;
}


/* returns pointer to next header line, and fill hdr_f ;
 * if at end of header returns pointer to the last crlf  (always buf)*/
char* get_sdp_hdr_field(char* buf, char* end, struct hdr_field* hdr)
{

	char* tmp;
	char *match;

	if ((*buf)=='\n' || (*buf)=='\r'){
		/* double crlf or lflf or crcr */
		hdr->type=HDR_EOH_T;
		return buf;
	}

	tmp=parse_hname2(buf, end, hdr);
	if (hdr->type==HDR_ERROR_T){
		LM_ERR("bad header\n");
		goto error;
	}

	/* eliminate leading whitespace */
	tmp=eat_lws_end(tmp, end); 
	if (tmp>=end) {
		LM_ERR("hf empty\n");
		goto error;
	}

	/* just skip over it */
	hdr->body.s=tmp;
	/* find end of header */
	/* find lf */
	do{
		match=memchr(tmp, '\n', end-tmp);
		if (match){
			match++;
		}else {
			LM_ERR("bad body for <%s>(%d)\n", hdr->name.s, hdr->type);
			tmp=end;
			goto error;
		}
		tmp=match;
	}while( match<end &&( (*match==' ')||(*match=='\t') ) );
	tmp=match;
	hdr->body.len=match-hdr->body.s;

	/* jku: if \r covered by current length, shrink it */
	trim_r( hdr->body );
	hdr->len=tmp-hdr->name.s;
	return tmp;
error:
	LM_DBG("error exit\n");
	hdr->type=HDR_ERROR_T;
	hdr->len=tmp-hdr->name.s;
	return tmp;
}



char *find_sdp_line_delimiter(char* p, char* plimit, str delimiter)
{
  static char delimiterhead[3] = "--";
  char *cp, *cp1;
  /* Iterate through body */
  cp = p;
  for (;;) {
    if (cp >= plimit)
      return NULL;
    for(;;) {
      cp1 = ser_memmem(cp, delimiterhead, plimit-cp, 2);
      if (cp1 == NULL)
	return NULL;
      /* We matched '--',
       * now let's match the boundary delimiter */
      if (strncmp(cp1+2, delimiter.s, delimiter.len) == 0)
	break;
      else
	cp = cp1 + 2 + delimiter.len;
      if (cp >= plimit)
	return NULL;
    }
    if (cp1[-1] == '\n' || cp1[-1] == '\r')
      return cp1;
    if (plimit - cp1 < 2 + delimiter.len)
      return NULL;
    cp = cp1 + 2 + delimiter.len;
  }
}


/*
 * This function assumes p points to a delimiter type line.
 */
char *find_next_sdp_line_delimiter(char* p, char* plimit, str delimiter, char* defptr)
{
  char *t;
  if (p >= plimit || plimit - p < 3)
    return defptr;
  t = find_sdp_line_delimiter(p + 2, plimit, delimiter);
  return t ? t : defptr;
}

