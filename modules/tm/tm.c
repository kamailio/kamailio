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



static int w_t_check(struct sip_msg* msg, char* str, char* str2);
static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_t_release(struct sip_msg* msg, char* str, char* str2);
static int fixup_t_forward(void** param, int param_no);
static int fixup_t_send_reply(void** param, int param_no);
static int w_t_unref( struct sip_msg* p_msg, char* foo, char* bar );
static int w_t_retransmit_reply(struct sip_msg* p_msg, char* foo, char* bar );
static int w_t_add_transaction(struct sip_msg* p_msg, char* foo, char* bar );
static int t_relay_to( struct sip_msg *p_msg, char *str_ip, char *str_port);
static int t_relay( struct sip_msg  *p_msg ,  char* foo, char* bar  );
static int w_t_forward_ack(struct sip_msg* msg, char* str, char* str2);
static int w_t_forward_nonack(struct sip_msg* msg, char* str, char* str2);
static int w_t_add_fork_ip(struct sip_msg* msg, char* str, char* str2);
static int w_t_add_fork_uri(struct sip_msg* msg, char* str, char* str2);
static int fixup_t_add_fork_uri(void** param, int param_no);
static int w_t_add_fork_on_no_rpl(struct sip_msg* msg,char* str,char* str2);
static int w_t_clear_forks(struct sip_msg* msg, char* str, char* str2);
static void w_onbreak(struct sip_msg* msg) { t_unref(); }

static int mod_init(void);

#ifdef STATIC_TM
struct module_exports tm_exports = {
#else
struct module_exports exports= {
#endif
	"tm",
	/* -------- exported functions ----------- */
	(char*[]){			
				"t_add_transaction",
				"t_lookup_request",
				"t_send_reply",
				"t_retransmit_reply",
				"t_release",
				"t_unref",
				"t_relay_to",
				"t_relay",
				"t_forward_nonack",
				"t_forward_ack",
				"t_fork_to_ip",
				"t_fork_to_uri",
				"t_clear_forks",
				"t_fork_on_no_response",
				"register_tmcb",
				"load_tm"
			},
	(cmd_function[]){
					w_t_add_transaction,
					w_t_check,
					w_t_send_reply,
					w_t_retransmit_reply,
					w_t_release,
					w_t_unref,
					t_relay_to,
					t_relay,
					w_t_forward_nonack,
					w_t_forward_ack,
					w_t_add_fork_ip,
					w_t_add_fork_uri,
					w_t_clear_forks,
					w_t_add_fork_on_no_rpl,
					(cmd_function) register_tmcb,
					(cmd_function) load_tm
					},
	(int[]){
				0, /* t_add_transaction */
				0, /* t_lookup_request */
				2, /* t_send_reply */
				0, /* t_retransmit_reply */
				0, /* t_release */
				0, /* t_unref */
				2, /* t_relay_to */
				0, /* t_relay */
				2, /* t_forward_nonack */
				2, /* t_forward_ack */
				2, /* t_fork_to_ip */
				1, /* t_fork_to_uri */
				0, /* t_clear_forks */
				1,  /* t_add_fork_on_no_response */
				2 /* register_tmcb */,
				1 /* load_tm */
			},
	(fixup_function[]){
				0,						/* t_add_transaction */
				0,						/* t_lookup_request */
				fixup_t_send_reply,		/* t_send_reply */
				0,						/* t_retransmit_reply */
				0,						/* t_release */
				0,						/* t_unref */
				fixup_t_forward,		/* t_relay_to */
				0,						/* t_relay */
				fixup_t_forward,		/* t_forward_nonack */
				fixup_t_forward,		/* t_forward_ack */
				fixup_t_forward,		/* t_fork_to_ip */
				fixup_t_add_fork_uri,   /* t_fork_to_uri */
				0,						/* t_clear_forks */
				fixup_t_add_fork_uri,	/* t_add_fork_on_no_response */
				0,						/* register_tmcb */
				0						/* load_tm */
	
		},
	16,

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




static int fixup_t_forward(void** param, int param_no)
{
	char* name;
	struct hostent* he;
	unsigned int port;
	int err;
#ifdef DNS_IP_HACK
	unsigned int ip;
	int len;
#endif

	DBG("TM module: fixup_t_forward(%s, %d)\n", (char*)*param, param_no);
	if (param_no==1){
		name=*param;
#ifdef DNS_IP_HACK
		len=strlen(name);
		ip=str2ip((unsigned char*)name, len, &err);
		if (err==0){
			goto copy;
		}
#endif
		/* fail over to normal lookup */
		he=gethostbyname(name);
		if (he==0){
			LOG(L_CRIT, "ERROR: mk_proxy: could not resolve hostname:"
						" \"%s\"\n", name);
			return E_BAD_ADDRESS;
		}
		memcpy(&ip, he->h_addr_list[0], sizeof(unsigned int));
	copy:
		free(*param);
		*param=(void*)ip;
		return 0;
	}else if (param_no==2){
		port=htons(str2s(*param, strlen(*param), &err));
		if (err==0){
			free(*param);
			*param=(void*)port;
			return 0;
		}else{
			LOG(L_ERR, "TM module:fixup_t_forward: bad port number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	return 0;
}




static int fixup_t_send_reply(void** param, int param_no)
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




static int w_t_check(struct sip_msg* msg, char* str, char* str2)
{
	return t_check( msg , 0 , 0 ) ? 1 : -1;
}




static int w_t_forward_ack(struct sip_msg* msg, char* str, char* str2)
{
	if (t_check( msg , 0 , 0 )==-1) return -1;
	if (!T) {
		DBG("DEBUG: t_forward_ack: no transaction found \n");
		return -1;
	}
	return t_forward_ack(msg, (unsigned int) str, (unsigned int) str2);
}




static int w_t_forward_nonack(struct sip_msg* msg, char* str, char* str2)
{
	if (t_check( msg , 0 , 0)==-1) return -1;
	if (!T) {
		DBG("DEBUG: t_forward_nonack: no transaction found\n");
		return -1;
	}
	return t_forward_nonack(msg, (unsigned int) str, (unsigned int) str2);
}



static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	if (t_check( msg , 0 , 0)==-1) return -1;
	if (!T) {
		LOG(L_ERR, "ERROR: t_send_reply: cannot send a t_reply to a message "
			"for which no T-state has been established\n");
		return -1;
	}
	return t_send_reply(msg, (unsigned int) str, str2, 0);
}




static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	if (t_check( msg  , 0 , 0 )==-1) return 1;
	if ( T && T!=T_UNDEFINED ) 
		return t_put_on_wait( T );
	return 1;
}




static int w_t_unref( struct sip_msg* p_msg, char* foo, char* bar )
{
	if (T==T_UNDEFINED || T==T_NULL)
		return -1;
    return t_unref( /* p_msg */ );
}




static int w_t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar)
{
	if (t_check( p_msg  , 0 , 0 )==-1) 
		return 1;
	if (T)
		return t_retransmit_reply( p_msg );
	else 
		return -1;
	return 1;
}




static int w_t_add_transaction( struct sip_msg* p_msg, char* foo, char* bar ) {
	if (t_check( p_msg , 0 , 0 )==-1) return -1;
	if (T) {
		LOG(L_ERR,"ERROR: t_add_transaction: won't add a retransmission\n");
		return -1;
	}
	return t_add_transaction( p_msg );
}




static int w_t_add_fork_ip(struct sip_msg* msg, char* str, char* str2)
{
	return t_add_fork((unsigned int)str, (unsigned int)str2, 0,0,DEFAULT,0);
}




static int fixup_t_add_fork_uri(void** param, int param_no)
{
	unsigned int ip, port;
	struct fork* fork_pack;

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
		if ( get_ip_and_port_from_uri(&(fork_pack->uri), &ip, &port)==0 )
		{
			fork_pack->ip = ip;
			fork_pack->port = port;
			*param=(void*)fork_pack;
			return 0;
		}else{
			LOG(L_ERR, "TM module:fixup_t_add_fork_uri: bad uri <%s>\n",
			(char*)(*param));
			return E_UNSPEC;
		}
	}
	/* second param => no conversion*/
	return 0;
}




static int w_t_add_fork_uri(struct sip_msg* msg, char* str, char* str2)
{
	return t_add_fork( ((struct fork*)str)->ip, ((struct fork*)str)->port,
		((struct fork*)str)->uri.s, ((struct fork*)str)->uri.len, DEFAULT,0);
}




static int w_t_clear_forks(struct sip_msg* msg, char* str, char* str2)
{
	return t_clear_forks();
}




static int w_t_add_fork_on_no_rpl(struct sip_msg* msg, char* str, char* str2)
{
	return t_add_fork( ((struct fork*)str)->ip, ((struct fork*)str)->port,
		((struct fork*)str)->uri.s,((struct fork*)str)->uri.len,NO_RESPONSE,0);
}




static int t_relay_to( struct sip_msg  *p_msg , char *str_ip , char *str_port)
{
	struct proxy_l *proxy;
	enum addifnew_status status;
	int ret=0;

	status = t_addifnew( p_msg );

	switch( status ) {
		case AIN_ERROR:		/*  fatal error (e.g, parsing) occured */
			ret = 0;
			break;
		case AIN_RETR:		/* it's a retransmission */
			ret=t_retransmit_reply( p_msg );
			/* look at ret for better debugging output ... */
			if (ret>0) DBG("DEBUG: reply retransmitted (status %d)\n", ret);
			else if (ret==-1) DBG("DEBUG: no reply to retransmit; "
				"probably a non-INVITE transaction which was not replied\n");
			else LOG(L_ERR, "ERROR: reply retranmission failed\n");
			/* eventually, do not worry and proceed whatever happened to you...*/
			ret = 1;
			break;
		case AIN_NEW:		/* it's a new request */
			if (!t_forward_nonack(p_msg,(unsigned int)str_ip,
			(unsigned int) str_port ))
			{
				DBG( "SER:ERROR: t_forward \n");
				ret = 0;
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
				ret = 1;
			}
			break;
		case AIN_NEWACK:	/* it's an ACK for which no transaction exists */
			DBG( "SER: forwarding ACK  statelessly \n");
			proxy = mk_proxy_from_ip( (unsigned int)str_ip,
				ntohs((unsigned int)str_port) );
			forward_request( p_msg , proxy ) ;
			free_proxy(proxy);
			free(proxy);
			ret=1;
			break;
		case AIN_OLDACK:	/* it's an ACK for an existing transaction */
			DBG( "SER: ACK received -> t_release\n");
			if ( !t_release_transaction( p_msg ) )
			{
				DBG( "SER: WARNING: bad t_release\n");
			}
			/* t_forward decides whether to forward (2xx) 
			   or not (anything else) */
			if ( !t_forward_ack( p_msg , (unsigned int) str_ip ,
			(unsigned int) str_port ) )
				DBG( "SER: WARNING: bad ACK forward\n");
			ret = 1;
			break;
		case AIN_RTRACK:	/* ACK retransmission */
			DBG( "SER: ACK retransmission\n");
			ret = 1;
			break;
		default:
			LOG(L_CRIT, "ERROR: unexpected addifnew return value: %d\n", ret);
			abort();
	};
	if (T) {
		T_UNREF( T );
	}
	return ret;
}




static int t_relay( struct sip_msg  *p_msg , char* foo, char* bar)
{
	unsigned int  ip, port;
	str           *uri;

	/* the original uri has been changed? */
	if (p_msg->new_uri.s==0 || p_msg->new_uri.len==0)
		uri = &(p_msg->first_line.u.request.uri);
	else
		uri = &(p_msg->new_uri);
	/* extracts ip and port from uri */		
	if ( get_ip_and_port_from_uri( uri , &ip, &port)<0 )
	{
		LOG( L_ERR , "ERROR: t_on_request_received_uri: unable"
			" to extract ip and port from uri!\n" );
		return -1;
	}
	return t_relay_to( p_msg , ( char *) ip , (char *) port );
}



