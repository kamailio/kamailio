/*$Id$
 *
 * TM module
 *
 */



#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "sip_msg.h"
#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "h_table.h"
#include "t_funcs.h"



/*static int test_f(struct sip_msg*, char*,char*);*/
static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_t_forward(struct sip_msg* msg, char* str, char* str2);
static int t_forward_uri(struct sip_msg* msg, char* str, char* str2);
static int fixup_t_forward(void** param, int param_no);
static int fixup_t_send_reply(void** param, int param_no);

static struct module_exports nm_exports= {
	"tm_module", 
	(char*[]){	"t_add_transaction",
				"t_lookup_request",
				"t_forward",
				"t_forward_uri",
				"t_send_reply",
				"t_retransmit_reply"
			},
	(cmd_function[]){
					t_add_transaction,
					t_lookup_request,
					w_t_forward,
					t_forward_uri,
					w_t_send_reply,
					t_retransmit_reply,
					},
	(int[]){
				0,
				0,
				2,
				0,
				2,
				0
			},
	(fixup_function[]){
				0,
				0,
				fixup_t_forward,
				0,
				fixup_t_send_reply,
				0
		},
	6,
	(response_function) t_on_reply_received
};



struct module_exports* mod_register()
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



static int t_forward_uri(struct sip_msg* msg, char* str, char* str2)
{

	LOG(L_CRIT, "BUG: TM module: t_forwad_uri not implemented!");
	return -1;
}



static int w_t_forward(struct sip_msg* msg, char* str, char* str2)
{
	return t_forward(msg, (unsigned int) str, (unsigned int) str2);
}



static int w_t_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	return t_send_reply(msg, (unsigned int) str, str2);
}



#if 0
static int test_f(struct sip_msg* msg, char* s1, char* s2)
{
	struct sip_msg* tst;
	struct hdr_field* hf;

	DBG("in test_f\n");

	tst=sip_msg_cloner(msg);
	DBG("after cloner...\n");
	DBG("first_line: <%s> <%s> <%s>\n", 
			tst->first_line.u.request.method.s,
			tst->first_line.u.request.uri.s,
			tst->first_line.u.request.version.s
		);
	if (tst->h_via1)
		DBG("via1: <%s> <%s>\n", tst->h_via1->name.s, tst->h_via1->body.s);
	if (tst->h_via2)
		DBG("via2: <%s> <%s>\n", tst->h_via2->name.s, tst->h_via2->body.s);
	if (tst->callid)
		DBG("callid: <%s> <%s>\n", tst->callid->name.s, tst->callid->body.s);
	if (tst->to)
		DBG("to: <%s> <%s>\n", tst->to->name.s, tst->to->body.s);
	if (tst->cseq)
		DBG("cseq: <%s> <%s>\n", tst->cseq->name.s, tst->cseq->body.s);
	if (tst->from)
		DBG("from: <%s> <%s>\n", tst->from->name.s, tst->from->body.s);
	if (tst->contact)
		DBG("contact: <%s> <%s>\n", tst->contact->name.s,tst->contact->body.s);

	DBG("headers:\n");
	for (hf=tst->headers; hf; hf=hf->next){
		DBG("header %d: <%s> <%s>\n", hf->type, hf->name.s, hf->body.s);
	}
	if (tst->eoh!=0) return;
	DBG("parsing all...\n"); 
	if (parse_headers(tst, HDR_EOH)==-1)
		DBG("error\n");
	else{
		DBG("new headers...");
		for (hf=tst->headers; hf; hf=hf->next){
			DBG("header %d: <%s> <%s>\n", hf->type, hf->name.s, hf->body.s);
		}
	}

	free(tst);
}

#endif


