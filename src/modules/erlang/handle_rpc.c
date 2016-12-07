/**
 * Copyright (C) 2015 Bicom Systems Ltd, (bicomsystems.com)
 *
 * Author: Seudin Kasumovic (seudin.kasumovic@gmail.com)
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
 *
 */

#include <stdarg.h>
#include <stdlib.h>

#include "../../mem/mem.h"
#include "../../dprint.h"

#include "handle_rpc.h"
#include "mod_erlang.h"

/* create empty recycle bin */
static struct erl_rpc_garbage *recycle_bin = 0;

static int get_int(int *int_ptr,erl_rpc_ctx_t *ctx, int reads, int autoconvert);
static int get_double(double *double_prt,erl_rpc_ctx_t *ctx, int reads, int autoconvert);
static int get_str(str *str_ptr, erl_rpc_ctx_t *ctx, int reads, int autoconvert);
static int find_member(erl_rpc_ctx_t *ctx, int arity, const char* member_name);
static int add_to_recycle_bin(int type, void* ptr, erl_rpc_ctx_t *ctx);

erl_rpc_param_t *erl_new_param(erl_rpc_ctx_t *ctx);
void erl_rpc_append_param(erl_rpc_ctx_t *ctx, erl_rpc_param_t *param);

/*
 * RPC holder
 */
rpc_t erl_rpc_func_param;

/*
 * Function returns always success - we uses EPMD for transport
 */
int erl_rpc_send(erl_rpc_ctx_t *ctx, int depth)
{
	if (ctx->response_sent) return 0;
	ctx->response_sent = 1;
	erl_rpc_ctx_t *handler;
	erl_rpc_param_t *fault = *(ctx->fault_p);

	if (fault)
	{
		LM_ERR("fault: %d %.*s\n",fault->type, STR_FMT(&fault->value.S));
		/* restore clear point */
		ctx->response->index = ctx->response_index;

		/* {error,{struct,[ {"code", 400}, {"error","Error message"}]}}*/
		if (ei_x_encode_tuple_header(ctx->response,1)) goto error;	/* {error,{_,_}} */
		if (rpc_reply_with_struct && ei_x_encode_atom(ctx->response,"struct")) goto error;	/* {error,{struct,_}} */
		if (ei_x_encode_list_header(ctx->response,2)) goto error;	/* {error,{struct,[_,_]}} */
		if (ei_x_encode_tuple_header(ctx->response,2)) goto error;	/* {error,{struct,[{_,_},_]}} */
		if (ei_x_encode_atom(ctx->response,"code")) goto error;		/* {error,{struct,[{code,_},_]}} */
		if (ei_x_encode_long(ctx->response,fault->type)) goto error;/* {error,{struct,[{code,400},_]}} */
		if (ei_x_encode_tuple_header(ctx->response,2)) goto error;	/* {error,{struct,[{code,400},{_,_}]}} */
		if (ei_x_encode_binary(ctx->response,"error",sizeof("error")-1)) goto error;	/* {error,{struct,[{code,400},{<<"error">>,_}]}} */
		if (ei_x_encode_binary(ctx->response,(void*)fault->value.S.s,fault->value.S.len)) /* {error,{struct,[{code,400},{<<"error">>,<<Msg>>}]}} */
			goto error;
		if (ei_x_encode_empty_list(ctx->response)) goto error;
	}
	else if (ctx->reply_params)
	{
		while(ctx->reply_params)
		{
			if (ctx->reply_params->member_name)
			{
				/* {"member_name", _} */
				if (ei_x_encode_tuple_header(ctx->response,2)) goto error;
				if (ei_x_encode_binary(ctx->response,ctx->reply_params->member_name, strlen(ctx->reply_params->member_name)))
					goto error;
			}
			/* {"member_name", MemberValue} */
			switch (ctx->reply_params->type) {
				case ERL_INTEGER_EXT:
					if(ei_x_encode_long(ctx->response,ctx->reply_params->value.n)) goto error;
					break;
				case ERL_FLOAT_EXT:
					if(ei_x_encode_double(ctx->response,ctx->reply_params->value.d)) goto error;
					break;
				case ERL_STRING_EXT:
					if(ei_x_encode_binary(ctx->response,ctx->reply_params->value.S.s,ctx->reply_params->value.S.len)) goto error;
					break;
				case ERL_SMALL_TUPLE_EXT: /* add as {struct,list(no_params)} */
					handler = (erl_rpc_ctx_t*)ctx->reply_params->value.handler;
					if (ei_x_encode_tuple_header(ctx->response,1)) goto error;
					if (rpc_reply_with_struct && ei_x_encode_atom(ctx->response,"struct")) goto error;
					if (ei_x_encode_list_header(ctx->response,handler->no_params)) goto error;
					if (erl_rpc_send(handler, depth++)) goto error;
					if (ei_x_encode_empty_list(ctx->response)) goto error;
					break;
				case ERL_LIST_EXT: /* add as [list(no_params)] */
					handler = (erl_rpc_ctx_t*)ctx->reply_params->value.handler;
					if (ei_x_encode_list_header(ctx->response,handler->no_params)) goto error;
					if (erl_rpc_send(handler, depth++)) goto error;
					if (handler->no_params)
						if (ei_x_encode_empty_list(ctx->response)) goto error;
					break;
				default:
					LM_ERR("Unknown type '%c' for encoding RPC reply\n",ctx->reply_params->type);
					break;
			}
			ctx->reply_params=ctx->reply_params->next;
		}
	}
	else if (!depth)
	{
		/* restore start point */
		LM_WARN("encode empty response -> ok");
		ctx->response->index = ctx->response_index;
		if (ei_x_encode_atom(ctx->response,"ok")) goto error;
	}

	return 0;

error:
	LM_ERR("error while encoding response\n");
	return -1;
}

void erl_rpc_fault(erl_rpc_ctx_t* ctx, int code, char* fmt, ...)
{
	static char buf[FAULT_BUF_LEN];
	erl_rpc_param_t *fault = *(ctx->fault_p);
	int len;

	va_list ap;

	if (fault) return;

	va_start(ap, fmt);
	len = vsnprintf(buf, FAULT_BUF_LEN, fmt, ap);
	va_end(ap);

	fault=(erl_rpc_param_t*)pkg_malloc(sizeof(erl_rpc_param_t));

	if (fault == 0)
	{
		LM_ERR("Not enough memory\n");
		return;
	}

	if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)fault,ctx))
	{
		pkg_free(fault);
		return;
	}

	fault->type = code;
	fault->value.S.s = buf;
	fault->value.S.len = len;
	ctx->fault = fault;
}

int erl_rpc_add(erl_rpc_ctx_t* ctx, char* fmt, ...)
{
	void **void_ptr;
	int int_ptr;
	double double_ptr;
	char *char_ptr;
	str *str_ptr;
	erl_rpc_ctx_t *handler;
	erl_rpc_param_t *param;

	int reads=0;

	va_list ap;

	va_start(ap,fmt);

	while(*fmt)
	{
		if ((param = erl_new_param(ctx))==0)
		{
			goto error;
		}

		switch(*fmt)
		{
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int);
			param->type = ERL_INTEGER_EXT;
			param->value.n = int_ptr;
			break;

		case 'f': /* double */
			double_ptr = va_arg(ap, double);
			param->type = ERL_FLOAT_EXT;
			param->value.d = double_ptr;
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);

			param->type = ERL_STRING_EXT;
			param->value.S = *str_ptr;

			break;

		case 's':/* zero terminated string */

			char_ptr = va_arg(ap, char *);

			param->type = ERL_STRING_EXT;
			param->value.S.len = strlen(char_ptr);

			param->value.S.s = (char*)pkg_malloc(param->value.S.len);

			if (!param->value.S.s)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR, param->value.S.s, ctx))
			{
				pkg_free(param->value.S.s);
				goto error;
			}

			memcpy(param->value.S.s,char_ptr,param->value.S.len);

			break;
		case '{':
			void_ptr = va_arg(ap,void**);

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			*handler = *ctx;
			handler->no_params = 0;
			handler->reply_params=0;
			handler->tail = 0;

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			param->type = ERL_SMALL_TUPLE_EXT;
			param->value.handler = (void*)handler;

			break;

		case '[':
			void_ptr = va_arg(ap,void**);

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			*handler = *ctx;
			handler->no_params = 0;
			handler->reply_params=0;
			handler->tail = 0;

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			param->type = ERL_LIST_EXT;
			param->value.handler = (void*)handler;

			break;
		 default:
			 LM_ERR("Invalid type '%c' in formatting string\n", *fmt);
			 goto error;
		}

		erl_rpc_append_param(ctx,param);

		reads++;
		fmt++;
	}
	va_end(ap);
	return reads;

error:
	LM_ERR("Failed to encode parameter #%d into response.\n",reads);
	va_end(ap);
	return -reads;
}

int erl_rpc_scan(erl_rpc_ctx_t* ctx, char* fmt, ...)
{
	int* int_ptr;
	char** char_ptr;
	str* str_ptr;
	double* double_ptr;
	void** void_ptr;
	str s; /* helper str */

	int reads = 0;
	int modifiers = 0;
	int autoconv = 0;

	int type,size;
	erl_rpc_ctx_t *handler;

	va_list ap;

	va_start(ap,fmt);

	while(*fmt && ctx->size)
	{
		/* optional and we at the end of decoding params */
		if (ctx->optional && !ctx->size)
		{
			break;
		}

		if (ei_get_type(ctx->request->buff,&ctx->request_index,&type,&size))
		{
			erl_rpc_fault(ctx,400,"Can't determine data type, for parameter #%d",reads);
			LM_ERR("Can't determine data type, for parameter #%d",reads);

			goto error;
		}

		switch(*fmt)
		{
		case '*': /* start of optional parameters */
			modifiers++;
			ctx->optional = 1;
			reads++;
			fmt++;
			continue;
		case '.':  /* autoconvert */
			modifiers++;
			autoconv = 1;
			reads++;
			fmt++;
			continue;
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);

			if (get_int(int_ptr,ctx,reads,autoconv))
			{
				goto error;
			}

			break;
		case 'f': /* double */
			double_ptr = va_arg(ap, double*);

			if (get_double(double_ptr,ctx,reads,autoconv))
			{
				goto error;
			}

			break;
		case 'S': /* str structure */

			str_ptr = va_arg(ap, str*);

			if (get_str(str_ptr,ctx,reads,autoconv))
			{
				goto error;
			}

			break;
		case 's':/* zero terminated string */

			char_ptr = va_arg(ap, char **);
			if (get_str(&s,ctx,reads,autoconv))
			{
				goto error;
			}

			*char_ptr = s.s;

			break;
		case '{':
			void_ptr = va_arg(ap,void**);

			if (type!=ERL_SMALL_TUPLE_EXT && type!=ERL_LARGE_TUPLE_EXT)
			{
				erl_rpc_fault(ctx,400,"Bad type of parameter #%d (t=%c).",reads,type);
				goto error;
			}

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
				LM_ERR("Not enough memory\n");
				goto error;
			}

			*handler = *ctx; /* copy state */
			handler->optional = 0;
			handler->no_params = 0;
			handler->size = size; /* size of tuple */

			if (add_to_recycle_bin(JUNK_PKGCHAR,handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			/* skip element */
			if (ei_skip_term(ctx->request->buff,&ctx->request_index))
			{
				goto error;
			}

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			break;
		 default:
			 LM_ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			 erl_rpc_fault(ctx, 500, "Server Internal Error (Invalid Formatting String)");
			 goto error;
		}

		autoconv = 0; /* reset autovoncersion for next parameter */
		reads++;
		fmt++;
		ctx->size--;
	}
    va_end(ap);
    return reads-modifiers;

error:
    va_end(ap);
    return -(reads-modifiers);

}

int erl_rpc_struct_scan(erl_rpc_ctx_t* ctx, char* fmt, ...)
{
	int* int_ptr;
	char** char_ptr;
	str* str_ptr;
	double* double_ptr;
	char* member_name;
	str s; /* helper str */

	int reads = 0;
	int modifiers = 0;
	int index;
	int autoconv = 0;

	int arity;

	va_list ap;

	/* save index */
	index = ctx->request_index;

	if(ei_decode_tuple_header(ctx->request->buff,&ctx->request_index, &arity))
	{
		erl_rpc_fault(ctx,400,"Bad tuple");
		return -1;
	}

	va_start(ap,fmt);

	while(*fmt)
	{
		member_name = va_arg(ap, char*);

		if (find_member(ctx,arity,member_name))
		{
			goto error;
		}

		switch(*fmt)
		{
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			int_ptr = va_arg(ap, int*);

			if (get_int(int_ptr,ctx,reads,autoconv))
			{
				goto error;
			}

			break;
		case 'f': /* double */
			double_ptr = va_arg(ap, double*);

			if (get_double(double_ptr,ctx,reads,autoconv))
			{
				goto error;
			}

			break;
		case 'S': /* str structure */

			str_ptr = va_arg(ap, str*);

			if (get_str(str_ptr,ctx,reads,autoconv))
			{
				goto error;
			}

			break;
		case 's':/* zero terminated string */

			char_ptr = va_arg(ap,char**);

			if (get_str(&s,ctx,reads,autoconv))
			{
				goto error;
			}

			*char_ptr = s.s;

			break;

		 default:
			 LM_ERR("Invalid parameter type in formatting string: %c\n", *fmt);
			 erl_rpc_fault(ctx, 500, "Server Internal Error (Invalid Formatting String)");
			 goto error;
		}

		reads++;
		fmt++;
	}

	/* restore index */
	ctx->request_index = index;

    va_end(ap);
    return reads-modifiers;

error:
    va_end(ap);
    return -(reads-modifiers);
}

#define RPC_BUF_SIZE 1024

/*
 * adds formated string into RPC response buffer as Erlang string/list
 */
int erl_rpc_printf(erl_rpc_ctx_t* ctx, char* fmt, ...)
{
	int n, buff_size;
	char *buff = 0;
	va_list ap;
	erl_rpc_param_t *param;

	buff = (char*)pkg_malloc(RPC_BUF_SIZE);
	if (!buff) {
			erl_rpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			return -1;
	}

	buff_size = RPC_BUF_SIZE;

	while(1)
	{
		/* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buff, buff_size, fmt, ap);
		va_end(ap);
			 /* If that worked, return the string. */
		if (n > -1 && n < buff_size)
		{
			if(add_to_recycle_bin(JUNK_PKGCHAR,(void*)buff,ctx))
			{
				goto error;
			}
			else if ((param = erl_new_param(ctx)))
			{
				param->type = ERL_STRING_EXT;
				param->value.S.s = buff;
				param->value.S.len = n;
				erl_rpc_append_param(ctx,param);
			}
			else
			{
				goto error;
			}

			return 0;
		}

		/* Else try again with more space. */
		if (n > -1)
		{	/* glibc 2.1 */
			buff_size = n + 1; /* precisely what is needed */
		}
		else
		{	/* glibc 2.0 */
			buff_size *= 2;  /* twice the old size */
		}
		if ((buff = pkg_realloc(buff, buff_size)) == 0)
		{
			erl_rpc_fault(ctx, 500, "Internal Server Error (No memory left)");
			ERR("No memory left\n");
			goto error;
		}
	}

	return 0;

error:
	if(buff) pkg_free(buff);
	return -1;
}

int erl_rpc_struct_add(erl_rpc_ctx_t* ctx, char* fmt, ...)
{
	void **void_ptr;
	char *char_ptr;
	str *str_ptr;
	erl_rpc_ctx_t *handler;
	erl_rpc_param_t *param;

	int reads=0;

	va_list ap;

	va_start(ap,fmt);

	while(*fmt)
	{
		if ((param = erl_new_param(ctx))==0)
		{
			goto error;
		}

		param->member_name = va_arg(ap, char*);

		switch(*fmt)
		{
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			param->type = ERL_INTEGER_EXT;
			param->value.n = va_arg(ap, int);;
			break;

		case 'f': /* double */
			param->type = ERL_FLOAT_EXT;
			param->value.d = va_arg(ap, double);
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);

			param->type = ERL_STRING_EXT;
			param->value.S = *str_ptr;
			break;

		case 's':/* zero terminated string */

			char_ptr = va_arg(ap, char *);

			param->type = ERL_STRING_EXT;
			param->value.S.len = strlen(char_ptr);

			param->value.S.s = (char*)pkg_malloc(param->value.S.len);

			if (!param->value.S.s)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR, param->value.S.s, ctx))
			{
				pkg_free(param->value.S.s);
				goto error;
			}

			memcpy(param->value.S.s,char_ptr,param->value.S.len);

			break;

		case '{':
			void_ptr = va_arg(ap,void**);

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			*handler = *ctx;
			handler->no_params = 0;
			handler->reply_params=0;
			handler->tail = 0;

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			param->type = ERL_SMALL_TUPLE_EXT;
			param->value.handler = (void*)handler;

			break;

		case '[':
			void_ptr = va_arg(ap,void**);

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			*handler = *ctx;
			handler->no_params = 0;
			handler->reply_params=0;
			handler->tail = 0;

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			param->type = ERL_LIST_EXT;
			param->value.handler = (void*)handler;

			break;

		 default:
			 LM_ERR("Invalid type '%c' in formatting string\n", *fmt);
			 goto error;
		}

		erl_rpc_append_param(ctx,param);

		reads++;
		fmt++;
	}
	va_end(ap);
	return reads;

error:

	LM_ERR("Failed to encode parameter #%d into response.\n",reads);
	va_end(ap);
	return -reads;
}

int erl_rpc_array_add(erl_rpc_ctx_t* ctx, char* fmt, ...)
{
	void **void_ptr;
	char *char_ptr;
	str *str_ptr;
	erl_rpc_ctx_t *handler;
	erl_rpc_param_t *param;

	int reads=0;

	va_list ap;

	va_start(ap,fmt);

	LM_DBG("ctx=%p add fmt=<%s>\n",(void*)ctx,fmt);

	while(*fmt)
	{
		if ((param = erl_new_param(ctx))==0)
		{
			goto error;
		}

		param->member_name = NULL;

		switch(*fmt)
		{
		case 'b': /* Bool */
		case 't': /* Date and time */
		case 'd': /* Integer */
			param->type = ERL_INTEGER_EXT;
			param->value.n = va_arg(ap, int);;
			break;

		case 'f': /* double */
			param->type = ERL_FLOAT_EXT;
			param->value.d = va_arg(ap, double);
			break;

		case 'S': /* str structure */
			str_ptr = va_arg(ap, str*);

			param->type = ERL_STRING_EXT;
			param->value.S = *str_ptr;
			break;

		case 's':/* zero terminated string */

			char_ptr = va_arg(ap, char *);

			param->type = ERL_STRING_EXT;
			param->value.S.len = strlen(char_ptr);

			param->value.S.s = (char*)pkg_malloc(param->value.S.len);

			if (!param->value.S.s)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR, param->value.S.s, ctx))
			{
				pkg_free(param->value.S.s);
				goto error;
			}

			memcpy(param->value.S.s,char_ptr,param->value.S.len);

			break;

		case '{':
			void_ptr = va_arg(ap,void**);

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			*handler = *ctx;
			handler->no_params = 0;
			handler->reply_params=0;
			handler->tail = 0;

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			param->type = ERL_SMALL_TUPLE_EXT;
			param->value.handler = (void*)handler;

			break;

		case '[':
			void_ptr = va_arg(ap,void**);

			handler = (erl_rpc_ctx_t*)pkg_malloc(sizeof(erl_rpc_ctx_t));

			if (!handler)
			{
				LM_ERR("Not enough memory\n");
				goto error;
			}

			if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)handler,ctx))
			{
				pkg_free(handler);
				goto error;
			}

			*handler = *ctx;
			handler->no_params = 0;
			handler->reply_params=0;
			handler->tail = 0;

			/* go where we stopped */
			*(erl_rpc_ctx_t**)void_ptr = handler;

			param->type = ERL_LIST_EXT;
			param->value.handler = (void*)handler;

			break;

		 default:
			 LM_ERR("Invalid type '%c' in formatting string\n", *fmt);
			 goto error;
		}

		erl_rpc_append_param(ctx,param);

		reads++;
		fmt++;
	}
	va_end(ap);
	return reads;

error:

	LM_ERR("Failed to encode parameter #%d into response.\n",reads);
	va_end(ap);
	return -reads;
}

int erl_rpc_struct_printf(erl_rpc_ctx_t* ctx, char* name, char* fmt, ...)
{
	int n, buff_size;
	char *buff;
	va_list ap;
	erl_rpc_param_t *param;

	LM_ERR("parsing name:%s fmt: %s\n",name, fmt);

	buff = (char*)pkg_malloc(RPC_BUF_SIZE);
	if (!buff) {
			ERR("No memory left\n");
			return -1;
	}

	buff_size = RPC_BUF_SIZE;

	while(1)
	{
		/* Try to print in the allocated space. */
		va_start(ap, fmt);
		n = vsnprintf(buff, buff_size, fmt, ap);
		va_end(ap);
			 /* If that worked, return the string. */
		if (n > -1 && n < buff_size)
		{

			if(add_to_recycle_bin(JUNK_PKGCHAR,(void*)buff,ctx))
			{
				goto error;
			}
			else if ((param = erl_new_param(ctx)))
			{
				param->type = ERL_STRING_EXT;
				param->value.S.s = buff;
				param->value.S.len = n;
				param->member_name = name;
				erl_rpc_append_param(ctx,param);
			}
			else
			{
				goto error;
			}

			return 0;
		}

		/* Else try again with more space. */
		if (n > -1)
		{	/* glibc 2.1 */
			buff_size = n + 1; /* precisely what is needed */
		}
		else
		{	/* glibc 2.0 */
			buff_size *= 2;  /* twice the old size */
		}
		if ((buff = pkg_realloc(buff, buff_size)) == 0)
		{
			ERR("No memory left\n");
			goto error;
		}
	}

	return 0;

error:
	if(buff) pkg_free(buff);
	return -1;
}

int erl_rpc_capabilities(erl_rpc_ctx_t* ctx)
{
	return 0; /* no RPC_DELAYED_REPLY */
}

/** Add a memory to the list of memory blocks that
 * need to be re-claimed later.
 *
 * @param type The type of the memory block.
 * @param ptr A pointer to the memory block.
 * @param ctx The context.
 * @return 0 on success, a negative number on error.
 * @sa empty_recycle_bin()
 */
static int add_to_recycle_bin(int type, void *ptr, erl_rpc_ctx_t *ctx)
{
	struct erl_rpc_garbage *p;

	p = (struct erl_rpc_garbage*)pkg_malloc(sizeof(struct erl_rpc_garbage));

	if (!p)
	{
			LM_ERR("Not enough memory\n");
			return -1;
	}

	p->type = type;
	p->ptr = ptr;
	p->next = recycle_bin;
	recycle_bin = p;
	return 0;
}

/** Re-claims all memory allocated in the process of building XML-RPC
 * reply.
 */
void empty_recycle_bin(void)
{
        struct erl_rpc_garbage* p;
             /* Collect garbage */
        while(recycle_bin)
        {
                p = recycle_bin;
                recycle_bin = recycle_bin->next;
                switch(p->type)
                {
                case JUNK_EI_X_BUFF:

                	if (p->ptr)
                	{
                		ei_x_free((ei_x_buff*)p->ptr);
						pkg_free(p->ptr);
					}

					break;

                case JUNK_PKGCHAR:

                	if (p->ptr)
					{
						pkg_free(p->ptr);
					}

					break;

                default:
                	ERR("BUG: Unsupported junk type\n");
                }

                pkg_free(p);
        }
}

/*
 * Get int parameter
 */
static int get_int(int *int_ptr,erl_rpc_ctx_t *ctx, int reads, int autoconvert)
{
	int type, size;
	char *p;
	char *endptr;
	double d;
	long l;

	if (ei_get_type(ctx->request->buff,&ctx->request_index,&type,&size))
	{
		if(ctx->optional) return 0;

		erl_rpc_fault(ctx,400,"Can't determine data type of parameter #%d",reads);
		return -1;
	}

	switch(type)
	{
	case ERL_SMALL_INTEGER_EXT:
	case ERL_INTEGER_EXT:
		if(ei_decode_long(ctx->request->buff, &ctx->request_index, &l))
		{
			erl_rpc_fault(ctx,400,"Bad value of parameter #%d.",reads);
			return -1;
		}
		*int_ptr = (int)l;

		break;
	case ERL_STRING_EXT:
	case ERL_LIST_EXT:

		if (autoconvert == 0)
		{
			erl_rpc_fault(ctx,400,"Bad type of parameter #%d",reads);
			return -1;
		}

		/* allocate buffer */
		p = (char*)pkg_malloc(size+1);

		if (!p)
		{
			erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
			LM_ERR("Not enough memory\n");
			return -1;
		}

		*int_ptr = strtol(p,&endptr,10);
		if (p == endptr)
		{
			erl_rpc_fault(ctx,400,"Unable to convert %s into integer for parameter at position %d",p,reads);
			pkg_free(p);
			return -1;
		}

		pkg_free(p);
		break;

	case ERL_FLOAT_EXT:

		if (autoconvert == 0)
		{
			erl_rpc_fault(ctx,400,"Bad type of parameter #%d",reads);
			return -1;
		}

		if (ei_decode_double(ctx->request->buff,&ctx->request_index,&d))
		{
			erl_rpc_fault(ctx,400, "Can't read parameter #%d",reads);
			return -1;
		}

		*int_ptr=(int)d;
		break;

	default:
		LM_ERR("Unsupported type ('%c') for conversion into integer parameter #%d.\n",type,reads);
		erl_rpc_fault(ctx,400,"Unsupported type ('%c') for conversion into integer parameter #%d.",type,reads);
		return -1;
	}

	return 0;
}

static int get_double(double *double_prt,erl_rpc_ctx_t *ctx, int reads, int autoconvert)
{
	int type, size;
	char *p;
	char *endptr;
	long n;

	if (ei_get_type(ctx->request->buff,&ctx->request_index,&type,&size)){
		erl_rpc_fault(ctx,400,"Can't determine data type of parameter #%d",reads);
		return -1;
	}

	switch(type)
	{
	case ERL_FLOAT_EXT:

		if (ei_decode_double(ctx->request->buff,&ctx->request_index,double_prt))
		{
			erl_rpc_fault(ctx,400, "Bad value of parameter #%d.",reads);
			return -1;
		}

		break;
	case ERL_STRING_EXT:
	case ERL_LIST_EXT:

		if (autoconvert == 0)
		{
			erl_rpc_fault(ctx,400,"Bad type of parameter #%d",reads);
			return -1;
		}

		/* allocate buffer */
		p = (char*)pkg_malloc(size+1);

		if (!p)
		{
			erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
			LM_ERR("Not enough memory\n");
			return -1;
		}

		*double_prt = strtod(p,&endptr);
		if (p == endptr)
		{
			erl_rpc_fault(ctx,400,"Unable to convert %s into double, parameter at position #%d",p,reads);
			pkg_free(p);
			return -1;
		}

		pkg_free(p);
		break;

	case ERL_SMALL_INTEGER_EXT:
	case ERL_INTEGER_EXT:

		if (autoconvert == 0)
		{
			erl_rpc_fault(ctx,400,"Bad type of parameter #%d",reads);
			return -1;
		}

		if(ei_decode_long(ctx->request->buff, &ctx->request_index, &n))
		{
			erl_rpc_fault(ctx,400,"Can't read parameter #%d",reads);
			return -1;
		}

		*double_prt=n;

		break;
	default:
		erl_rpc_fault(ctx,400,"Can't convert to double parameter #%d.",reads);
		return -1;
	}

	return 0;
}

#define MAX_DIGITS	20

static int get_str(str *str_ptr, erl_rpc_ctx_t *ctx, int reads, int autoconvert)
{
	int type, size;
	char *p;
	double d;
	long n;

	if (ei_get_type(ctx->request->buff,&ctx->request_index,&type,&size))
	{
		erl_rpc_fault(ctx,400,"Can't determine data type of parameter #%d",reads);
		return -1;
	}

	switch(type)
	{
	case ERL_FLOAT_EXT:

		if (autoconvert == 0)
		{
			erl_rpc_fault(ctx,400,"Bad type of parameter #%d",reads);
			return -1;
		}

		if (ei_decode_double(ctx->request->buff,&ctx->request_index,&d))
		{
			erl_rpc_fault(ctx,400, "Bad value of parameter #%d.",reads);
			return -1;
		}

		p=(char*)pkg_malloc(MAX_DIGITS);

		if (!p)
		{
			erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
			LM_ERR("Not enough memory\n");
			return -1;
		}

		if (add_to_recycle_bin(JUNK_PKGCHAR, p, ctx))
		{
			pkg_free(p);
			return -1;
		}

		str_ptr->len=snprintf(p, MAX_DIGITS, "%f", d);
		str_ptr->s = p;

		break;

	case ERL_STRING_EXT:
	case ERL_LIST_EXT:
	case ERL_BINARY_EXT:

		/* allocate buffer */
		p = (char*)pkg_malloc(size+1);

		if (!p)
		{
			erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
			LM_ERR("Not enough memory\n");
			return -1;
		}

		if (add_to_recycle_bin(JUNK_PKGCHAR, p, ctx))
		{
			pkg_free(p);
			return -1;
		}

		if(ei_decode_strorbin(ctx->request->buff,&ctx->request_index,size+1,p))
		{
			erl_rpc_fault(ctx,400, "Can't read parameter #%d",reads);
			return -1;
		}

		str_ptr->s=p;
		str_ptr->len=size;

		break;

	case ERL_SMALL_INTEGER_EXT:
	case ERL_INTEGER_EXT:

		if (autoconvert == 0)
		{
			erl_rpc_fault(ctx,400,"Bad type of parameter #%d",reads);
			return -1;
		}

		if (ei_decode_long(ctx->request->buff,&ctx->request_index,&n))
		{
			erl_rpc_fault(ctx,400, "Bad value of parameter #%d.",reads);
			return -1;
		}

		p=(char*)pkg_malloc(MAX_DIGITS);

		if (!p)
		{
			erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
			LM_ERR("Not enough memory\n");
			return -1;
		}

		if (add_to_recycle_bin(JUNK_PKGCHAR, p, ctx))
		{
			pkg_free(p);
			return -1;
		}

		str_ptr->len=snprintf(p, MAX_DIGITS, "%ld", n);
		str_ptr->s = p;

		break;
	default:
		erl_rpc_fault(ctx,400,"Can't convert to string parameter #%d.",reads);
		return -1;
	}

	LM_ERR("parameter #%d:<%.*s>\n",reads,STR_FMT(str_ptr));

	return 0;
}

/*
 * Find member in tuple (aka RPC struct)
 */
static int find_member(erl_rpc_ctx_t *ctx, int arity, const char* member_name)
{
	int index,i=0;
	int type,size;
	char key_name[MAXATOMLEN];

	/* save position */
	index = ctx->request_index;

	/* { name, Value, name, Value...} */
	while (i < arity)
	{
		if (ei_get_type(ctx->request->buff,&ctx->request_index,&type,&size))
		{
			erl_rpc_fault(ctx,400,"Bad struct member type");
			goto error;
		}

		if(ei_decode_atom(ctx->request->buff,&ctx->request_index, key_name))
		{
			erl_rpc_fault(ctx,400,"Bad member name");
			goto error;
		}

		if (strcasecmp(member_name,key_name))
		{
			if(ei_skip_term(ctx->request->buff,&ctx->request_index))
			{
				erl_rpc_fault(ctx,400,"Unexpected end of struct tuple");
				goto error;
			}
			continue;
		}
		else
		{
			/* return at current position */
			return 0;
		}

		i++;
	}

	erl_rpc_fault(ctx,400, "Member %s not found",member_name);

error:
	ctx->request_index = index;
	return -1;
}

void init_rpc_handlers()
{
	erl_rpc_func_param.send = (rpc_send_f)erl_rpc_send;
	erl_rpc_func_param.fault = (rpc_fault_f)erl_rpc_fault;
	erl_rpc_func_param.add = (rpc_add_f)erl_rpc_add;
	erl_rpc_func_param.scan = (rpc_scan_f)erl_rpc_scan;
	erl_rpc_func_param.rpl_printf = (rpc_rpl_printf_f)erl_rpc_printf;
	erl_rpc_func_param.struct_add = (rpc_struct_add_f)erl_rpc_struct_add;
	erl_rpc_func_param.array_add = (rpc_array_add_f)erl_rpc_array_add;
	erl_rpc_func_param.struct_scan = (rpc_struct_scan_f)erl_rpc_struct_scan;
	erl_rpc_func_param.struct_printf = (rpc_struct_printf_f)erl_rpc_struct_printf;
	erl_rpc_func_param.capabilities = (rpc_capabilities_f)erl_rpc_capabilities;
	erl_rpc_func_param.delayed_ctx_new = 0;
	erl_rpc_func_param.delayed_ctx_close = 0;
}

erl_rpc_param_t *erl_new_param(erl_rpc_ctx_t *ctx)
{
	erl_rpc_param_t *p = (erl_rpc_param_t *)pkg_malloc(sizeof(erl_rpc_param_t));

	if (add_to_recycle_bin(JUNK_PKGCHAR,(void*)p,ctx))
	{
		erl_rpc_fault(ctx,500, "Internal Server Error (No memory left)");
		LM_ERR("Not enough memory\n");

		pkg_free(p);
		return 0;
	}

	p->next = 0;
	p->member_name = 0;
	return p;
}

void erl_rpc_append_param(erl_rpc_ctx_t *ctx, erl_rpc_param_t *param)
{

	if (ctx->tail)
	{
		ctx->tail->next = param;
		ctx->tail = param;
	}
	else
	{
		ctx->reply_params = ctx->tail = param;
	}

	param->next = 0;
	ctx->no_params++;
}
