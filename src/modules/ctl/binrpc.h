/*
 * Copyright (C) 2006 iptelorg GmbH
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
 */

/* binrpc is  supposed to be a minimalist binary rpc implementation */



/* packet header:
 * (big endian where it applies)
 *      4b      4b        4b     2b   2b       <var>          <var>
 *  | MAGIC  | VERS  || FLAGS  | LL | CL || total_len ... || cookie ... |
 *  total_len = payload len (doesn't include the packet header)
 *  LL = total length len -1 (number of bytes on which total len is
 *        represented)
 *  CL = cookie length -1 (number of bytes on which the cookie is represented)
 *  E.g.: LL= 0 => total_len is represented on 1 byte (LL+1)
 *        CL= 3 => cookie is represneted on 4 bytes (CL+1)
 */
/* record format:
 *  1b   3b     4b
 * |S | size |  type  || <optional value len> ... || <optional value> ... ||
 *
 * if S==0, size is the size (in bytes) of the value (if size==0 => null value)
 * if S==1, optional_value_len is present, and size is it's size
 *    (if size==0 => and type==array or struct => marks end, else
 *     error, reserved)
 *  Examples:
 *     int (type=0) 0x1234     -> 0x20 0x12 0x34           (optimal)
 *                                0x90 0x02 0x12 0x34      (suboptimal)
 *                                0xA0 0x00 0x02 0x12 0x34 (even worse)
 *                   0x07      -> 0x10 0x07                (optimal)
 *                   0x00      -> 0x00                     (optimal)
 *                                0x10 0x00
 *
 *     str (type=1) - strings are 0 terminated (an extra 0 is added to them
 *                    to make the format easier to parse when asciiz strings
 *                    are required); the length includes the terminating 0
 *                  "abcdef"   -> 0x71 "abcdef" 0x00
 *                  "abcdefhij"-> 0x91 0x0A "abcdefhij" 0x00
 *                  ""         -> 0x11 0x00     (0 len str)
 *     65535 bytes
 *     (using str for it)      -> 0xB1 0x01 0x00 0x00 array 0x00
 *
 *     bytes (type=6) -like str but not 0 terminated
 *                  "abcdef"   -> 0x66 "abcdef"
 *     65535 bytes   *         -> 0xA6 0xff 0xff bytes
 *
 *     arrays (array type=4)
 *       arrays are implemented as anonymous value lists:
 *         array_start value1, value2, ..., array_end
 *                               (start) (1st int)  (2nd elem)  (end array)
 *      ints   [ 0x01 0x02 ]   -> 0x04   0x10 0x01  0x10 0x02        0x84
 *      combo  [ 0x07 "abc"]   -> 0x04   0x10 0x07  0x41 "abc" 0x00  0x84
 *
 *      structs (struct type=3)
 *       structs are implemented as avp list:
 *           struct_start, avp1, avp2 .., struct_end
 *        an avp is a named value pair:  name, value. 
 *          - name behaves like a normal string, but has a diff. type (5)
 *          - avps are legal only inside structs.
 *        avp example:           name part (str)   val part (int here) 
 *         "test", int 0x0b   -> 0x55 "test" 0x00   0x10 0x0b
 *
 *      struct example:
 *                              (start)  (avps)                           (end)
 *      struct{                  0x03        (name )        (val)
 *           intval: int 0x3  ->       0x75 "intval" 0x00 0x10 0x3
 *           s:      str "abc"-        0x25 "s" 0x00      0x41 "abc" 0x00
 *      }                                                                  0x83
 *
 *      Limitations: for now avps cannot have array values =>
 *                   structs cannot contain arrays.
 */


#ifndef _binrpc_h
#define _binrpc_h


#include "../../str.h"
#include <string.h>

#define BINRPC_MAGIC 0xA
#define BINRPC_VERS  1

/* sizes & offsets */
#define BINRPC_FIXED_HDR_SIZE	2
#define BINRPC_TLEN_OFFSET		BINRPC_FIXED_HDR_SIZE
#define BINRPC_MIN_HDR_SIZE	(BINRPC_FIXED_HDR_SIZE+2)
#define BINRPC_MAX_HDR_SIZE	(BINRPC_FIXED_HDR_SIZE+4+4)
#define BINRPC_MIN_RECORD_SIZE	1
/* min pkt size: min header + min. len (1) + min. cookie (1)*/
#define BINRPC_MIN_PKT_SIZE	BINRPC_MIN_HDR_SIZE

/* message types */
#define BINRPC_REQ   0
#define BINRPC_REPL  1
#define BINRPC_FAULT 3

/* values types */
#define BINRPC_T_INT	0
#define BINRPC_T_STR	1 /* 0 term, for easier parsing */
#define BINRPC_T_DOUBLE	2
#define BINRPC_T_STRUCT	3
#define BINRPC_T_ARRAY	4
#define BINRPC_T_AVP	5  /* allowed only in structs */
#define BINRPC_T_BYTES	6 /* like STR, but not 0 term */

#define BINRPC_T_ALL	0xf /* wildcard type, will match any record type
							   in the packet (not allowed inside the pkt)*/

/* errors */
#define E_BINRPC_INVAL		-1  /* invalid function call parameters */
#define E_BINRPC_OVERFLOW	-2  /* buffer overflow */
#define E_BINRPC_BADPKT		-3  /* something went really bad, the packet is
								   corrupted*/
#define E_BINRPC_MORE_DATA	-4 /* parsing error: more bytes are needed,
								  just repeat the failed op, when you have
								  more bytes available */
#define E_BINRPC_EOP		-5	/* end of packet reached */
#define E_BINRPC_NOTINIT	-6  /* parse ctx not initialized */
#define E_BINRPC_TYPE		-7  /* unkown type for record, or requested
								   type doesn't match record type */
#define E_BINRPC_RECORD		-8  /* bad record (unexpected, bad struct a.s.o)*/
#define E_BINRPC_BUG		-9  /* internal error, bug */
#define E_BINRPC_LAST		-10 /* used to count the errors, keep always
								   last */

/* flags */
#define BINRPC_F_INIT	1

struct binrpc_pkt{  /* binrpc body */
	unsigned char* body;
	unsigned char* end;
	unsigned char* crt; /*private */
};


struct binrpc_parse_ctx{
	/* header */
	unsigned int tlen; /* total len */
	unsigned int cookie;
	int type; /* request, reply, error */
	
	/* parsing info */
	unsigned int flags;   /* parsing flags */
	unsigned int offset; /* current offset (inside payload) */
	unsigned int in_struct;
	unsigned int in_array;
};



struct binrpc_val{
	str name; /* used only in structs */
	int type;
	union{
		str strval;
		double fval;
		int intval;
		int end;
	}u;
};



/*  helper functions  */

/* return int size: minimum number of bytes needed to represent it
 * if i=0 returns 0 */
inline static int binrpc_get_int_len(int i)
{
	int size;
	for (size=4; size && ((i & (0xff<<24))==0); i<<=8, size--);
	return size;
}



/* adds a start or end tag (valid only for STRUCT or ARRAY for now */
inline static int binrpc_add_tag(struct binrpc_pkt* pkt, int type, int end)
{
	if (pkt->crt>=pkt->end) return E_BINRPC_OVERFLOW;
	*pkt->crt=(end<<7)|type;
	pkt->crt++;
	return 0;
}



/*  writes a minimal int, returns the new offset and sets
 * len to the number of bytes written (<=4)
 * to check for oveflow use:  returned_value-p != *len
 * (Note: if *len==0 using the test above succeeds even if p>=end)
 */
inline static unsigned char* binrpc_write_int(	unsigned char* p,
												unsigned char* end,
												int i, int *len)
{
	int size;

	for (size=4; size && ((i & (0xff<<24))==0); i<<=8, size--);
	*len=size;
	for(; (p<end) && (size); p++, size--){
		*p=(unsigned char)(i>>24);
		i<<=8;
	}
	return p;
}



/* API functions */

/* initialize a binrpc_pkt structure, for packet creation
 * params: pkt         - binrpc body structure that will be initialized 
 *         buf, b_len  -  destination buffer/len
 * returns -1 on error, 0 on success
 *
 * Example usage:
 *  binrpc_init_pkt(pkt, body, BODY_SIZE);
 *  binrpc_addint(pkt, 1);
 *  binrpc_addstr(pkt, "test", sizeof("test")-1);
 *  ...
 *  bytes=binrpc_build_hdr(pkt, BINRPC_REQ, 0x123, hdr_buf, HDR_BUF_LEN);
 *  writev(sock, {{ hdr, bytes}, {pkt->body, pkt->crt-pkt->body}} , 2)*/
inline static int binrpc_init_pkt(struct binrpc_pkt *pkt,
								  unsigned char* buf, int b_len)
{
	if (b_len<BINRPC_MIN_RECORD_SIZE)
		return E_BINRPC_OVERFLOW;
	pkt->body=buf;
	pkt->end=buf+b_len;
	pkt->crt=pkt->body;
	return 0;
};



/* used to update internal contents if the original buffer 
 * (from binrpc_init_pkt) was realloc'ed (and has grown) */
inline static int binrpc_pkt_update_buf(struct binrpc_pkt *pkt,
										unsigned char* new_buf,
										int new_len)
{
	if ((int)(pkt->crt-pkt->body)>new_len){
		return E_BINRPC_OVERFLOW;
	}
	pkt->crt=new_buf+(pkt->crt-pkt->body);
	pkt->body=new_buf;
	pkt->end=new_buf+new_len;
	return 0;
}



/* builds a binrpc header for the binrpc pkt. body pkt and writes it in buf
 * params:  
 *          type     - binrpc packet type (request, reply, fault)
 *          body_len - body len
 *          cookie   - binrpc cookie value
 *          buf,len  - destination buffer & len
 * returns -1 on error, number of bytes written on success */
inline static int binrpc_build_hdr(	int type, int body_len,
									unsigned int cookie,
									unsigned char* buf, int b_len) 
{
	unsigned char* p;
	int len_len;
	int c_len;
	
	len_len=binrpc_get_int_len(body_len);
	c_len=binrpc_get_int_len(cookie);
	if (len_len==0) len_len=1; /* we can't have 0 len */
	if (c_len==0) c_len=1;  /* we can't have 0 len */
	/* size check: 2 bytes header + len_len + cookie len*/
	if (b_len<(BINRPC_FIXED_HDR_SIZE+len_len+c_len)){
		goto error_len;
	}
	p=buf;
	*p=(BINRPC_MAGIC << 4) | BINRPC_VERS;
	p++;
	*p=(type<<4)|((len_len-1)<<2)|(c_len-1);
	p++;
	for(;len_len>0; len_len--,p++){
		*p=(unsigned char)(body_len>>((len_len-1)*8));
	}
	for(;c_len>0; c_len--,p++){
		*p=(unsigned char)(cookie>>((c_len-1)*8));
	}
	return (int)(p-buf);
error_len:
	return E_BINRPC_OVERFLOW;
}



#define binrpc_pkt_len(pkt)		((int)((pkt)->crt-(pkt)->body))



/* changes the length of a header (enough space must be availale) */
inline static int binrpc_hdr_change_len(unsigned char* hdr, int hdr_len,
										int new_len)
{
	int len_len;
	
	binrpc_write_int(&hdr[BINRPC_TLEN_OFFSET], hdr+hdr_len, new_len, &len_len);
	return 0;
}



/* int format:     size BINRPC_T_INT <val>  */
inline static int binrpc_add_int_type(struct binrpc_pkt* pkt, int i, int type)
{
	
	unsigned char* p;
	int size;
	
	p=binrpc_write_int(pkt->crt+1, pkt->end, i, &size);
	if ((pkt->crt>=pkt->end) || ((int)(p-pkt->crt-1)!=size))
		goto error_len;
	*(pkt->crt)=(size<<4) | type;
	pkt->crt=p;
	return 0;
error_len:
	return E_BINRPC_OVERFLOW;
}



/* double format:  FIXME: for now a hack: fixed point represented in
 *  an int (=> max 3 decimals, < MAX_INT/1000) */
#define binrpc_add_double_type(pkt, f, type)\
	binrpc_add_int_type((pkt), (int)((f)*1000), (type))



/* skip bytes bytes (leaves an empty space, for possible future use)
 * WARNING: use with care, low level function
 */
inline static int binrpc_add_skip(struct binrpc_pkt* pkt, int bytes)
{
	
	if ((pkt->crt+bytes)>=pkt->end)
		return E_BINRPC_OVERFLOW;
	pkt->crt+=bytes;
	return 0;
}



/*
 * adds only the string mark and len, you'll have to memcpy the contents
 * manually later (and also use binrpc_add_skip(pkt, l) or increase
 *  pkt->crt directly if you want to continue adding to this pkt).
 *  Usefull for optimizing str writing (e.g. writev(iovec))
 *  WARNING: use with care, low level function, binrpc_addstr or 
 *           binrpc_add_str_type are probably what you want.
 *  WARNING1: BINRPC_T_STR and BINRPC_T_AVP must be  0 term, the len passed to
 *            this function, must include the \0 in this case.
 */
inline static int binrpc_add_str_mark(struct binrpc_pkt* pkt, int type,
										int l)
{
	int size;
	unsigned char* p;
	
	if (pkt->crt>=pkt->end) goto error_len;
	if (l<8){
		size=l;
		p=pkt->crt+1;
	}else{ /* we need a separate len */
		p=binrpc_write_int(pkt->crt+1, pkt->end, l, &size);
		if (((int)(p-pkt->crt-1)!=size))
			goto error_len;
		size|=8; /* mark it as having external len  */
	}
	*(pkt->crt)=(size)<<4|type;
	pkt->crt=p;
	return 0;
error_len:
	return E_BINRPC_OVERFLOW;
}



inline static int binrpc_add_str_type(struct binrpc_pkt* pkt, char* s, int len,
										int type)
{
	int size;
	int l;
	int zero_term; /* whether or not to add an extra 0 at the end */
	unsigned char* p;
	
	zero_term=((type==BINRPC_T_STR)||(type==BINRPC_T_AVP));
	l=len+zero_term;
	if (l<8){
		size=l;
		p=pkt->crt+1;
	}else{ /* we need a separate len */
		p=binrpc_write_int(pkt->crt+1, pkt->end, l, &size);
		/* if ((int)(p-pkt->crt)<(size+1)) goto error_len;  - not needed,
		 *  caught by the next check */
		size|=8; /* mark it as having external len  */
	}
	if ((p+l)>pkt->end) goto error_len;
	*(pkt->crt)=(size)<<4|type;
	memcpy(p, s, len);
	if (zero_term) p[len]=0;
	pkt->crt=p+l;
	return 0;
error_len:
	return E_BINRPC_OVERFLOW;
}



/* adds an avp (name, value) pair, usefull to add structure members */
inline static int binrpc_addavp(struct binrpc_pkt* pkt, struct binrpc_val* avp)
{
	int ret;
	unsigned char* bak;
	
	bak=pkt->crt;
	ret=binrpc_add_str_type(pkt, avp->name.s, avp->name.len, BINRPC_T_AVP);
	if (ret<0) return ret;
	switch (avp->type){
		case BINRPC_T_INT:
			ret=binrpc_add_int_type(pkt, avp->u.intval, avp->type);
			break;
		case BINRPC_T_STR:
		case BINRPC_T_BYTES:
			ret=binrpc_add_str_type(pkt, avp->u.strval.s, 
										avp->u.strval.len,
										avp->type);
			break;
		case BINRPC_T_STRUCT:
		case BINRPC_T_ARRAY:
			ret=binrpc_add_tag(pkt, avp->type, 0);
			break;
		case BINRPC_T_DOUBLE: 
			ret=binrpc_add_double_type(pkt, avp->u.fval, avp->type);
			break;
		default:
			ret=E_BINRPC_BUG;
	}
	if (ret<0)
		pkt->crt=bak; /* roll back */
	return ret;
}



#define binrpc_addint(pkt, i)	binrpc_add_int_type((pkt), (i), BINRPC_T_INT) 

#define binrpc_adddouble(pkt, f)	\
	binrpc_add_double_type((pkt), (f), BINRPC_T_DOUBLE)

#define binrpc_addstr(pkt, s, len)	\
	binrpc_add_str_type((pkt), (s), (len), BINRPC_T_STR) 

#define binrpc_addbytes(pkt, s, len)	\
	binrpc_add_str_type((pkt), (s), (len), BINRPC_T_BYTES) 

/* struct type format:
 *  start :         0000 | BINRPC_T_STRUCT 
 *  end:            1000 | BINRPC_T_STRUCT
 */
#define  binrpc_start_struct(pkt) binrpc_add_tag((pkt), BINRPC_T_STRUCT, 0)

#define  binrpc_end_struct(pkt) binrpc_add_tag((pkt), BINRPC_T_STRUCT, 1)

#define  binrpc_start_array(pkt) binrpc_add_tag((pkt), BINRPC_T_ARRAY, 0)

#define  binrpc_end_array(pkt) binrpc_add_tag((pkt), BINRPC_T_ARRAY, 1)


static inline int binrpc_addfault(	struct binrpc_pkt* pkt,
									int code,
									char* s, int len)
{
	int ret;
	unsigned char* bak;
	
	bak=pkt->crt;
	if ((ret=binrpc_addint(pkt, code))<0)
		return ret;
	ret=binrpc_addstr(pkt, s, len);
	if (ret<0)
		pkt->crt=bak; /* roll back */
	return ret;
}

/* parsing incoming messages */


static inline unsigned char* binrpc_read_int(	int* i,
												int len,
												unsigned char* s, 
												unsigned char* end,
												int *err
												)
{
	unsigned char* start;
	
	start=s;
	*i=0;
	*err=0;
	for(;len>0; len--, s++){
		if (s>=end){
			*err=E_BINRPC_MORE_DATA;
			return start;
		}
		*i<<=8;
		*i|=*s;
	};
	return s;
}



/* initialize parsing context, it tries to read the whole message header,
 * if there is not enough data, sets *err to E_BINRPC_MORE_DATA. In this
 *  case just redo the call when more data is available (len is bigger)
 * on success sets *err to 0 and returns the current position in  buf
 * (=> you can discard the content between buf & the returned value).
 * On error buf is returned back, and *err set.
 */
static inline unsigned char* binrpc_parse_init(	struct binrpc_parse_ctx* ctx,
												unsigned char* buf,
												int len,
												int *err
												)
{
	int len_len, c_len;
	unsigned char *p;

	*err=0;
	ctx->tlen=0;	/* init to 0 */
	ctx->cookie=0;	/* init to 0 */
	if (len<BINRPC_MIN_PKT_SIZE){
		*err=E_BINRPC_MORE_DATA;
		goto error;
	}
	if (buf[0]!=((BINRPC_MAGIC<<4)|BINRPC_VERS)){
		*err=E_BINRPC_BADPKT;
		goto error;
	}
	ctx->type=buf[1]>>4;
	/* type check */
	switch(ctx->type){
		case BINRPC_REQ:
		case BINRPC_REPL:
		case BINRPC_FAULT:
			break;
		default:
			*err=E_BINRPC_BADPKT;
			goto error;
	}
	len_len=((buf[1]>>2) & 3) + 1;
	c_len=(buf[1]&3) + 1;
	if ((BINRPC_TLEN_OFFSET+len_len+c_len)>len){
		*err=E_BINRPC_MORE_DATA;
		goto error;
	}
	p=binrpc_read_int((int*)&ctx->tlen, len_len, &buf[BINRPC_TLEN_OFFSET],
						&buf[len], err);
	/* empty packets (replies) are allowed
	   if (ctx->tlen==0){
		*err=E_BINRPC_BADPKT;
		goto error;
	} */
	p=binrpc_read_int((int*)&ctx->cookie, c_len, p, &buf[len], err);
	ctx->offset=0;
	ctx->flags|=BINRPC_F_INIT;
	return p;
error:
	return buf;
}



/* returns bytes needed (till the end of the packet)
 * on error (non. init ctx) returns < 0 
 */
inline static int binrpc_bytes_needed(struct binrpc_parse_ctx *ctx)
{
	if (ctx->flags & BINRPC_F_INIT)
		return ctx->tlen-ctx->offset;
	return E_BINRPC_NOTINIT;
}



/* prefill v with the requested type, if type==BINRPC_T_ALL it 
 * will be replaced by the actual record type 
 * known problems: no support for arrays inside STRUCT
 * param smode: allow simple vals inside struct (needed for 
 * not-strict-formatted rpc responses)
 * returns position after the record and *err==0 if succesfull
 *         original position and *err<0 if not */
inline static unsigned char* binrpc_read_record(struct binrpc_parse_ctx* ctx,
												unsigned char* buf,
												unsigned char* end,
												struct binrpc_val* v,
												int smode,
												int* err
												)
{
	int type;
	int len;
	int end_tag;
	int tmp;
	unsigned char* p;
	int i;
	
	p=buf;
	end_tag=0;
	*err=0;
	if (!(ctx->flags & BINRPC_F_INIT)){
		*err=E_BINRPC_NOTINIT;
		goto error;
	}
	if (ctx->offset>=ctx->tlen){
		*err=E_BINRPC_EOP;
		goto error;
	}
	if (p>=end){
		*err=E_BINRPC_MORE_DATA;
		goto error;
	}
	/* read type_len */
	type=*p & 0xf;
	len=*p>>4;
	p++;
	if (len & 8){
		end_tag=1; /* possible end mark for array or structs */
		/* we have to read len bytes and use them as the new len */
		p=binrpc_read_int(&len, len&7, p, end, err);
		if (*err<0)
			goto error;
	}
	if ((p+len)>end){
		*err=E_BINRPC_MORE_DATA;
		goto error;
	}
	if ((v->type!=type) && (v->type !=BINRPC_T_ALL)){
		goto error_type;
	}
	v->type=type;
	switch(type){
		case BINRPC_T_STRUCT:
			if (ctx->in_struct){
				if (end_tag){
					ctx->in_struct--;
					v->u.end=1;
				}else{
					if(smode==0) {
						goto error_record;
					} else {
						v->u.end=0;
						ctx->in_struct++;
					}
				}
			} else {
				if (end_tag)
					goto error_record;
				v->u.end=0;
				ctx->in_struct++;
			}
			break;
		case BINRPC_T_AVP:
			/* name | value */
			if (ctx->in_struct){
				v->name.s=(char*)p;
				v->name.len=(len-1); /* don't include 0 term */
				p+=len;
				if (p>=end){
					*err=E_BINRPC_MORE_DATA;
					goto error;
				}
				/* avp value type */
				type=*p & 0xf;
				if ((type!=BINRPC_T_AVP) && (type!=BINRPC_T_ARRAY)){
					tmp=ctx->in_struct;
					ctx->in_struct=0; /* hack to parse a normal record */
					v->type=type; /* hack */
					p=binrpc_read_record(ctx, p, end, v, smode, err);
					if (err<0){
						ctx->in_struct=tmp;
						goto error;
					}else{
						ctx->in_struct+=tmp;
						/* the offset is already updated => skip */
						goto no_offs_update;
					}
				}else{
					goto  error_record;
				}
			} else {
				goto error_type;
			}
			break;
		case BINRPC_T_INT:
			if (ctx->in_struct && smode==0) goto error_record;
			p=binrpc_read_int(&v->u.intval, len, p, end, err);
			break;
		case BINRPC_T_STR:
			if (ctx->in_struct && smode==0) goto error_record;
			v->u.strval.s=(char*)p;
			v->u.strval.len=(len-1); /* don't include terminating 0 */
			p+=len;
			break;
		case BINRPC_T_BYTES:
			if (ctx->in_struct && smode==0) goto error_record;
			v->u.strval.s=(char*)p;
			v->u.strval.len=len;
			p+=len;
		case BINRPC_T_ARRAY:
			if (ctx->in_struct && smode==0) goto error_record;
			if (end_tag){
				if (ctx->in_array>0){
					ctx->in_array--;
					v->u.end=1;
				}else{
					goto error_record;
				}
			}else{
				ctx->in_array++;
				v->u.end=0;
			}
			break;
		case BINRPC_T_DOUBLE: /* FIXME: hack: represented as fixed point
		                                      inside an int */
			if (ctx->in_struct && smode==0) goto error_record;
			p=binrpc_read_int(&i, len, p, end, err);
			v->u.fval=((double)i)/1000;
			break;
		default:
			if (ctx->in_struct){
				goto error_record;
			} else {
				goto error_type;
			}
	}
	ctx->offset+=(int)(p-buf);
no_offs_update:
	return p;
error_type:
	*err=E_BINRPC_TYPE;
	return buf;
error_record:
	*err=E_BINRPC_RECORD;
error:
	return buf;
}



/* reads/skips an entire struct
 * the struct start/end are saved in v->u.strval.s, v->u.strval.len 
 * return:  - new buffer position  and set *err to 0 if successfull
 *          - original buffer and *err<0 on error */
inline static unsigned char* binrpc_read_struct(struct binrpc_parse_ctx* ctx,
												unsigned char* buf,
												unsigned char* end,
												struct binrpc_val* v,
												int* err
												)
{

	int type;
	int len;
	int end_tag;
	unsigned char* p;
	int in_struct;
	
	*err=0;
	p=buf;
	end_tag=0;
	if (!(ctx->flags & BINRPC_F_INIT)){
		*err=E_BINRPC_NOTINIT;
		goto error;
	}
	if (ctx->offset>=ctx->tlen){
		*err=E_BINRPC_EOP;
		goto error;
	}
	if (p>=end){
		*err=E_BINRPC_MORE_DATA;
		goto error;
	}
	/* read type_len */
	type=*p & 0xf;
	len=*p>>4;
	p++;
	if (len & 8){
		end_tag=1; /* possible end mark for array or structs */
		/* we have to read len bytes and use them as the new len */
		p=binrpc_read_int(&len, len&7, p, end, err);
		if (*err<0)
			goto error;
	}
	if ((p+len)>=end){
		*err=E_BINRPC_MORE_DATA;
		goto error;
	}
	if (type!=BINRPC_T_STRUCT){
		goto error_type;
	}
	if (end_tag){
		goto error_record;
	}
	p+=len; /* len should be 0 for a struct tag */
	in_struct=1;
	v->type=type;
	v->u.strval.s=(char*)p; /* it will conain the inside of the struc */
	while(in_struct){
		/* read name */
		type=*p & 0xf;
		len=*p>>4;
		p++;
		if (len & 8){
			end_tag=1; /* possible end mark for array or structs */
			/* we have to read len bytes and use them as the new len */
			p=binrpc_read_int(&len, len&7, p, end, err);
			if (*err<0)
				goto error;
		}
		if ((type==BINRPC_T_STRUCT) && end_tag){
			in_struct--;
			if (in_struct<0)
				goto error_record;
			continue;
		}else if (type!=BINRPC_T_AVP){
			goto error_record;
		}
		/* skip over it */
		p+=len;
		if (p>=end){
			*err=E_BINRPC_MORE_DATA;
			goto error;
		}
		/* read value */
		type=*p & 0xf;
		len=*p>>4;
		p++;
		if (len & 8){
			end_tag=1; /* possible end mark for array or structs */
			/* we have to read len bytes and use them as the new len */
			p=binrpc_read_int(&len, len&7, p, end, err);
			if (*err<0)
				goto error;
		}
		if (type==BINRPC_T_STRUCT){
			if (end_tag)
				goto error_record;
			in_struct++;
		};
		p+=len;
		if (p>=end){
			*err=E_BINRPC_MORE_DATA;
			goto error;
		}
	}
	/* don't include the end tag */;
	v->u.strval.len=(int)(p-(unsigned char*)v->u.strval.s)-1;
	return p;
	
error_type:
	*err=E_BINRPC_RECORD;
	return buf;
error_record:
	*err=E_BINRPC_TYPE;
error:
	return buf;
}



/* error code to string */
const char* binrpc_error(int err);
#endif
