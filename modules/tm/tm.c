/*
 * $Id$
 *
 * TM module
 *
 */

#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"

#include "sip_msg.h"
#include "h_table.h"
#include "t_funcs.h"
#include "t_hooks.h"
#include "tm_load.h"
#include "t_fork.h"
#include "ut.h"



inline static int w_t_check(struct sip_msg* msg, char* str, char* str2);
inline static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2);
inline static int w_t_release(struct sip_msg* msg, char* str, char* str2);
inline static int fixup_t_send_reply(void** param, int param_no);
inline static int w_t_unref( struct sip_msg* p_msg, char* foo, char* bar );
inline static int w_t_retransmit_reply(struct sip_msg* p_msg, char* foo, char* bar );
/*
inline static int w_t_add_transaction(struct sip_msg* p_msg, char* foo, char* bar );
*/

inline static int w_t_newtran(struct sip_msg* p_msg, char* foo, char* bar );

inline static int t_relay( struct sip_msg  *p_msg ,  char* , char* );
inline static int w_t_relay_to( struct sip_msg  *p_msg , char *proxy, char *);
static int t_relay_to( struct sip_msg  *p_msg , struct proxy_l *proxy );
/*
inline static int w_t_forward_ack(struct sip_msg* msg, char* proxy, char* );
*/
inline static int w_t_forward_nonack(struct sip_msg* msg, char* str, char* );
inline static int fixup_hostport2proxy(void** param, int param_no);

inline static int w_t_add_fork_ip(struct sip_msg* msg, char* proxy, char* );
inline static int w_t_add_fork_uri(struct sip_msg* msg, char* str, char* );
inline static int w_t_add_fork_on_no_rpl(struct sip_msg* msg,char* str,char* );
inline static int w_t_clear_forks(struct sip_msg* msg, char* , char* );

inline static int fixup_uri2fork(void** param, int param_no);

inline static void w_onbreak(struct sip_msg* msg) { t_unref(); }

static int mod_init(void);

#ifdef STATIC_TM
struct module_exports tm_exports = {
#else
struct module_exports exports= {
#endif
	"tm",
	/* -------- exported functions ----------- */
	(char*[]){			
/*		- obsoleted by atomic t_newtran 
				"t_add_transaction",
*/
				"t_newtran",
				"t_lookup_request",
				"t_send_reply",
				"t_retransmit_reply",
				"t_release",
				"t_unref",
				"t_relay_to",
				"t_relay",
				"t_forward_nonack",
/*
				"t_forward_ack",
*/
				"t_fork_to_ip",
				"t_fork_to_uri",
				"t_clear_forks",
				"t_fork_on_no_response",
				"register_tmcb",
				"load_tm"
			},
	(cmd_function[]){
/*
					w_t_add_transaction,
*/					w_t_newtran,
					w_t_check,
					w_t_send_reply,
					w_t_retransmit_reply,
					w_t_release,
					w_t_unref,
					w_t_relay_to,
					t_relay,
					w_t_forward_nonack,
/*
					w_t_forward_ack,
*/
					w_t_add_fork_ip,
					w_t_add_fork_uri,
					w_t_clear_forks,
					w_t_add_fork_on_no_rpl,
					(cmd_function) register_tmcb,
					(cmd_function) load_tm
					},
	(int[]){
				0, /* t_newtran */
				0, /* t_lookup_request */
				2, /* t_send_reply */
				0, /* t_retransmit_reply */
				0, /* t_release */
				0, /* t_unref */
				2, /* t_relay_to */
				0, /* t_relay */
				2, /* t_forward_nonack */
/* 				2,*/ /* t_forward_ack */

				2, /* t_fork_to_ip */
				1, /* t_fork_to_uri */
				0, /* t_clear_forks */
				1,  /* t_add_fork_on_no_response */
				2 /* register_tmcb */,
				1 /* load_tm */
			},
	(fixup_function[]){
				0,						/* t_newtran */
				0,						/* t_lookup_request */
				fixup_t_send_reply,		/* t_send_reply */
				0,						/* t_retransmit_reply */
				0,						/* t_release */
				0,						/* t_unref */
				fixup_hostport2proxy,	/* t_relay_to */
				0,						/* t_relay */
				fixup_hostport2proxy,	/* t_forward_nonack */
				/* fixup_hostport2proxy,	*/ /* t_forward_ack */
				fixup_hostport2proxy,	/* t_fork_to_ip */
				fixup_uri2fork,   		/* t_fork_to_uri */
				0,						/* t_clear_forks */
				fixup_uri2fork,		/* t_add_fork_on_no_response */
				0,						/* register_tmcb */
				0						/* load_tm */
	
		},
	15,

	/* ------------ exported variables ---------- */
	(char *[]) { /* Module parameter names */
		"fr_timer",
		"fr_inv_timer",
		"wt_timer",
		"delete_timer",
		"retr_timer1p1",
		"retr_timer1p2",
		"retr_timer1p3",
		"retr_timer2"
	},
	(modparam_t[]) { /* variable types */
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void *[]) { /* variable pointers */
		&(timer_id2timeout[FR_TIMER_LIST]),
		&(timer_id2timeout[FR_INV_TIMER_LIST]),
		&(timer_id2timeout[WT_TIMER_LIST]),
		&(timer_id2timeout[DELETE_LIST]),
		&(timer_id2timeout[RT_T1_TO_1]),
		&(timer_id2timeout[RT_T1_TO_2]),
		&(timer_id2timeout[RT_T1_TO_3]),
		&(timer_id2timeout[RT_T2])
	},
	8,      /* Number of module paramers */

	mod_init, /* module initialization function */
	(response_function) t_on_reply,
	(destroy_function) tm_shutdown,
	w_onbreak,
	0  /* per-child init function */
};



static int mod_init(void)
{

	DBG( "TM - initializing...\n");
	if (tm_startup()==-1) return -1;
	return 0;
}


/* (char *hostname, char *port_nr) ==> (struct proxy_l *, -)  */

inline static int fixup_hostport2proxy(void** param, int param_no)
{
	unsigned int port;
	char *host;
	int err;
	struct proxy_l *proxy;
	
	DBG("TM module: fixup_t_forward(%s, %d)\n", (char*)*param, param_no);
	if (param_no==1){
		DBG("TM module: fixup_t_forward: param 1.. do nothing, wait for #2\n");
		return 0;
	} else if (param_no==2) {

		host=(char *) (*(param-1)); 
		port=str2s(*param, strlen(*param), &err);
		if (err!=0) {
			LOG(L_ERR, "TM module:fixup_t_forward: bad port number <%s>\n",
				(char*)(*param));
			 return E_UNSPEC;
		}
		proxy=mk_proxy(host, port);
		if (proxy==0) {
			LOG(L_ERR, "ERROR: fixup_t_forwardv6: bad host name in URI <%s>\n",
				host );
			return E_BAD_ADDRESS;
		}
		/* success -- fix the first parameter to proxy now ! */
		free( *(param-1));
		*(param-1)=proxy;
		return 0;
	} else {
		LOG(L_ERR, "ERROR: fixup_t_forwardv6 called with parameter #<>{1,2}\n");
		return E_BUG;
	}
}


/* (char *code, char *reason_phrase)==>(int code, r_p as is) */


inline static int fixup_t_send_reply(void** param, int param_no)
{
	unsigned int code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "TM module:fixup_t_send_reply: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	/* second param => no conversion*/
	return 0;
}




inline static int w_t_check(struct sip_msg* msg, char* str, char* str2)
{
	return t_check( msg , 0 , 0 ) ? 1 : -1;
}



#ifdef _TOO_OLD
inline static int w_t_forward_ack(struct sip_msg* msg, char* proxy, char* _foo)
{
	if (t_check( msg , 0 , 0 )==-1) return -1;
	if (!T) {
		DBG("DEBUG: t_forward_ack: no transaction found \n");
		return -1;
	}
	return t_forward_ack(msg /*, ( struct proxy_l *) proxy */ );
}

#endif


inline static int w_t_forward_nonack(struct sip_msg* msg, char* proxy, char* _foo)
{
	if (t_check( msg , 0 , 0)==-1) return -1;
	if (!T) {
		DBG("DEBUG: t_forward_nonack: no transaction found\n");
		return -1;
	}
	return t_forward_nonack(msg, ( struct proxy_l *) proxy );
}



inline static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	if (t_check( msg , 0 , 0)==-1) return -1;
	if (!T) {
		LOG(L_ERR, "ERROR: t_send_reply: cannot send a t_reply to a message "
			"for which no T-state has been established\n");
		return -1;
	}
	return t_send_reply(msg, (unsigned int) str, str2, 0);
}




inline static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	if (t_check( msg  , 0 , 0 )==-1) return 1;
	if ( T && T!=T_UNDEFINED ) 
		return t_put_on_wait( T );
	return 1;
}




inline static int w_t_unref( struct sip_msg* p_msg, char* foo, char* bar )
{
	if (T==T_UNDEFINED || T==T_NULL)
		return -1;
    return t_unref( /* p_msg */ );
}




inline static int w_t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar)
{
	if (t_check( p_msg  , 0 , 0 )==-1) 
		return 1;
	if (T)
		return t_retransmit_reply( p_msg );
	else 
		return -1;
	return 1;
}




/* inline static int w_t_add_transaction( struct sip_msg* p_msg, 
	char* foo, char* bar ) {
*/
inline static int w_t_newtran( struct sip_msg* p_msg, char* foo, char* bar ) {
	if (t_check( p_msg , 0 , 0 )==-1) return -1;
	if (T) {
		LOG(L_ERR,"ERROR: t_add_transaction: won't add a retransmission\n");
		return -1;
	}
	return t_newtran( p_msg );
}




inline static int w_t_add_fork_ip(struct sip_msg* msg, char* proxy, char* _foo )
{
	struct proxy_l *p;
	union sockaddr_union to;

	p=(struct proxy_l *) proxy;
	hostent2su(&to, &p->host, p->addr_idx, (p->port)?htons(p->port):htons(SIP_PORT));
	return t_add_fork( to, 0,0,DEFAULT,0);
}




inline static int fixup_uri2fork (void** param, int param_no)
{
	struct fork* fork_pack;
	struct proxy_l *p;

	if (param_no==1)
	{
		fork_pack = (struct fork*)pkg_malloc(sizeof(struct fork));
		if (!fork_pack)
		{
			LOG(L_ERR, "TM module:fixup_t_add_fork_uri: \
				cannot allocate memory!\n");
			return E_UNSPEC;
		}
		fork_pack->uri.len = strlen(*param);
		fork_pack->uri.s = *param;

		p=uri2proxy( & fork_pack->uri );
		if (p==0) {
			pkg_free( fork_pack );
			return E_CFG;
		}
		hostent2su(&fork_pack->to, &p->host, p->addr_idx,
        	(p->port)?htons(p->port):htons(SIP_PORT));

		free_proxy(p); free(p);
		*param=(void*)fork_pack;
		return 0;
	} else {
		LOG(L_ERR, "Error: URI should not be followed by another parameter\n");
		/* second param => no conversion*/
		return E_BUG;
	}
}




inline static int w_t_add_fork_uri(struct sip_msg* msg, char* str, char* _foo)
{
	struct fork *fp;
	fp=(struct fork *) str;
	return t_add_fork( fp->to, fp->uri.s, fp->uri.len, DEFAULT, 0);
}

inline static int w_t_add_fork_on_no_rpl(struct sip_msg* msg, char* str, char* _foo )
{
	struct fork *fp;
	fp=(struct fork *) str;
	return t_add_fork(  fp->to, fp->uri.s, fp->uri.len, NO_RESPONSE, 0);
}

inline static int w_t_clear_forks(struct sip_msg* msg, char* _foo, char* _bar )
{
	return t_clear_forks();
}


inline static int w_t_relay_to( struct sip_msg  *p_msg , 
						 char *proxy, /* struct proxy_l *proxy expected */
						 char *_foo       /* nothing expected */ )
{
	return t_relay_to( p_msg, ( struct proxy_l *) proxy );
}

static int t_relay_to( struct sip_msg  *p_msg , struct proxy_l *proxy )
{
	int ret;
	int new_tran;
	char err_buffer[128];
	int sip_err;
	int reply_ret;

	ret=0;

	new_tran = t_newtran( p_msg );

	/* parsing error, memory alloc, whatever ... return -1;
	   note -- this is a difference to writing a real script --
	   we translate t_newtran 0 error to a negative value allowign
	   some error handling; a script breaks on 0
    */
	if (new_tran==0) {
		ret=E_UNSPEC;
		goto done;
	}

	/* nothing new -- it is an existing transaction */
	if (new_tran==-1) {
		if ( p_msg->REQ_METHOD==METHOD_ACK) {
			DBG( "SER: ACK received -> t_release\n");
			if ( !t_release_transaction( p_msg ) )
			{
				DBG( "SER: WARNING: bad t_release\n");
			}
			/* ack fwd-ing not needed -- only hbh ACK
			   match (new_tran==1) and do not need to
			   be fwd-ed */
			ret = 1;
		} else { /* non-ACK -- retransmit */
			ret=t_retransmit_reply( p_msg );
			/* look at ret for better debugging output ... */
			if (ret>0) DBG("DEBUG: reply retransmitted (status %d)\n", ret);
			else if (ret==-1) DBG("DEBUG: no reply to retransmit; "
				"probably a non-INVITE transaction which was not replied\n");
			else LOG(L_ERR, "ERROR: reply retranmission failed\n");
			/* do not worry and proceed whatever happened to you...*/
			ret = 1;
		}
	} else { /* it is a new transaction */
		if ( p_msg->REQ_METHOD==METHOD_ACK) {
			/* ACKs does not establish a transaction and is
			   fwd-ed statelessly */
			DBG( "SER: forwarding ACK  statelessly \n");
			ret=forward_request( p_msg , proxy ) ;
		} else {
			ret=t_forward_nonack(p_msg, proxy);
			if (ret<=0)
			{
				DBG( "SER:ERROR: t_forward \n");
				/* we reply statefuly and enter WAIT state since error might 
				   have occured in middle of forking and we do not 
				   want to put the forking burden on upstream client;
				   howver, it may fail too due to lack of memory */
				err2reason_phrase( ser_error, &sip_err,
					err_buffer, sizeof(err_buffer), "TM" );
				reply_ret=t_send_reply( p_msg , sip_err, err_buffer,0);
				t_release_transaction( p_msg );
				if (reply_ret>0) {
					/* we have taken care of all -- do nothing in
				  	script */
					DBG("ERROR: generation of a stateful reply "
						"on error succeeded\n");
					ret=0;
				}  else {
					DBG("ERROR: generation of a stateful reply "
						"on error failed\n");
				};
			} else { /* let upstream know, I forwarded downstream */
				if ( p_msg->REQ_METHOD==METHOD_CANCEL)
				{
					DBG( "SER: new CANCEL\n");
					if ( !t_send_reply( p_msg , 200, "glad to cancel", 0) )
						DBG( "SER:ERROR: t_send_reply\n");
				} else if (p_msg->REQ_METHOD==METHOD_INVITE)
				{
					DBG( "SER: new INVITE\n");
					if (!t_send_reply( p_msg , 100 ,
					"trying -- your call is important to us",0))
						DBG("SER: ERROR: t_send_reply (100)\n");
				} else {
					DBG( "SER: new transaction\n");
				}
			} /* forward_nonack succeeded */
		} /* a new non-ACK trancation */
	} /* a new transaction */


done:
	if (T!=T_UNDEFINED && T!=T_NULL) {
		T_UNREF( T );
	}
	return ret;
}


static int t_relay( struct sip_msg  *p_msg , char* _foo , char* _bar )
{
	str           *uri;
	struct proxy_l *p;
	int ret;

	/* the original uri has been changed? */
	if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
		uri = &(p_msg->first_line.u.request.uri);
	else
		uri = &(p_msg->new_uri);

	p=uri2proxy( uri );
	if (p==0) return E_BAD_ADDRESS;

	/* relay now */
	ret=t_relay_to( p_msg, p );
	free_proxy( p );
	free( p );
	return ret;
}

