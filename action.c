/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
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

/*!
 * \file
 * \brief Kamailio core :: Config file actions
 * \ingroup core
 * Module: \ref core
 */



#include "comp_defs.h"

#include "action.h"
#include "config.h"
#include "error.h"
#include "dprint.h"
#include "proxy.h"
#include "forward.h"
#include "udp_server.h"
#include "route.h"
#include "parser/msg_parser.h"
#include "parser/parse_uri.h"
#include "ut.h"
#include "lvalue.h"
#include "sr_module.h"
#include "select_buf.h"
#include "mem/mem.h"
#include "globals.h"
#include "dset.h"
#include "onsend.h"
#include "resolve.h"
#ifdef USE_TCP
#include "tcp_server.h"
#endif
#ifdef USE_SCTP
#include "sctp_core.h"
#endif
#include "switch.h"
#include "events.h"
#include "cfg/cfg_struct.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>


#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

int _last_returned_code  = 0;
struct onsend_info* p_onsend=0; /* onsend route send info */

/* current action executed from config file */
static cfg_action_t *_cfg_crt_action = 0;

/*!< maximum number of recursive calls for blocks of actions */
static unsigned int max_recursive_level = 256;

void set_max_recursive_level(unsigned int lev)
{
	max_recursive_level = lev;
}

/* return current action executed from config file */
cfg_action_t *get_cfg_crt_action(void)
{
	return _cfg_crt_action;
}

/* return line in config for current executed action */
int get_cfg_crt_line(void)
{
	if(_cfg_crt_action==0)
		return 0;
	return _cfg_crt_action->cline;
}

/* return name of config for current executed action */
char *get_cfg_crt_name(void)
{
	if(_cfg_crt_action==0)
		return 0;
	return _cfg_crt_action->cfile;
}

/* handle the exit code of a module function call.
 * (used internally in do_action())
 * @param h - script handle (h->last_retcode and h->run_flags will be set).
 * @param ret - module function (v0 or v2) retcode
 * Side-effects: sets _last_returned_code
 */
#define MODF_HANDLE_RETCODE(h, ret) \
	do { \
		/* if (unlikely((ret)==0)) (h)->run_flags|=EXIT_R_F; */ \
		(h)->run_flags |= EXIT_R_F & (((ret) != 0) -1); \
		(h)->last_retcode=(ret); \
		_last_returned_code = (h)->last_retcode; \
	} while(0)



/* frees parameters converted using MODF_RVE_PARAM_CONVERT() from dst.
 * (used internally in do_action())
 * Assumes src is unchanged.
 * Side-effects: clobbers i (int).
 */
#define MODF_RVE_PARAM_FREE(cmd, src, dst) \
		for (i=0; i < (dst)[1].u.number; i++) { \
			if ((src)[i+2].type == RVE_ST && (dst)[i+2].u.data) { \
				if ((dst)[i+2].type == RVE_FREE_FIXUP_ST) {\
					/* call free_fixup (which should restore the original
					   string) */ \
					(void)call_fixup((cmd)->free_fixup, &(dst)[i+2].u.data, i+1); \
				} else if ((dst)[i+2].type == FPARAM_DYN_ST) {\
					/* completely frees fparam and restore original string */\
					fparam_free_restore(&(dst)[i+2].u.data); \
				} \
				/* free allocated string */ \
				pkg_free((dst)[i+2].u.data); \
				(dst)[i+2].u.data = 0; \
			} \
		}


/* fills dst from src, converting RVE_ST params to STRING_ST.
 * (used internally in do_action())
 * @param src - source action_u_t array, as in the action structure
 * @param dst - destination action_u_t array, will be filled from src.
 * WARNING: dst must be cleaned when done, use MODULE_RVE_PARAM_FREE()
 * Side-effects: clobbers i (int), s (str), rv (rvalue*), might jump to error.
 */
#define MODF_RVE_PARAM_CONVERT(h, msg, cmd, src, dst) \
	do { \
		(dst)[1]=(src)[1]; \
		for (i=0; i < (src)[1].u.number; i++) { \
			if ((src)[2+i].type == RVE_ST) { \
				rv=rval_expr_eval((h), (msg), (src)[i+2].u.data); \
				if (unlikely(rv == 0 || \
					rval_get_str((h), (msg), &s, rv, 0) < 0)) { \
					rval_destroy(rv); \
					ERR("failed to convert RVE to string\n"); \
					(dst)[1].u.number = i; \
					MODF_RVE_PARAM_FREE(cmd, src, dst); \
					goto error; \
				} \
				(dst)[i+2].type = STRING_RVE_ST; \
				(dst)[i+2].u.string = s.s; \
				(dst)[i+2].u.str.len = s.len; \
				rval_destroy(rv); \
				if ((cmd)->fixup) {\
					if ((cmd)->free_fixup) {\
						if (likely( call_fixup((cmd)->fixup, \
										&(dst)[i+2].u.data, i+1) >= 0) ) { \
							/* success => mark it for calling free fixup */ \
							if (likely((dst)[i+2].u.data != s.s)) \
								(dst)[i+2].type = RVE_FREE_FIXUP_ST; \
						} else { \
							/* error calling fixup => mark conv. parameter \
							   and return error */ \
							(dst)[1].u.number = i; \
							ERR("runtime fixup failed for %s param %d\n", \
									(cmd)->name, i+1); \
							MODF_RVE_PARAM_FREE(cmd, src, dst); \
							goto error; \
						} \
					} else if ((cmd)->fixup_flags & FIXUP_F_FPARAM_RVE) { \
						if (likely( call_fixup((cmd)->fixup, \
										&(dst)[i+2].u.data, i+1) >= 0)) { \
							if ((dst)[i+2].u.data != s.s) \
								(dst)[i+2].type = FPARAM_DYN_ST; \
						} else { \
							/* error calling fixup => mark conv. parameter \
							   and return error */ \
							(dst)[1].u.number = i; \
							ERR("runtime fixup failed for %s param %d\n", \
									(cmd)->name, i+1); \
							MODF_RVE_PARAM_FREE(cmd, src, dst); \
							goto error; \
						}\
					} \
				} \
			} else \
				(dst)[i+2]=(src)[i+2]; \
		} \
	} while(0)



/* call a module function with normal STRING_ST params.
 * (used internally in do_action())
 * @param f_type - cmd_function type
 * @param h
 * @param msg
 * @param src - source action_u_t array (e.g. action->val)
 * @param params... - variable list of parameters, passed to the module
 *               function
 * Side-effects: sets ret, clobbers i (int), s (str), rv (rvalue*), cmd,
 *               might jump to error.
 *
 */
#ifdef __SUNPRO_C
#define MODF_CALL(f_type, h, msg, src, ...) \
	do { \
		cmd=(src)[0].u.data; \
		ret=((f_type)cmd->function)((msg), __VAR_ARGS__); \
		MODF_HANDLE_RETCODE(h, ret); \
	} while (0)
#else  /* ! __SUNPRO_C  (gcc, icc a.s.o) */
#define MODF_CALL(f_type, h, msg, src, params...) \
	do { \
		cmd=(src)[0].u.data; \
		ret=((f_type)cmd->function)((msg), ## params ); \
		MODF_HANDLE_RETCODE(h, ret); \
	} while (0)
#endif /* __SUNPRO_C */



/* call a module function with possible RVE params.
 * (used internally in do_action())
 * @param f_type - cmd_function type
 * @param h
 * @param msg
 * @param src - source action_u_t array (e.g. action->val)
 * @param dst - temporary action_u_t array used for conversions. It can be
 *              used for the function parameters. It's contents it's not
 *              valid after the call.
 * @param params... - variable list of parameters, passed to the module
 *               function
 * Side-effects: sets ret, clobbers i (int), s (str), rv (rvalue*), f, dst,
 *               might jump to error.
 *
 */
#ifdef __SUNPRO_C
#define MODF_RVE_CALL(f_type, h, msg, src, dst, ...) \
	do { \
		cmd=(src)[0].u.data; \
		MODF_RVE_PARAM_CONVERT(h, msg, cmd, src, dst); \
		ret=((f_type)cmd->function)((msg), __VAR_ARGS__); \
		MODF_HANDLE_RETCODE(h, ret); \
		/* free strings allocated by us or fixups */ \
		MODF_RVE_PARAM_FREE(cmd, src, dst); \
	} while (0)
#else  /* ! __SUNPRO_C  (gcc, icc a.s.o) */
#define MODF_RVE_CALL(f_type, h, msg, src, dst, params...) \
	do { \
		cmd=(src)[0].u.data; \
		MODF_RVE_PARAM_CONVERT(h, msg, cmd, src, dst); \
		ret=((f_type)cmd->function)((msg), ## params ); \
		MODF_HANDLE_RETCODE(h, ret); \
		/* free strings allocated by us or fixups */ \
		MODF_RVE_PARAM_FREE(cmd, src, dst); \
	} while (0)
#endif /* __SUNPRO_C */


/* ret= 0! if action -> end of list(e.g DROP),
      > 0 to continue processing next actions
   and <0 on error */
int do_action(struct run_act_ctx* h, struct action* a, struct sip_msg* msg)
{
	int ret;
	int v;
	struct dest_info dst;
	char* tmp;
	char *new_uri, *end, *crt;
	sr31_cmd_export_t* cmd;
	int len;
	int user;
	struct sip_uri uri, next_hop;
	struct sip_uri *u;
	unsigned short port;
	str* dst_host;
	int i, flags;
	avp_t* avp;
	struct search_state st;
	struct switch_cond_table* sct;
	struct switch_jmp_table*  sjt;
	struct rval_expr* rve;
	struct match_cond_table* mct;
	struct rvalue* rv;
	struct rvalue* rv1;
	struct rval_cache c1;
	str s;
	void *srevp[2];
	/* temporary storage space for a struct action.val[] working copy
	 (needed to transform RVE intro STRING before calling module
	   functions). [0] is not used (corresp. to the module export pointer),
	   [1] contains the number of params, and [2..] the param values.
	   We need [1], because some fixup function use it
	  (see fixup_get_param_count()).  */
	static action_u_t mod_f_params[MAX_ACTIONS];

	/* reset the value of error to E_UNSPEC so avoid unknowledgable
	   functions to return with error (status<0) and not setting it
	   leaving there previous error; cache the previous value though
	   for functions which want to process it */
	prev_ser_error=ser_error;
	ser_error=E_UNSPEC;

	/* hook for every executed action (in use by cfg debugger) */
	if(unlikely(sr_event_enabled(SREV_CFG_RUN_ACTION)))
	{
		srevp[0] = (void*)a;
		srevp[1] = (void*)msg;
		sr_event_exec(SREV_CFG_RUN_ACTION, (void*)srevp);
	}

	ret=E_BUG;
	switch ((unsigned char)a->type){
		case DROP_T:
				switch(a->val[0].type){
					case NUMBER_ST:
						ret=(int) a->val[0].u.number;
						break;
					case RVE_ST:
						rve=(struct rval_expr*)a->val[0].u.data;
						rval_expr_eval_int(h, msg, &ret, rve);
						break;
					case RETCODE_ST:
						ret=h->last_retcode;
						break;
					default:
						BUG("unexpected subtype %d in DROP_T\n",
								a->val[0].type);
						ret=0;
						goto error;
				}
				h->run_flags|=(unsigned int)a->val[1].u.number;
			break;
		case FORWARD_T:
#ifdef USE_TCP
		case FORWARD_TCP_T:
#endif
#ifdef USE_TLS
		case FORWARD_TLS_T:
#endif
#ifdef USE_SCTP
		case FORWARD_SCTP_T:
#endif
		case FORWARD_UDP_T:
			/* init dst */
			init_dest_info(&dst);
			if (a->type==FORWARD_UDP_T) dst.proto=PROTO_UDP;
#ifdef USE_TCP
			else if (a->type==FORWARD_TCP_T) dst.proto= PROTO_TCP;
#endif
#ifdef USE_TLS
			else if (a->type==FORWARD_TLS_T) dst.proto= PROTO_TLS;
#endif
#ifdef USE_SCTP
			else if (a->type==FORWARD_SCTP_T) dst.proto=PROTO_SCTP;
#endif
			else dst.proto=PROTO_NONE;
			if (a->val[0].type==URIHOST_ST){
				/*parse uri*/

				if (msg->dst_uri.len) {
					ret = parse_uri(msg->dst_uri.s, msg->dst_uri.len,
									&next_hop);
					u = &next_hop;
				} else {
					ret = parse_sip_msg_uri(msg);
					u = &msg->parsed_uri;
				}

				if (ret<0) {
					LM_ERR("forward: bad_uri dropping packet\n");
					goto error;
				}

				switch (a->val[1].type){
					case URIPORT_ST:
									port=u->port_no;
									break;
					case NUMBER_ST:
									port=a->val[1].u.number;
									break;
					default:
							LM_CRIT("bad forward 2nd param type (%d)\n", a->val[1].type);
							ret=E_UNSPEC;
							goto error_fwd_uri;
				}
				if (dst.proto == PROTO_NONE){ /* only if proto not set get it
											 from the uri */
					switch(u->proto){
						case PROTO_NONE:
							/*dst.proto=PROTO_UDP; */
							/* no proto, try to get it from the dns */
							break;
						case PROTO_UDP:
#ifdef USE_TCP
						case PROTO_TCP:
						case PROTO_WS:
#endif
#ifdef USE_TLS
						case PROTO_TLS:
						case PROTO_WSS:
#endif
#ifdef USE_SCTP
						case PROTO_SCTP:
#endif
							dst.proto=u->proto;
							break;
						default:
							LM_ERR("forward: bad uri transport %d\n", u->proto);
							ret=E_BAD_PROTO;
							goto error_fwd_uri;
					}
#ifdef USE_TLS
					if (u->type==SIPS_URI_T){
						if (u->proto==PROTO_UDP){
							LM_ERR("forward: secure uri incompatible with transport %d\n",
									u->proto);
							ret=E_BAD_PROTO;
							goto error_fwd_uri;
						} else if (u->proto!=PROTO_WSS)
							dst.proto=PROTO_TLS;
						else
							dst.proto=PROTO_WSS;
					}
#endif
				}

#ifdef HONOR_MADDR
				if (u->maddr_val.s && u->maddr_val.len)
					dst_host=&u->maddr_val;
				else
#endif
					dst_host=&u->host;
#ifdef USE_COMP
				dst.comp=u->comp;
#endif
				ret=forward_request(msg, dst_host, port, &dst);
				if (ret>=0){
					ret=1;
				}
			}else if ((a->val[0].type==PROXY_ST) && (a->val[1].type==NUMBER_ST)){
				if (dst.proto==PROTO_NONE)
					dst.proto=msg->rcv.proto;
				proxy2su(&dst.to,  (struct proxy_l*)a->val[0].u.data);
				ret=forward_request(msg, 0, 0, &dst);
				if (ret>=0){
					ret=1;
					proxy_mark((struct proxy_l*)a->val[0].u.data, ret);
				}else if (ser_error!=E_OK){
					proxy_mark((struct proxy_l*)a->val[0].u.data, ret);
				}
			}else{
				LM_CRIT("bad forward() types %d, %d\n",
						a->val[0].type, a->val[1].type);
				ret=E_BUG;
				goto error;
			}
			break;
		case LOG_T:
			if ((a->val[0].type!=NUMBER_ST)|(a->val[1].type!=STRING_ST)){
				LM_CRIT("bad log() types %d, %d\n",
						a->val[0].type, a->val[1].type);
				ret=E_BUG;
				goto error;
			}
			LOG_(DEFAULT_FACILITY, a->val[0].u.number, "<script>: ", "%s", 
				 a->val[1].u.string);
			ret=1;
			break;

		/* jku -- introduce a new branch */
		case APPEND_BRANCH_T:
			if (unlikely(a->val[0].type!=STR_ST)) {
				LM_CRIT("bad append_branch_t %d\n", a->val[0].type );
				ret=E_BUG;
				goto error;
			}
			getbflagsval(0, (flag_t*)&flags);
			ret=append_branch(msg, &a->val[0].u.str, &msg->dst_uri,
					  &msg->path_vec, a->val[1].u.number,
					  (flag_t)flags, msg->force_send_socket,
					  0, 0, 0, 0);
			/* if the uri is the ruri and q was also not changed, mark
			   ruri as consumed, to avoid having an identical branch */
			if ((a->val[0].u.str.s == 0 || a->val[0].u.str.len == 0) &&
					a->val[1].u.number == Q_UNSPECIFIED)
				ruri_mark_consumed();
			break;

		/* remove last branch */
		case REMOVE_BRANCH_T:
			if (a->val[0].type!=NUMBER_ST) {
				ret=drop_sip_branch(0) ? -1 : 1;
			} else {
				ret=drop_sip_branch(a->val[0].u.number) ? -1 : 1;
			}
			break;

		/* remove all branches */
		case CLEAR_BRANCHES_T:
			clear_branches();
			ret=1;
			break;

		/* jku begin: is_length_greater_than */
		case LEN_GT_T:
			if (a->val[0].type!=NUMBER_ST) {
				LM_CRIT("bad len_gt type %d\n", a->val[0].type );
				ret=E_BUG;
				goto error;
			}
			/* LM_DBG("message length %d, max %d\n",
				msg->len, a->val[0].u.number ); */
			ret = msg->len >= a->val[0].u.number ? 1 : -1;
			break;
		/* jku end: is_length_greater_than */

		/* jku - begin : flag processing */

		case SETFLAG_T:
			if (a->val[0].type!=NUMBER_ST) {
				LM_CRIT("bad setflag() type %d\n", a->val[0].type );
				ret=E_BUG;
				goto error;
			}
			if (!flag_in_range( a->val[0].u.number )) {
				ret=E_CFG;
				goto error;
			}
			setflag( msg, a->val[0].u.number );
			ret=1;
			break;

		case RESETFLAG_T:
			if (a->val[0].type!=NUMBER_ST) {
				LM_CRIT("bad resetflag() type %d\n", a->val[0].type );
				ret=E_BUG;
				goto error;
			}
			if (!flag_in_range( a->val[0].u.number )) {
				ret=E_CFG;
				goto error;
			}
			resetflag( msg, a->val[0].u.number );
			ret=1;
			break;

		case ISFLAGSET_T:
			if (a->val[0].type!=NUMBER_ST) {
				LM_CRIT("bad isflagset() type %d\n", a->val[0].type );
				ret=E_BUG;
				goto error;
			}
			if (!flag_in_range( a->val[0].u.number )) {
				ret=E_CFG;
				goto error;
			}
			ret=isflagset( msg, a->val[0].u.number );
			break;
		/* jku - end : flag processing */

		case AVPFLAG_OPER_T:
			ret = 0;
			if ((a->val[0].u.attr->type & AVP_INDEX_ALL) == AVP_INDEX_ALL ||
					(a->val[0].u.attr->type & AVP_NAME_RE)!=0) {
				for (avp=search_first_avp(a->val[0].u.attr->type,
							a->val[0].u.attr->name, NULL, &st);
						avp;
						avp = search_next_avp(&st, NULL)) {
					switch (a->val[2].u.number) {
						/* oper: 0..reset, 1..set, -1..no change */
						case 0:
							avp->flags &= ~(avp_flags_t)a->val[1].u.number;
							break;
						case 1:
							avp->flags |= (avp_flags_t)a->val[1].u.number;
							break;
						default:;
					}
					ret = ret ||
						((avp->flags & (avp_flags_t)a->val[1].u.number) != 0);
				}
			} else {
				avp = search_avp_by_index(a->val[0].u.attr->type,
											a->val[0].u.attr->name, NULL,
											a->val[0].u.attr->index);
				if (avp) {
					switch (a->val[2].u.number) {
						/* oper: 0..reset, 1..set, -1..no change */
						case 0:
							avp->flags &= ~(avp_flags_t)a->val[1].u.number;
							break;
						case 1:
							avp->flags |= (avp_flags_t)a->val[1].u.number;
							break;
						default:;
					}
					ret = (avp->flags & (avp_flags_t)a->val[1].u.number) != 0;
				}
			}
			if (ret==0)
				ret = -1;
			break;
		case ERROR_T:
			if ((a->val[0].type!=STRING_ST)|(a->val[1].type!=STRING_ST)){
				LM_CRIT("bad error() types %d, %d\n", a->val[0].type, a->val[1].type);
				ret=E_BUG;
				goto error;
			}
			LM_NOTICE("error(\"%s\", \"%s\") "
					"not implemented yet\n", a->val[0].u.string, a->val[1].u.string);
			ret=1;
			break;
		case ROUTE_T:
			if (likely(a->val[0].type == NUMBER_ST))
				i = a->val[0].u.number;
			else if (a->val[0].type == RVE_ST) {
				rv = rval_expr_eval(h, msg, a->val[0].u.data);
				rval_cache_init(&c1);
				if (unlikely(rv == 0 ||
						rval_get_tmp_str(h, msg, &s, rv, 0, &c1) < 0)) {
					rval_destroy(rv);
					rval_cache_clean(&c1);
					ERR("failed to convert RVE to string\n");
					ret = E_UNSPEC;
					goto error;
				}
				i = route_lookup(&main_rt, s.s);
				if (unlikely(i < 0)) {
					ERR("route \"%s\" not found at %s:%d\n",
							s.s, (a->cfile)?a->cfile:"line", a->cline);
					rval_destroy(rv);
					rval_cache_clean(&c1);
					s.s = 0;
					ret = E_SCRIPT;
					goto error;
				}
				rval_destroy(rv);
				rval_cache_clean(&c1);
				s.s = 0;
			} else {
				LM_CRIT("bad route() type %d\n", a->val[0].type);
				ret=E_BUG;
				goto error;
			}
			if (unlikely((i>=main_rt.idx)||(i<0))){
				LM_ERR("invalid routing table number in"
							"route(%lu) at %s:%d\n", a->val[0].u.number,
							(a->cfile)?a->cfile:"line", a->cline);
				ret=E_CFG;
				goto error;
			}
			/*ret=((ret=run_actions(rlist[a->val[0].u.number],msg))<0)?ret:1;*/
			ret=run_actions(h, main_rt.rlist[i], msg);
			h->last_retcode=ret;
			_last_returned_code = h->last_retcode;
			h->run_flags&=~(RETURN_R_F|BREAK_R_F); /* absorb return & break */
			break;
		case EXEC_T:
			if (a->val[0].type!=STRING_ST){
				LM_CRIT("bad exec() type %d\n", a->val[0].type);
				ret=E_BUG;
				goto error;
			}
			LM_NOTICE("exec(\"%s\") not fully implemented,"
						" using dumb version...\n", a->val[0].u.string);
			ret=system(a->val[0].u.string);
			if (ret!=0){
				LM_NOTICE("exec() returned %d\n", ret);
			}
			ret=1;
			break;
		case REVERT_URI_T:
			if (msg->new_uri.s) {
				pkg_free(msg->new_uri.s);
				msg->new_uri.len=0;
				msg->new_uri.s=0;
				msg->parsed_uri_ok=0; /* invalidate current parsed uri*/
				ruri_mark_new(); /* available for forking */
			};
			ret=1;
			break;
		case SET_HOST_T:
		case SET_HOSTPORT_T:
		case SET_HOSTPORTTRANS_T:
		case SET_HOSTALL_T:
		case SET_USER_T:
		case SET_USERPASS_T:
		case SET_PORT_T:
		case SET_URI_T:
		case PREFIX_T:
		case STRIP_T:
		case STRIP_TAIL_T:
		case SET_USERPHONE_T:
				user=0;
				if (a->type==STRIP_T || a->type==STRIP_TAIL_T) {
					if (a->val[0].type!=NUMBER_ST) {
						LM_CRIT("bad set*() type %d\n", a->val[0].type);
						ret=E_BUG;
						goto error;
					}
				} else if (a->type!=SET_USERPHONE_T) {
					if (a->val[0].type!=STRING_ST) {
						LM_CRIT("bad set*() type %d\n", a->val[0].type);
						ret=E_BUG;
						goto error;
					}
				}
				if (a->type==SET_URI_T){
					if (msg->new_uri.s) {
							pkg_free(msg->new_uri.s);
							msg->new_uri.len=0;
					}
					msg->parsed_uri_ok=0;
					len=strlen(a->val[0].u.string);
					msg->new_uri.s=pkg_malloc(len+1);
					if (msg->new_uri.s==0){
						LM_ERR("memory allocation failure\n");
						ret=E_OUT_OF_MEM;
						goto error;
					}
					memcpy(msg->new_uri.s, a->val[0].u.string, len);
					msg->new_uri.s[len]=0;
					msg->new_uri.len=len;
					ruri_mark_new(); /* available for forking */

					ret=1;
					break;
				}
				if (msg->parsed_uri_ok==0) {
					if (msg->new_uri.s) {
						tmp=msg->new_uri.s;
						len=msg->new_uri.len;
					}else{
						tmp=msg->first_line.u.request.uri.s;
						len=msg->first_line.u.request.uri.len;
					}
					if (parse_uri(tmp, len, &uri)<0){
						LM_ERR("bad uri <%s>, dropping packet\n", tmp);
						ret=E_UNSPEC;
						goto error;
					}
				} else {
					uri=msg->parsed_uri;
				}

				/* skip SET_USERPHONE_T action if the URI is already
				 * a tel: or tels: URI, or contains the user=phone param */
				if ((a->type==SET_USERPHONE_T) 
					&& ((uri.type==TEL_URI_T) || (uri.type==TELS_URI_T)
						|| ((uri.user_param_val.len==5) && (memcmp(uri.user_param_val.s, "phone", 5)==0)))
				) {
					ret=1;
					break;
				}
				/* SET_PORT_T does not work with tel: URIs */
				if ((a->type==SET_PORT_T)
					&& ((uri.type==TEL_URI_T) || (uri.type==TELS_URI_T))
					&& ((uri.flags & URI_SIP_USER_PHONE)==0)
				) {
					LM_ERR("port number of a tel: URI cannot be set\n");
					ret=E_UNSPEC;
					goto error;
				}

				new_uri=pkg_malloc(MAX_URI_SIZE);
				if (new_uri==0){
					LM_ERR("memory allocation failure\n");
					ret=E_OUT_OF_MEM;
					goto error;
				}
				end=new_uri+MAX_URI_SIZE;
				crt=new_uri;
				/* begin copying */
				/* Preserve the URI scheme unless the host part needs
				 * to be rewritten, and the shceme is tel: or tels: */
				switch (uri.type) {
				case SIP_URI_T:
					len=s_sip.len;
					tmp=s_sip.s;
					break;

				case SIPS_URI_T:
					len=s_sips.len;
					tmp=s_sips.s;
					break;

				case TEL_URI_T:
					if ((uri.flags & URI_SIP_USER_PHONE)
						|| (a->type==SET_HOST_T)
						|| (a->type==SET_HOSTPORT_T)
						|| (a->type==SET_HOSTPORTTRANS_T)
					) {
						len=s_sip.len;
						tmp=s_sip.s;
						break;
					}
					len=s_tel.len;
					tmp=s_tel.s;
					break;

				case TELS_URI_T:
					if ((uri.flags & URI_SIP_USER_PHONE)
						|| (a->type==SET_HOST_T)
						|| (a->type==SET_HOSTPORT_T)
						|| (a->type==SET_HOSTPORTTRANS_T)
					) {
						len=s_sips.len;
						tmp=s_sips.s;
						break;
					}
					len=s_tels.len;
					tmp=s_tels.s;
					break;

				default:
					LM_ERR("Unsupported URI scheme (%d), reverted to sip:\n",
						uri.type);
					len=s_sip.len;
					tmp=s_sip.s;
				}
				if(crt+len+1 /* colon */ >end) goto error_uri;
				memcpy(crt,tmp,len);crt+=len;
				*crt=':'; crt++;

				/* user */

				/* prefix (-jiri) */
				if (a->type==PREFIX_T) {
					tmp=a->val[0].u.string;
					len=strlen(tmp); if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
					/* whatever we had before, with prefix we have username
					   now */
					user=1;
				}

				if ((a->type==SET_USER_T)||(a->type==SET_USERPASS_T)) {
					tmp=a->val[0].u.string;
					len=strlen(tmp);
				} else if (a->type==STRIP_T) {
					if (a->val[0].u.number>uri.user.len) {
						LM_WARN("too long strip asked; deleting username: %lu of <%.*s>\n",
									a->val[0].u.number, uri.user.len, uri.user.s );
						len=0;
					} else if (a->val[0].u.number==uri.user.len) {
						len=0;
					} else {
						tmp=uri.user.s + a->val[0].u.number;
						len=uri.user.len - a->val[0].u.number;
					}
				} else if (a->type==STRIP_TAIL_T) {
					if (a->val[0].u.number>uri.user.len) {
						LM_WARN("too long strip_tail asked; "
									" deleting username: %lu of <%.*s>\n",
									a->val[0].u.number, uri.user.len, uri.user.s );
						len=0;
					} else if (a->val[0].u.number==uri.user.len) {
						len=0;
					} else {
						tmp=uri.user.s;
						len=uri.user.len - a->val[0].u.number;
					}
				} else {
					tmp=uri.user.s;
					len=uri.user.len;
				}

				if (len){
					if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
					user=1; /* we have an user field so mark it */
				}

				if (a->type==SET_USERPASS_T) tmp=0;
				else tmp=uri.passwd.s;
				/* passwd - keep it only if user is set */
				if (user && tmp){
					len=uri.passwd.len; if(crt+len+1>end) goto error_uri;
					*crt=':'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* tel: URI parameters */
				if ((uri.type==TEL_URI_T)
					|| (uri.type==TELS_URI_T)
				) {
					tmp=uri.params.s;
					if (tmp){
						len=uri.params.len; if(crt+len+1>end) goto error_uri;
						*crt=';'; crt++;
						memcpy(crt,tmp,len);crt+=len;
					}
				}
				/* host */
				if ((a->type==SET_HOST_T)
						|| (a->type==SET_HOSTPORT_T)
						|| (a->type==SET_HOSTALL_T)
						|| (a->type==SET_HOSTPORTTRANS_T)
				) {
					tmp=a->val[0].u.string;
					if (tmp) len = strlen(tmp);
					else len=0;
				} else if ((uri.type==SIP_URI_T)
					|| (uri.type==SIPS_URI_T)
					|| (uri.flags & URI_SIP_USER_PHONE)
				) {
					tmp=uri.host.s;
					len=uri.host.len;
				} else {
					tmp=0;
				}
				if (tmp){
					if (user) { /* add @ */
						if(crt+1>end) goto error_uri;
						*crt='@'; crt++;
					}
					if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				if(a->type==SET_HOSTALL_T)
					goto done_seturi;
				/* port */
				if ((a->type==SET_HOSTPORT_T)
						|| (a->type==SET_HOSTPORTTRANS_T))
					tmp=0;
				else if (a->type==SET_PORT_T) {
					tmp=a->val[0].u.string;
					if (tmp) {
						len = strlen(tmp);
						if(len==0) tmp = 0;
					} else len = 0;
				} else {
					tmp=uri.port.s;
					len = uri.port.len;
				}
				if (tmp){
					if(crt+len+1>end) goto error_uri;
					*crt=':'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* params */
				if ((a->type==SET_HOSTPORTTRANS_T)
					&& uri.sip_params.s
					&& uri.transport.s
				) {
					/* bypass the transport parameter */
					if (uri.sip_params.s < uri.transport.s) {
						/* there are parameters before transport */
						len = uri.transport.s - uri.sip_params.s - 1;
							/* ignore the ';' at the end */
						if (crt+len+1>end) goto error_uri;
						*crt=';'; crt++;
						memcpy(crt,uri.sip_params.s,len);crt+=len;
					}
					len = (uri.sip_params.s + uri.sip_params.len) -
						(uri.transport.s + uri.transport.len);
					if (len > 0) {
						/* there are parameters after transport */
						if (crt+len>end) goto error_uri;
						tmp = uri.transport.s + uri.transport.len;
						memcpy(crt,tmp,len);crt+=len;
					}
				} else {
					tmp=uri.sip_params.s;
					if (tmp){
						len=uri.sip_params.len; if(crt+len+1>end) goto error_uri;
						*crt=';'; crt++;
						memcpy(crt,tmp,len);crt+=len;
					}
				}
				/* Add the user=phone param if a tel: or tels:
				 * URI was converted to sip: or sips:.
				 * (host part of a tel/tels URI was set.)
				 * Or in case of sip: URI and SET_USERPHONE_T action */
				if (((((uri.type==TEL_URI_T) || (uri.type==TELS_URI_T))
					&& ((uri.flags & URI_SIP_USER_PHONE)==0))
					&& ((a->type==SET_HOST_T)
						|| (a->type==SET_HOSTPORT_T)
						|| (a->type==SET_HOSTPORTTRANS_T)))
					|| (a->type==SET_USERPHONE_T)
				) {
					tmp=";user=phone";
					len=strlen(tmp);
					if(crt+len>end) goto error_uri;
					memcpy(crt,tmp,len);crt+=len;
				}
				/* headers */
				tmp=uri.headers.s;
				if (tmp){
					len=uri.headers.len; if(crt+len+1>end) goto error_uri;
					*crt='?'; crt++;
					memcpy(crt,tmp,len);crt+=len;
				}
	done_seturi:
				*crt=0; /* null terminate the thing */
				/* copy it to the msg */
				if (msg->new_uri.s) pkg_free(msg->new_uri.s);
				msg->new_uri.s=new_uri;
				msg->new_uri.len=crt-new_uri;
				msg->parsed_uri_ok=0;
				ruri_mark_new(); /* available for forking */
				ret=1;
				break;
		case IF_T:
					rve=(struct rval_expr*)a->val[0].u.data;
					if (unlikely(rval_expr_eval_int(h, msg, &v, rve) != 0)){
						ERR("if expression evaluation failed (%d,%d-%d,%d)\n",
								rve->fpos.s_line, rve->fpos.s_col,
								rve->fpos.e_line, rve->fpos.e_col);
						v=0; /* false */
					}
					if (unlikely(h->run_flags & EXIT_R_F)){
						ret=0;
						break;
					}
					h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return &
															    break in expr*/
					ret=1;  /*default is continue */
					if (v>0) {
						if ((a->val[1].type==ACTIONS_ST)&&a->val[1].u.data){
							ret=run_actions(h,
										(struct action*)a->val[1].u.data, msg);
						}
					}else if ((a->val[2].type==ACTIONS_ST)&&a->val[2].u.data){
							ret=run_actions(h,
										(struct action*)a->val[2].u.data, msg);
					}
			break;
		case MODULE0_T:
			MODF_CALL(cmd_function, h, msg, a->val, 0, 0);
			break;
		/* instead of using the parameter number, we use different names
		 * for calls to functions with 3, 4, 5, 6 or variable number of
		 * parameters due to performance reasons */
		case MODULE1_T:
			MODF_CALL(cmd_function, h, msg, a->val,
										(char*)a->val[2].u.data,
										0
					);
			break;
		case MODULE2_T:
			MODF_CALL(cmd_function, h, msg, a->val,
										(char*)a->val[2].u.data,
										(char*)a->val[3].u.data
					);
			break;
		case MODULE3_T:
			MODF_CALL(cmd_function3, h, msg, a->val,
										(char*)a->val[2].u.data,
										(char*)a->val[3].u.data,
										(char*)a->val[4].u.data
					);
			break;
		case MODULE4_T:
			MODF_CALL(cmd_function4, h, msg, a->val,
										(char*)a->val[2].u.data,
										(char*)a->val[3].u.data,
										(char*)a->val[4].u.data,
										(char*)a->val[5].u.data
					);
			break;
		case MODULE5_T:
			MODF_CALL(cmd_function5, h, msg, a->val,
										(char*)a->val[2].u.data,
										(char*)a->val[3].u.data,
										(char*)a->val[4].u.data,
										(char*)a->val[5].u.data,
										(char*)a->val[6].u.data
					);
			break;
		case MODULE6_T:
			MODF_CALL(cmd_function6, h, msg, a->val,
										(char*)a->val[2].u.data,
										(char*)a->val[3].u.data,
										(char*)a->val[4].u.data,
										(char*)a->val[5].u.data,
										(char*)a->val[6].u.data,
										(char*)a->val[7].u.data
					);
			break;
		case MODULEX_T:
			MODF_CALL(cmd_function_var, h, msg, a->val,
							a->val[1].u.number, &a->val[2]);
			break;
		case MODULE1_RVE_T:
			MODF_RVE_CALL(cmd_function, h, msg, a->val, mod_f_params,
											(char*)mod_f_params[2].u.data,
											0
					);
			break;
		case MODULE2_RVE_T:
			MODF_RVE_CALL(cmd_function, h, msg, a->val, mod_f_params,
											(char*)mod_f_params[2].u.data,
											(char*)mod_f_params[3].u.data
					);
			break;
		case MODULE3_RVE_T:
			MODF_RVE_CALL(cmd_function3, h, msg, a->val, mod_f_params,
											(char*)mod_f_params[2].u.data,
											(char*)mod_f_params[3].u.data,
											(char*)mod_f_params[4].u.data
					);
			break;
		case MODULE4_RVE_T:
			MODF_RVE_CALL(cmd_function4, h, msg, a->val, mod_f_params,
											(char*)mod_f_params[2].u.data,
											(char*)mod_f_params[3].u.data,
											(char*)mod_f_params[4].u.data,
											(char*)mod_f_params[5].u.data
					);
			break;
		case MODULE5_RVE_T:
			MODF_RVE_CALL(cmd_function5, h, msg, a->val, mod_f_params,
											(char*)mod_f_params[2].u.data,
											(char*)mod_f_params[3].u.data,
											(char*)mod_f_params[4].u.data,
											(char*)mod_f_params[5].u.data,
											(char*)mod_f_params[6].u.data
					);
			break;
		case MODULE6_RVE_T:
			MODF_RVE_CALL(cmd_function6, h, msg, a->val, mod_f_params,
											(char*)mod_f_params[2].u.data,
											(char*)mod_f_params[3].u.data,
											(char*)mod_f_params[4].u.data,
											(char*)mod_f_params[5].u.data,
											(char*)mod_f_params[6].u.data,
											(char*)mod_f_params[7].u.data
					);
			break;
		case MODULEX_RVE_T:
			MODF_RVE_CALL(cmd_function_var, h, msg, a->val, mod_f_params,
							a->val[1].u.number, &mod_f_params[2]);
			break;
		case EVAL_T:
			/* only eval the expression to account for possible
			   side-effect */
			rval_expr_eval_int(h, msg, &v,
					(struct rval_expr*)a->val[0].u.data);
			if (h->run_flags & EXIT_R_F){
				ret=0;
				break;
			}
			h->run_flags &= ~RETURN_R_F|BREAK_R_F; /* catch return & break in
													  expr */
			ret=1; /* default is continue */
			break;
		case SWITCH_COND_T:
			sct=(struct switch_cond_table*)a->val[1].u.data;
			if (unlikely( rval_expr_eval_int(h, msg, &v,
									(struct rval_expr*)a->val[0].u.data) <0)){
				/* handle error in expression => use default */
				ret=-1;
				goto sw_cond_def;
			}
			if (h->run_flags & EXIT_R_F){
				ret=0;
				break;
			}
			h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return & break
													    in expr */
			ret=1; /* default is continue */
			for(i=0; i<sct->n; i++)
				if (sct->cond[i]==v){
					if (likely(sct->jump[i])){
						ret=run_actions(h, sct->jump[i], msg);
						h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
													   returns passthrough */
					}
					goto skip;
				}
sw_cond_def:
			if (sct->def){
				ret=run_actions(h, sct->def, msg);
				h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
											   returns passthrough */
			}
			break;
		case SWITCH_JT_T:
			sjt=(struct switch_jmp_table*)a->val[1].u.data;
			if (unlikely( rval_expr_eval_int(h, msg, &v,
									(struct rval_expr*)a->val[0].u.data) <0)){
				/* handle error in expression => use default */
				ret=-1;
				goto sw_jt_def;
			}
			if (h->run_flags & EXIT_R_F){
				ret=0;
				break;
			}
			h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return & break
													    in expr */
			ret=1; /* default is continue */
			if (likely(v >= sjt->first && v <= sjt->last)){
				if (likely(sjt->tbl[v - sjt->first])){
					ret=run_actions(h, sjt->tbl[v - sjt->first], msg);
					h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
												   returns passthrough */
				}
				break; 
			}else{
				for(i=0; i<sjt->rest.n; i++)
					if (sjt->rest.cond[i]==v){
						if (likely(sjt->rest.jump[i])){
							ret=run_actions(h, sjt->rest.jump[i], msg);
							h->run_flags &= ~BREAK_R_F; /* catch breaks, but 
														   let returns pass */
						}
						goto skip;
					}
			}
			/* not found => try default */
sw_jt_def:
			if (sjt->rest.def){
				ret=run_actions(h, sjt->rest.def, msg);
				h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
											   returns passthrough */
			}
			break;
		case BLOCK_T:
			if (likely(a->val[0].u.data)){
				ret=run_actions(h, (struct action*)a->val[0].u.data, msg);
				h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
											   returns passthrough */
			}
			break;
		case MATCH_COND_T:
			mct=(struct match_cond_table*)a->val[1].u.data;
			rval_cache_init(&c1);
			rv=0;
			rv1=0;
			ret=rval_expr_eval_rvint(h, msg, &rv, &v, 
									(struct rval_expr*)a->val[0].u.data, &c1);
									
			if (unlikely( ret<0)){
				/* handle error in expression => use default */
				ret=-1;
				goto match_cond_def;
			}
			if (h->run_flags & EXIT_R_F){
				ret=0;
				break;
			}
			h->run_flags &= ~(RETURN_R_F|BREAK_R_F); /* catch return & break
													    in expr */
			if (likely(rv)){
				rv1=rval_convert(h, msg, RV_STR, rv, &c1);
				if (unlikely(rv1==0)){
					ret=-1;
					goto match_cond_def;
				}
				s=rv1->v.s;
			}else{
				/* int result in v */
				rval_cache_clean(&c1);
				s.s=sint2str(v, &s.len);
			}
			ret=1; /* default is continue */
			for(i=0; i<mct->n; i++)
				if (( mct->match[i].type==MATCH_STR &&
						mct->match[i].l.s.len==s.len &&
						memcmp(mct->match[i].l.s.s, s.s, s.len) == 0 ) ||
					 ( mct->match[i].type==MATCH_RE &&
					  regexec(mct->match[i].l.regex, s.s, 0, 0, 0) == 0)
					){
					if (likely(mct->jump[i])){
						/* make sure we cleanup first, in case run_actions()
						   exits the script directly via longjmp() */
						if (rv1){
							rval_destroy(rv1);
							rval_destroy(rv);
							rval_cache_clean(&c1);
						}else if (rv){
							rval_destroy(rv);
							rval_cache_clean(&c1);
						}
						ret=run_actions(h, mct->jump[i], msg);
						h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
													   returns passthrough */
						goto skip;
					}
					goto match_cleanup;
				}
match_cond_def:
			if (mct->def){
				/* make sure we cleanup first, in case run_actions()
				   exits the script directly via longjmp() */
				if (rv1){
					rval_destroy(rv1);
					rval_destroy(rv);
					rval_cache_clean(&c1);
				}else if (rv){
					rval_destroy(rv);
					rval_cache_clean(&c1);
				}
				ret=run_actions(h, mct->def, msg);
				h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
											   returns passthrough */
				break;
			}
match_cleanup:
			if (rv1){
				rval_destroy(rv1);
				rval_destroy(rv);
				rval_cache_clean(&c1);
			}else if (rv){
				rval_destroy(rv);
				rval_cache_clean(&c1);
			}
			break;
		case WHILE_T:
			i=0;
			flags=0;
			rve=(struct rval_expr*)a->val[0].u.data;
			ret=1;
			while(!(flags & (BREAK_R_F|RETURN_R_F|EXIT_R_F)) &&
					(rval_expr_eval_int(h, msg, &v, rve) == 0) && v){
				if (cfg_get(core, core_cfg, max_while_loops) > 0)
					i++;

				if (unlikely(i > cfg_get(core, core_cfg, max_while_loops))){
					LM_ERR("runaway while (%d, %d): more then %d loops\n", 
								rve->fpos.s_line, rve->fpos.s_col,
								cfg_get(core, core_cfg, max_while_loops));
					ret=-1;
					goto error;
				}
				if (likely(a->val[1].u.data)){
					ret=run_actions(h, (struct action*)a->val[1].u.data, msg);
					flags|=h->run_flags;
					h->run_flags &= ~BREAK_R_F; /* catch breaks, but let
												   returns pass-through */
				}
			}
			break;
		case FORCE_RPORT_T:
			msg->msg_flags|=FL_FORCE_RPORT;
			ret=1; /* continue processing */
			break;
		case ADD_LOCAL_RPORT_T:
			msg->msg_flags|=FL_ADD_LOCAL_RPORT;
			ret=1; /* continue processing */
			break;
		case UDP_MTU_TRY_PROTO_T:
			msg->msg_flags|= (unsigned int)a->val[0].u.number & FL_MTU_FB_MASK;
			ret=1; /* continue processing */
			break;
		case SET_ADV_ADDR_T:
			if (a->val[0].type!=STR_ST){
				LM_CRIT("bad set_advertised_address() type %d\n", a->val[0].type);
				ret=E_BUG;
				goto error;
			}
			msg->set_global_address=*((str*)a->val[0].u.data);
			ret=1; /* continue processing */
			break;
		case SET_ADV_PORT_T:
			if (a->val[0].type!=STR_ST){
				LM_CRIT("bad set_advertised_port() type %d\n", a->val[0].type);
				ret=E_BUG;
				goto error;
			}
			msg->set_global_port=*((str*)a->val[0].u.data);
			ret=1; /* continue processing */
			break;
#ifdef USE_TCP
		case FORCE_TCP_ALIAS_T:
			if ( msg->rcv.proto==PROTO_TCP
#ifdef USE_TLS
					|| msg->rcv.proto==PROTO_TLS
#endif
			   ){

				if (a->val[0].type==NOSUBTYPE)	port=msg->via1->port;
				else if (a->val[0].type==NUMBER_ST) port=(int)a->val[0].u.number;
				else{
					LM_CRIT("bad force_tcp_alias"
							" port type %d\n", a->val[0].type);
					ret=E_BUG;
					goto error;
				}

				if (tcpconn_add_alias(msg->rcv.proto_reserved1, port,
									msg->rcv.proto)!=0){
					LM_ERR("receive_msg: tcp alias failed\n");
					ret=E_UNSPEC;
					goto error;
				}
			}
#endif
			ret=1; /* continue processing */
			break;
		case FORCE_SEND_SOCKET_T:
			if (a->val[0].type!=SOCKETINFO_ST){
				LM_CRIT("bad force_send_socket argument"
						" type: %d\n", a->val[0].type);
				ret=E_BUG;
				goto error;
			}
			set_force_socket(msg, (struct socket_info*)a->val[0].u.data);
			ret=1; /* continue processing */
			break;

		case ADD_T:
		case ASSIGN_T:
			v=lval_assign(h, msg, (struct lvalue*)a->val[0].u.data,
								  (struct rval_expr*)a->val[1].u.data);
			if (likely(v>=0))
				ret = 1;
			else if (unlikely (v == EXPR_DROP)) /* hack to quit on DROP*/
				ret=0;
			else
				ret=v;
			break;
		case SET_FWD_NO_CONNECT_T:
			msg->fwd_send_flags.f|= SND_F_FORCE_CON_REUSE;
			ret=1; /* continue processing */
			break;
		case SET_RPL_NO_CONNECT_T:
			msg->rpl_send_flags.f|= SND_F_FORCE_CON_REUSE;
			ret=1; /* continue processing */
			break;
		case SET_FWD_CLOSE_T:
			msg->fwd_send_flags.f|= SND_F_CON_CLOSE;
			ret=1; /* continue processing */
			break;
		case SET_RPL_CLOSE_T:
			msg->rpl_send_flags.f|= SND_F_CON_CLOSE;
			ret=1; /* continue processing */
			break;
		case CFG_SELECT_T:
			if (a->val[0].type != CFG_GROUP_ST) {
				BUG("unsupported parameter in CFG_SELECT_T: %d\n",
						a->val[0].type);
				ret=-1;
				goto error;
			}
			switch(a->val[1].type) {
				case NUMBER_ST:
					v=(int)a->val[1].u.number;
					break;
				case RVE_ST:
					if (rval_expr_eval_int(h, msg, &v, (struct rval_expr*)a->val[1].u.data) < 0) {
						ret=-1;
						goto error;
					}
					break;
				default:
					BUG("unsupported group id type in CFG_SELECT_T: %d\n",
							a->val[1].type);
					ret=-1;
					goto error;
			}
			ret=(cfg_select((cfg_group_t*)a->val[0].u.data, v) == 0) ? 1 : -1;
			break;
		case CFG_RESET_T:
			if (a->val[0].type != CFG_GROUP_ST) {
				BUG("unsupported parameter in CFG_RESET_T: %d\n",
						a->val[0].type);
				ret=-1;
				goto error;
			}
			ret=(cfg_reset((cfg_group_t*)a->val[0].u.data) == 0) ? 1 : -1;
			break;
/*
		default:
			LM_CRIT("unknown type %d\n", a->type);
*/
	}
skip:
	return ret;

error_uri:
	LM_ERR("set*: uri too long\n");
	if (new_uri) pkg_free(new_uri);
	LM_ERR("run action error at: %s:%d\n", (a->cfile)?a->cfile:"", a->cline);
	return E_UNSPEC;
error_fwd_uri:
	/*free_uri(&uri); -- not needed anymore, using msg->parsed_uri*/
error:
	LM_ERR("run action error at: %s:%d\n", (a->cfile)?a->cfile:"", a->cline);
	return ret;
}



/* returns: 0, or 1 on success, <0 on error */
/* (0 if drop or break encountered, 1 if not ) */
int run_actions(struct run_act_ctx* h, struct action* a, struct sip_msg* msg)
{
	struct action* t;
	int ret;
	struct sr_module *mod;
	unsigned int ms = 0;

	ret=E_UNSPEC;
	h->rec_lev++;
	if (unlikely(h->rec_lev>max_recursive_level)){
		LM_ERR("too many recursive routing table lookups (%d) giving up!\n", h->rec_lev);
		ret=E_UNSPEC;
		goto error;
	}
	if (unlikely(h->rec_lev==1)){
		h->run_flags=0;
		h->last_retcode=0;
		_last_returned_code = h->last_retcode;
#ifdef USE_LONGJMP
		if (unlikely(setjmp(h->jmp_env))){
			h->rec_lev=0;
			ret=h->last_retcode;
			goto end;
		}
#endif
	}

	if (unlikely(a==0)){
		LM_DBG("null action list (rec_level=%d)\n", h->rec_lev);
		ret=1;
	}

	for (t=a; t!=0; t=t->next){
		if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0))
			ms = TICKS_TO_MS(get_ticks_raw());
		_cfg_crt_action = t;
		ret=do_action(h, t, msg);
		_cfg_crt_action = 0;
		if(unlikely(cfg_get(core, core_cfg, latency_limit_action)>0)) {
			ms = TICKS_TO_MS(get_ticks_raw()) - ms;
			if(ms >= cfg_get(core, core_cfg, latency_limit_action)) {
				LOG(cfg_get(core, core_cfg, latency_log),
						"alert - action [%s (%d)]"
						" cfg [%s:%d] took too long [%u ms]\n",
						is_mod_func(t) ?
							((cmd_export_common_t*)(t->val[0].u.data))->name
							: "corefunc",
						t->type, (t->cfile)?t->cfile:"", t->cline, ms);
			}
		}
		/* break, return or drop/exit stop execution of the current
		   block */
		if (unlikely(h->run_flags & (BREAK_R_F|RETURN_R_F|EXIT_R_F))){
			if (unlikely(h->run_flags & EXIT_R_F)) {
				h->last_retcode=ret;
				_last_returned_code = h->last_retcode;
#ifdef USE_LONGJMP
				longjmp(h->jmp_env, ret);
#endif
			}
			break;
		}
		/* ignore error returns */
	}

	h->rec_lev--;
end:
	/* process module onbreak handlers if present */
	if (unlikely(h->rec_lev==0 && ret==0 &&
					!(h->run_flags & IGNORE_ON_BREAK_R_F)))
		for (mod=modules;mod;mod=mod->next)
			if (unlikely(mod->exports.onbreak_f)) {
				mod->exports.onbreak_f( msg );
			}
	return ret;


error:
	h->rec_lev--;
	return ret;
}



#ifdef USE_LONGJMP
/** safe version of run_actions().
 * It always return (it doesn't longjmp on forced script end).
 * @returns 0, or 1 on success, <0 on error
 * (0 if drop or break encountered, 1 if not ) */
int run_actions_safe(struct run_act_ctx* h, struct action* a,
						struct sip_msg* msg)
{
	struct run_act_ctx ctx;
	int ret;
	int ign_on_break;
	
	/* start with a fresh action context */
	init_run_actions_ctx(&ctx);
	ctx.last_retcode = h->last_retcode;
	ign_on_break = h->run_flags & IGNORE_ON_BREAK_R_F;
	ctx.run_flags = h->run_flags | IGNORE_ON_BREAK_R_F;
	ret = run_actions(&ctx, a, msg);
	h->last_retcode = ctx.last_retcode;
	h->run_flags = (ctx.run_flags & ~IGNORE_ON_BREAK_R_F) | ign_on_break;
	return ret;
}
#endif /* USE_LONGJMP */



int run_top_route(struct action* a, sip_msg_t* msg, struct run_act_ctx *c)
{
	struct run_act_ctx ctx;
	struct run_act_ctx *p;
	int ret;
	flag_t sfbk;

	p = (c)?c:&ctx;
	sfbk = getsflags();
	setsflagsval(0);
	reset_static_buffer();
	init_run_actions_ctx(p);
	ret = run_actions(p, a, msg);
	setsflagsval(sfbk);
	return ret;
}
