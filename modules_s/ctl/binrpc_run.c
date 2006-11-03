/*
 * $Id$
 *
 * Copyright (C) 2006 iptelorg GmbH
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* History:
 * --------
 *  2006-02-08  created by andrei
 */


#include "binrpc.h"
#include "../../dprint.h"
#include "../../rpc.h"
#include "../../sr_module.h"
#include "../../mem/mem.h"
#include "../../clist.h"
#include "io_listener.h"

#include <stdio.h>  /* vsnprintf */
#include <stdarg.h>

#define BINRPC_MAX_BODY	4096  /* maximum body for send */
#define STRUCT_MAX_BODY	1024
#define MAX_MSG_CHUNKS	96

struct rpc_struct_head{
	struct rpc_struct_l* next;
	struct rpc_struct_l* prev;
};


struct rpc_struct_l{
	struct rpc_struct_l* next;
	struct rpc_struct_l* prev;
	struct binrpc_pkt pkt;
	struct rpc_struct_head substructs; /* head */
	int offset; /* byte offset in parent's pkt */
};

struct binrpc_send_ctx{
	struct binrpc_pkt pkt; /* body */
	struct rpc_struct_head structs; /* list head */
};

struct binrpc_recv_ctx{
	struct binrpc_parse_ctx ctx;
	unsigned char*  s; /* current position in buffer */
	unsigned char* end;
	int record_no;
	int in_struct;
};

struct binrpc_ctx{
	struct binrpc_recv_ctx in;
	struct binrpc_send_ctx out;
	void* send_h; /* send handle */
	char* method;
	int replied;
};


struct iovec_array{
	struct iovec* v;
	int idx;
	int len;
};

/* send */
static void rpc_fault(struct binrpc_ctx* ctx, int code, char* fmt, ...);
static int rpc_send(struct binrpc_ctx* ctx);
static int rpc_add(struct binrpc_ctx* ctx, char* fmt, ...);
static int rpc_scan(struct binrpc_ctx* ctx, char* fmt, ...);
static int rpc_printf(struct binrpc_ctx* ctx, char* fmt, ...);
static int rpc_struct_add(struct rpc_struct_l* s, char* fmt, ...);
static int rpc_struct_scan(struct rpc_struct_l* s, char* fmt, ...);
/* struct scan */
static int rpc_struct_printf(struct rpc_struct_l *s, char* name,
								char* fmt, ...);


static rpc_t binrpc_callbacks={
	(rpc_fault_f)			rpc_fault,
	(rpc_send_f)			rpc_send,
	(rpc_add_f)				rpc_add,
	(rpc_scan_f)			rpc_scan,
	(rpc_printf_f)			rpc_printf,
	(rpc_struct_add_f)		rpc_struct_add,
	(rpc_struct_scan_f)		rpc_struct_scan,
	(rpc_struct_printf_f)	rpc_struct_printf
};



static struct rpc_struct_l* new_rpc_struct()
{
	struct rpc_struct_l* rs;
	
	/* alloc everything in one chunk */
	rs=pkg_malloc(sizeof(struct rpc_struct_l)+STRUCT_MAX_BODY);
	if (rs==0)
		goto error;
	memset(rs, 0, sizeof(struct rpc_struct_l));
	clist_init(&rs->substructs, next, prev);
	if (binrpc_init_pkt(&rs->pkt,
				(unsigned char*)rs+sizeof(struct rpc_struct_l),
				STRUCT_MAX_BODY)<0){
		pkg_free(rs);
		goto error;
	}
	return rs;
error:
	return 0;
}



#if 0 /* not used yet */
/* doubles the size */
static struct rpc_struct_l* grow_rpc_struct(struct rpc_struct_l *rs)
{
	
	struct rpc_struct_l* new_rs;
	int csize; /* body */
	
	csize=binrpc_pkt_len(&rs->pkt);
	csize*=2;
	new_rs=pkg_realloc(rs, sizeof(struct rpc_struct_l)+csize);
	if (new_rs){
		binrpc_pkt_update_buf(&rs->pkt, 
							(unsigned char*)new_rs+sizeof(struct rpc_struct_l),
							csize);
	}
	return new_rs;
}
#endif



/* appends buf to an already init. binrpc_pkt */
inline static int append_pkt_body(struct binrpc_pkt* p, unsigned char* buf,
							int len)
{
	
	if ((int)(p->end-p->crt)<len){
		goto error;
#if 0
		size=2*(int)(p->end-p->body);
		offset=binrpc_pkt_len(p);
		for(;(size-offset)<len; size*=2); /* find new size */
		new_b=pkg_realloc(p->body, size);
		if (new_b==0)
			goto error;
		binrpc_pkt_update_buf(p, new_b, size);
#endif
	}
	memcpy(p->crt, buf, len);
	p->crt+=len;
	return 0;
error:
	return -1; /* buff. overflow */
}



inline static int append_iovec(struct iovec_array* a, unsigned char* buf,
								int len)
{
	
	if (a->idx >= a->len)
		goto error;
	a->v[a->idx].iov_base=buf;
	a->v[a->idx].iov_len=len;
	a->idx++;
	return 0;
error:
	return -1; /* overflow */
}



static int body_get_len(struct binrpc_pkt* body,
							struct rpc_struct_head* sl_head)
{
	struct rpc_struct_l* l;
	int len;
	
	len=binrpc_pkt_len(body);
	clist_foreach(sl_head, l, next){
		len+=body_get_len(&l->pkt, &l->substructs);
	}
	return len;
}



static int body_fill_iovec(struct iovec_array* v_a,
							struct binrpc_pkt* body, 
							struct rpc_struct_head* sl_head)
{
	int offs;
	struct rpc_struct_l* l;
	int ret;
	
	offs=0;
	clist_foreach(sl_head, l, next){
		if ((ret=append_iovec(v_a, body->body+offs, l->offset-offs))<0)
			goto error;
		offs=l->offset;
		if ((ret=body_fill_iovec(v_a, &l->pkt, &l->substructs))<0)
			goto error;
	};
	/* copy the rest */
	ret=append_iovec(v_a, body->body+offs, binrpc_pkt_len(body)-offs);
error:
	return ret;
}



/* expects an initialized new_b */
static int build_structs(struct binrpc_pkt *new_b, struct binrpc_pkt* body, 
							struct rpc_struct_head* sl_head)
{
	int offs;
	struct rpc_struct_l* l;
	int ret;
	
	offs=0;
	clist_foreach(sl_head, l, next){
		if ((ret=append_pkt_body(new_b, body->body+offs, l->offset-offs))<0)
			goto error;
		offs=l->offset;
		if ((ret=build_structs(new_b, &l->pkt, &l->substructs))<0)
			goto error;
	};
	/* copy the rest */
	ret=append_pkt_body(new_b, body->body+offs, binrpc_pkt_len(body)-offs);
error:
	return ret;
}



static void free_structs(struct rpc_struct_head* sl_head)
{
	struct rpc_struct_l* l;
	struct rpc_struct_l* tmp;
	
	clist_foreach_safe(sl_head, l, tmp, next){
		free_structs(&l->substructs);
		memset(l, 0, sizeof(struct rpc_struct_l)); /* debugging */
		pkg_free(l);
	};
}



inline static int init_binrpc_ctx(	struct binrpc_ctx* ctx,
									unsigned char* recv_buf,
									int recv_buf_len,
									void* send_handle
								)
{
	int err;
	unsigned char* send_buf;
	int send_buf_len;
	
	memset(ctx, 0, sizeof(struct binrpc_ctx));
	clist_init(&ctx->out.structs, next, prev);
	ctx->send_h=send_handle;
	ctx->in.end=recv_buf+recv_buf_len;
	ctx->in.s=binrpc_parse_init(&ctx->in.ctx, recv_buf, recv_buf_len, &err);
	if (err<0) goto end;
	if ((ctx->in.ctx.tlen+(int)(ctx->in.s-recv_buf))>recv_buf_len){
		err=E_BINRPC_MORE_DATA;
		goto end;
	}
	
	/* alloc temporary body buffer */
	send_buf_len=BINRPC_MAX_BODY;
	send_buf=pkg_malloc(send_buf_len);
	if (send_buf==0){
		err=E_BINRPC_LAST;
		goto end;
	}
	/* we'll keep only the body */
	err=binrpc_init_pkt(&ctx->out.pkt, send_buf, send_buf_len);
end:
	return err;
}



inline void destroy_binrpc_ctx(struct binrpc_ctx* ctx)
{
	free_structs(&ctx->out.structs);
	if (ctx->out.pkt.body){
		pkg_free(ctx->out.pkt.body);
		ctx->out.pkt.body=0;
	}
}



#define MAX_FAULT_LEN 256
#define FAULT_START_BUF (3 /* maxint*/+2/*max str header*/)
static void rpc_fault(struct binrpc_ctx* ctx, int code, char* fmt, ...)
{
	char buf[MAX_FAULT_LEN];
	static unsigned char fault_start[FAULT_START_BUF];
	static unsigned char hdr[BINRPC_MAX_HDR_SIZE];
	struct iovec v[3];
	struct binrpc_pkt body;
	int b_len;
	va_list ap;
	int len;
	int hdr_len;
	int err;
	
	if (ctx->replied){
		LOG(L_ERR, "ERROR: binrpc: rpc_send: rpc method %s tried to reply"
					" more then once\n", ctx->method?ctx->method:"");
		return;
	}
	err=0;
	va_start(ap, fmt);
	len=vsnprintf(buf, MAX_FAULT_LEN, fmt, ap); /* ignore trunc. errors */
	if ((len<0) || (len > MAX_FAULT_LEN))
		len=MAX_FAULT_LEN-1;
	va_end(ap);
	
	len++; /* vnsprintf doesn't include the terminating 0 */
	err=binrpc_init_pkt(&body, fault_start, FAULT_START_BUF);
	if (err<0){
		LOG(L_ERR, "ERROR: binrpc_init_pkt error\n");
		goto error;
	}
	/* adding a fault "manually" to avoid extra memcpys */
	err=binrpc_addint(&body, code);
	if (err<0){
		LOG(L_ERR, "ERROR: rpc_fault: addint error\n");
		goto error;
	}
	err=binrpc_add_str_mark(&body, BINRPC_T_STR, len);
	if (err<0){
		LOG(L_ERR, "ERROR: rpc_fault: add_str_mark error\n");
		goto error;
	}
	/*
	err=binrpc_addfault(&body, code, buf, len); 
	if (err<0){
		LOG(L_ERR, "ERROR: binrpc_addfault error\n");
		goto error;
	}*/
	b_len=binrpc_pkt_len(&body);
	err=hdr_len=binrpc_build_hdr(BINRPC_FAULT, b_len+len,
								ctx->in.ctx.cookie, hdr, BINRPC_MAX_HDR_SIZE);
	if (err<0){
		LOG(L_ERR, "ERROR: binrpc_build_hdr error\n");
		goto error;
	}
	v[0].iov_base=hdr;
	v[0].iov_len=hdr_len;
	v[1].iov_base=body.body;
	v[1].iov_len=b_len;
	v[2].iov_base=buf;
	v[2].iov_len=len;
	if ((err=sock_send_v(ctx->send_h, v, 3))<0){
		if (err==-2){
			LOG(L_ERR, "ERROR: binrpc_fault: send failed: "
					"datagram too big\n");
			return;
		}
		LOG(L_ERR, "ERROR: binrpc_fault: send failed\n");
		return;
	}
	ctx->replied=1;
	return;
error:
	LOG(L_ERR, "ERROR: binrpc_fault: binrpc_* failed with: %s (%d)\n",
			binrpc_error(err), err);
}



/* build the reply from the current body */
static int rpc_send(struct binrpc_ctx* ctx)
{
	int b_len;
	int hdr_len;
	struct iovec v[MAX_MSG_CHUNKS];
	struct iovec_array a;
	static unsigned char hdr[BINRPC_MAX_HDR_SIZE];
	int err;
	
	err=0;
	a.v=v;
	a.idx=1;
	a.len=MAX_MSG_CHUNKS;
	
	if (ctx->replied){
		LOG(L_ERR, "ERROR: binrpc: rpc_send: rpc method %s tried to reply"
					" more then once\n", ctx->method?ctx->method:"");
		goto error;
	}
	b_len=body_get_len(&ctx->out.pkt, &ctx->out.structs);
	err=hdr_len=binrpc_build_hdr( BINRPC_REPL, b_len, ctx->in.ctx.cookie,
									hdr, BINRPC_MAX_HDR_SIZE);
	if (err<0){
		LOG(L_ERR, "ERROR: binrpc: rpc_fault: binrpc_* failed with:"
					" %s (%d)\n", binrpc_error(err), err);
		goto error;
	}
	v[0].iov_base=hdr;
	v[0].iov_len=hdr_len;
	/* fill the rest of the iovecs */
	err=body_fill_iovec(&a, &ctx->out.pkt, &ctx->out.structs);
	if (err<0){
		LOG(L_ERR, "ERROR: binrprc: rpc_send: too many message chunks\n");
		goto error;
	}
	if ((err=sock_send_v(ctx->send_h, v, a.idx))<0){
		if (err==-2){
			LOG(L_ERR, "ERROR: binrpc: rpc_send: send failed: "
					"datagram too big\n");
			goto error;
		}
		LOG(L_ERR, "ERROR: binrprc: rpc_send: send failed\n");
		goto error;
	}
	ctx->replied=1;
	return 0;
error:
	return -1;
}



/* params: buf, size     - buffer containing the packet
 *         bytes_needed  - int pointer, filled with how many bytes are still
 *                         needed (after bytes_needed new bytes received this
 *                         function will be called again 
 *         reply,        - buffer where the reply will be written
 *         reply_len     - intially filled with the reply buffer len,
 *                         after the call will contain how much of that 
 *                         buffer was really used
 * returns: number of bytes processed on success/partial success
 *          -1 on error
 */
int process_rpc_req(unsigned char* buf, int size, int* bytes_needed,
					void* sh, void** saved_state)
{
	int err;
	struct binrpc_val val;
	rpc_export_t* rpc_e;
	struct binrpc_ctx f_ctx;
	struct binrpc_parse_ctx* ctx;
	
	if (size<BINRPC_MIN_PKT_SIZE){
		*bytes_needed=BINRPC_MIN_PKT_SIZE-size;
		return 0; /* more data , nothing processed */
	}
	err=init_binrpc_ctx(&f_ctx, buf, size, sh);
	ctx=&f_ctx.in.ctx;
	if (err<0){
		if (err==E_BINRPC_MORE_DATA){
			if (f_ctx.in.ctx.tlen){
				*bytes_needed=ctx->tlen+(int)(f_ctx.in.s-buf)-size;
			}else{
				*bytes_needed=1; /* we don't really know how much */
			}
			goto more_data;
		}else if( err==E_BINRPC_LAST){
			LOG(L_ERR, "ERROR: init_binrpc_ctx: out of memory\n");
			rpc_fault(&f_ctx, 500, "internal server error: out of mem.");
			goto error;
		}
		rpc_fault(&f_ctx, 400, "bad request: %s", binrpc_error(err));
		goto error;
	}
	err=E_BINRPC_BADPKT;
	if (ctx->type!=BINRPC_REQ){
		rpc_fault(&f_ctx, 400, "bad request: %s", binrpc_error(err));
		goto error;
	}
	/* now we have the entire packet */
	
	/* get rpc method */
	val.type=BINRPC_T_STR;
	f_ctx.in.s=binrpc_read_record(ctx, f_ctx.in.s, f_ctx.in.end, &val, &err);
	if (err<0){
		LOG(L_CRIT, "ERROR: bad rpc request method, binrpc error: %s (%d)\n",
				binrpc_error(err), err);
		rpc_fault(&f_ctx, 400, "bad request method: %s", binrpc_error(err) );
		goto error;
	}
	
	/* find_rpc_exports needs 0 terminated strings, but all str are
	 * 0 term by default */
	rpc_e=find_rpc_export(val.u.strval.s, 0);
	if ((rpc_e==0) || (rpc_e->function==0)){
		rpc_fault(&f_ctx, 500, "command %s not found", val.u.strval.s);
		goto end;
	}
	f_ctx.method=val.u.strval.s;
	rpc_e->function(&binrpc_callbacks, &f_ctx);
	if (f_ctx.replied==0){
		/* to get an error reply if the rpc handlers hasn't replied
		 *  uncomment the following code:
		 * if (binrpc_pkt_len(&f_ctx.out.pkt)==0){
			rpc_fault(&f_ctx, 500, "internal server error: no reply");
			LOG(L_ERR, "ERROR: rpc method %s hasn't replied\n",
					val.u.strval.s);
		}else  */
			rpc_send(&f_ctx);
	}
end:
	*bytes_needed=0; /* full read */
	destroy_binrpc_ctx(&f_ctx);
	return (int)(f_ctx.in.s-buf);
error:
	if (f_ctx.replied==0){
			rpc_fault(&f_ctx, 500, "internal server error");
			LOG(L_ERR, "ERROR: unknown rpc errror\n");
	}
	*bytes_needed=0; /* we don't need anymore crap */
	destroy_binrpc_ctx(&f_ctx);
	return -1;
more_data:
	destroy_binrpc_ctx(&f_ctx);
	return 0; /* nothing was processed */
}




static char* rpc_type_name(int type)
{
	switch(type){
		case BINRPC_T_INT:
			return "integer";
		case BINRPC_T_STR:
			return "string";
		case BINRPC_T_DOUBLE:
			return "float";
		case BINRPC_T_STRUCT:
			return "structure";
		case BINRPC_T_ARRAY:
			return "array";
		case BINRPC_T_AVP:
			return "structure member";
		case BINRPC_T_BYTES:
			return "bytes array";
		case BINRPC_T_ALL:
			return "any";
	}
	return "<unknown/error>";
};



/* rpc interface functions */

/* returns the number of parameters read
 * on error: - number of parameters read so far (<=0)*/
static int rpc_scan(struct binrpc_ctx* ctx, char* fmt, ...)
{
	va_list ap;
	struct binrpc_val v;
	int err;
	char* orig_fmt;
	
	va_start(ap, fmt);
	orig_fmt=fmt;
	for (;*fmt; fmt++){
		switch(*fmt){
			case 'b': /* bool */
			case 't': /* time */
			case 'd': /* int */
				v.type=BINRPC_T_INT;
				ctx->in.s=binrpc_read_record(&ctx->in.ctx, ctx->in.s,
												ctx->in.end, &v, &err);
				if (err<0) goto error_read;
				*(va_arg(ap, int*))=v.u.intval;
				break;
			case 'f':
				v.type=BINRPC_T_DOUBLE;
				ctx->in.s=binrpc_read_record(&ctx->in.ctx, ctx->in.s,
												ctx->in.end, &v, &err);
				if (err<0) goto error_read;
				*(va_arg(ap, double*))=v.u.fval;
				break;
			case 's': /* asciiz */
			case 'S': /* str */
				v.type=BINRPC_T_STR;
				v.u.strval.s="if you get this string, you don't"
							"check rpc_scan return code !!! (very bad)";
				v.u.strval.len=strlen(v.u.strval.s);
				ctx->in.s=binrpc_read_record(&ctx->in.ctx, ctx->in.s, 
												ctx->in.end, &v,&err);
				if (*fmt=='s'){
					*(va_arg(ap, char**))=v.u.strval.s; /* 0 term by proto*/
				}else{
					*(va_arg(ap, str*))=v.u.strval;
				}
				if (err<0) goto error_read;
				break;
			case '{': /* struct */
				v.type=BINRPC_T_STRUCT;
				/* FIXME: structure reading doesn't work for now */
#if 0
				ctx->in.s=binrpc_read_record(&ctx->in.ctx, ctx->in.s, 
												ctx->in.end, &v, &err);
				if (err<0) goto error_read;
				ctx->in.in_struct++;
				*(va_arg(ap, void**))=ctx; /* use the same context */
#endif
				goto error_not_supported;
				break;
			default:
				goto error_inv_param;
		}
		ctx->in.record_no++;
	}
	va_end(ap);
	return (int)(fmt-orig_fmt);
error_read:
	rpc_fault(ctx, 400, "error at parameter %d: expected %s type but"
						" %s", ctx->in.record_no, rpc_type_name(v.type),
						 binrpc_error(err));
	/*
	rpc_fault(ctx, 400, "invalid record %d, offset %d (expected %d type)"
						": %s", ctx->in.record_no, ctx->in.ctx.offset,
								v.type, binrpc_error(err));
	*/
	goto error_ret;
error_not_supported:
	rpc_fault(ctx, 500, "internal server error, type %d not supported",
						v.type);
	LOG(L_CRIT, "BUG: binrpc: rpc_scan: formatting char \'%c\'"
						" not supported\n", *fmt);
goto error_ret;
error_inv_param:
	rpc_fault(ctx, 500, "internal server error, invalid format char \'%c\'",
						*fmt);
error_ret:
	va_end(ap);
	return -(int)(fmt-orig_fmt);
}



/* returns  0 on success, -1 on error */
static int rpc_add(struct binrpc_ctx* ctx, char* fmt, ...)
{
	va_list ap;
	int err;
	char* s;
	str* st;
	struct rpc_struct_l* rs;
	
	va_start(ap, fmt);
	for (;*fmt; fmt++){
		switch(*fmt){
			case 'd':
			case 't':
			case 'b':
				err=binrpc_addint(&ctx->out.pkt, va_arg(ap, int));
				if (err<0) goto error_add;
				break;
			case 's': /* asciiz */
				s=va_arg(ap, char*);
				if (s==0) /* fix null strings */
					s="<null string>"; 
				err=binrpc_addstr(&ctx->out.pkt, s, strlen(s));
				if (err<0) goto error_add;
				break;
			case 'S': /* str */
				st=va_arg(ap, str*);
				err=binrpc_addstr(&ctx->out.pkt, st->s, st->len);
				if (err<0) goto error_add;
				break;
			case '{':
				err=binrpc_start_struct(&ctx->out.pkt);
				if (err<0) goto error_add;
				rs=new_rpc_struct();
				if (rs==0) goto error_mem;
				rs->offset=binrpc_pkt_len(&ctx->out.pkt);
				err=binrpc_end_struct(&ctx->out.pkt);
				if (err<0) goto error_add;
				clist_append(&ctx->out.structs, rs, next, prev);
				*(va_arg(ap, void**))=rs;
				break;
			case 'f': 
				err=binrpc_adddouble(&ctx->out.pkt, va_arg(ap, double));
				if (err<0) goto error_add;
				break;
			default: 
				rpc_fault(ctx, 500, "Internal server error: "
								"invalid formatting character \'%c\'", *fmt);
				LOG(L_CRIT, "BUG: binrpc: rpc_add: formatting char \'%c\'"
							" not supported\n", *fmt);
				goto error;
		}
	}
	va_end(ap);
	return 0;
error_mem:
	LOG(L_ERR, "ERROR: binrpc: rpc_add: out of memory\n");
	rpc_fault(ctx, 500, "Internal server error: out of memory");
	goto error;
error_add:
	rpc_fault(ctx, 500, "Internal server error processing \'%c\': %s (%d)",
				*fmt, binrpc_error(err), err);
error:
	va_end(ap);
	return -1;
}



#define RPC_PRINTF_BUF_SIZE	1024
/* returns  0 on success, -1 on error */
static int rpc_printf(struct binrpc_ctx* ctx, char* fmt, ...)
{
	va_list ap;
	char* buf;
	int len;
	int err;
	
	buf=pkg_malloc(RPC_PRINTF_BUF_SIZE);
	if (buf==0) goto error;
	va_start(ap, fmt);
	len=vsnprintf(buf, RPC_PRINTF_BUF_SIZE, fmt, ap);
	va_end(ap);
	if ((len<0) || (len> RPC_PRINTF_BUF_SIZE)){
		LOG(L_ERR, "ERROR: binrpc: rpc_printf: buffer size exceeded(%d)\n",
				RPC_PRINTF_BUF_SIZE);
		goto error;
	}
	if ((err=binrpc_addstr(&ctx->out.pkt, buf, len))<0){
		LOG(L_ERR, "ERROR: binrpc: rpc_printf: binrpc_addstr failed:"
					" %s (%d)\n", binrpc_error(err), err);
		goto error;
	}
	pkg_free(buf);
	return 0;
error:
	if (buf) pkg_free(buf);
	return -1;
}



/* returns  0 on success, -1 on error */
static int rpc_struct_add(struct rpc_struct_l* s, char* fmt, ...)
{
	va_list ap;
	int err;
	struct binrpc_val avp;
	struct rpc_struct_l* rs;
	
	memset(&avp, 0, sizeof(struct binrpc_val));
	va_start(ap, fmt);
	for (;*fmt; fmt++){
		avp.name.s=va_arg(ap, char*);
		avp.name.len=strlen(avp.name.s);
		switch(*fmt){
			case 'd':
			case 't':
			case 'b':
				avp.type=BINRPC_T_INT;
				avp.u.intval=va_arg(ap, int);
				break;
			case 's': /* asciiz */
				avp.type=BINRPC_T_STR;
				avp.u.strval.s=va_arg(ap, char*);
				avp.u.strval.len=strlen(avp.u.strval.s);
				break;
			case 'S': /* str */
				avp.type=BINRPC_T_STR;
				avp.u.strval=*(va_arg(ap, str*));
				break;
			case '{':
				avp.type=BINRPC_T_STRUCT;
				err=binrpc_addavp(&s->pkt, &avp);
				if (err<0) goto error_add;
				rs=new_rpc_struct();
				if (rs==0) goto error_mem;
				rs->offset=binrpc_pkt_len(&s->pkt);
				err=binrpc_end_struct(&s->pkt);
				if (err<0) goto error_add;
				clist_append(&s->substructs, rs, next, prev);
				*(va_arg(ap, void**))=rs;
				goto end;
			case 'f': 
				avp.type=BINRPC_T_DOUBLE;
				avp.u.fval=va_arg(ap, double);
				break;
			default: 
				LOG(L_CRIT, "BUG: binrpc: rpc_struct_add: formatting char"
							" \'%c\'" " not supported\n", *fmt);
				goto error;
		}
		err=binrpc_addavp(&s->pkt, &avp);
		if (err<0) goto error;
	}
end:
	va_end(ap);
	return 0;
error_mem:
error_add:
error:
	va_end(ap);
	return -1;
}



/* returns  0 on success, -1 on error */
static int rpc_struct_printf(struct rpc_struct_l *s, char* name,
								char* fmt, ...)
{
	va_list ap;
	char* buf;
	int len;
	int err;
	struct binrpc_val avp;
	
	buf=pkg_malloc(RPC_PRINTF_BUF_SIZE);
	if (buf==0) goto error;
	va_start(ap, fmt);
	len=vsnprintf(buf, RPC_PRINTF_BUF_SIZE, fmt, ap);
	va_end(ap);
	if ((len<0) || (len> RPC_PRINTF_BUF_SIZE)){
		LOG(L_ERR, "ERROR: binrpc: rpc_struct_printf:"
				" buffer size exceeded(%d)\n", RPC_PRINTF_BUF_SIZE);
		goto error;
	}
	avp.name.s=name;
	avp.name.len=strlen(name);
	avp.type=BINRPC_T_STR;
	avp.u.strval.s=name;
	avp.u.strval.len=strlen(avp.u.strval.s);
	
	if ((err=binrpc_addavp(&s->pkt, &avp))<0){
		LOG(L_ERR, "ERROR: binrpc: rpc_printf: binrpc_addavp failed:"
					" %s (%d)\n", binrpc_error(err), err);
		goto error;
	}
	pkg_free(buf);
	return 0;
error:
	if (buf) pkg_free(buf);
	return -1;
}



static int rpc_struct_scan(struct rpc_struct_l* s, char* fmt, ...)
{
	LOG(L_CRIT, "ERROR: binrpc:rpc_struct_scan: not implemented\n");
	return -1;
};
	
