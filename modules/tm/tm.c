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



/*static int test_f(struct sip_msg*, char*,char*);*/
static int w_t_check(struct sip_msg* msg, char* str, char* str2);
static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_t_forward(struct sip_msg* msg, char* str, char* str2);
static int w_t_forward_def(struct sip_msg* msg, char* str, char* str2);
static int w_t_release(struct sip_msg* msg, char* str, char* str2);
static int fixup_t_forward(void** param, int param_no);
static int fixup_t_forward_def(void** param, int param_no);
static int fixup_t_send_reply(void** param, int param_no);

static struct module_exports nm_exports= {
	"tm_module",
	(char*[]){			"t_add_transaction",
				"t_lookup_request",
				"t_forward",
				"t_forward_def",
				"t_forward_uri",
				"t_send_reply",
				"t_retransmit_reply",
				"t_release",
				"t_unref"
			},
	(cmd_function[]){
					t_add_transaction,
					w_t_check,
					w_t_forward,
					w_t_forward_def,
					t_forward_uri,
					w_t_send_reply,
					t_retransmit_reply,
					w_t_release,
					t_unref
					},
	(int[]){
				0,
				0,
				2,
				1,
				0,
				2,
				0,
				0,
				0
			},
	(fixup_function[]){
				0,
				0,
				fixup_t_forward,
				fixup_t_forward_def,
				0,
				fixup_t_send_reply,
				0,
				0,
				0,
		},
	9,
	(response_function) t_on_reply_received,
	(destroy_function) tm_shutdown
};



#ifdef STATIC_TM
struct module_exports* tm_mod_register()
#else
struct module_exports* mod_register()
#endif
{

	DBG( "TM - registering...\n");
	if (tm_startup()==-1) return 0;
	return &nm_exports;
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
		ip=str2ip(name, len, &err);
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
					*param);
			return E_UNSPEC;
		}
	}
	return 0;
}



static int fixup_t_forward_def(void** param, int param_no)
{
	char* name;
	struct hostent* he;
	int err;
#ifdef DNS_IP_HACK
	unsigned int ip;
	int len;
#endif

	DBG("TM module: fixup_t_forward_def(%s, %d)\n", (char*)*param, param_no);
	if (param_no==1){
		name=*param;
#ifdef DNS_IP_HACK
		len=strlen(name);
		ip=str2ip(name, len, &err);
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
					*param);
			return E_UNSPEC;
		}
	}
	/* second param => no conversion*/
	return 0;
}



static int w_t_check(struct sip_msg* msg, char* str, char* str2)
{
	return t_check( msg , 0 );
}

static int w_t_forward(struct sip_msg* msg, char* str, char* str2)
{
	return t_forward(msg, (unsigned int) str, (unsigned int) str2);
}


static int w_t_forward_def(struct sip_msg* msg, char* str, char* str2)
{
	return t_forward(msg, (unsigned int) str, 5060 );
}


static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	return t_send_reply(msg, (unsigned int) str, str2);
}

static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	return t_release_transaction(msg);
}

