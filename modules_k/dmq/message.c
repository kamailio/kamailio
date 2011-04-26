#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../sip_msg_clone.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../../ut.h"
#include "dmq.h"
#include "worker.h"
#include "peer.h"
#include "message.h"

str dmq_200_rpl  = str_init("OK");
str dmq_400_rpl  = str_init("Bad request");
str dmq_500_rpl  = str_init("Server Internal Error");
str dmq_404_rpl  = str_init("User Not Found");

int handle_dmq_message(struct sip_msg* msg, char* str1, char* str2) {
	dmq_peer_t* peer;
	struct sip_msg* cloned_msg = NULL;
	int cloned_msg_len;
	if ((parse_sip_msg_uri(msg) < 0) || (!msg->parsed_uri.user.s)) {
			LM_ERR("error parsing msg uri\n");
			goto error;
	}
	LM_DBG("handle_dmq_message [%.*s %.*s] [%s %s]\n",
	       msg->first_line.u.request.method.len, msg->first_line.u.request.method.s,
	       msg->first_line.u.request.uri.len, msg->first_line.u.request.uri.s,
	       ZSW(str1), ZSW(str2));
	/* the peer id is given as the userinfo part of the request URI */
	peer = find_peer(msg->parsed_uri.user);
	if(!peer) {
		LM_DBG("no peer found for %.*s\n", msg->parsed_uri.user.len, msg->parsed_uri.user.s);
		if(slb.freply(msg, 404, &dmq_404_rpl) < 0)
		{
			LM_ERR("sending reply\n");
			goto error;
		}
		return 0;
	}
	LM_DBG("handle_dmq_message peer found: %.*s\n", msg->parsed_uri.user.len, msg->parsed_uri.user.s);
	cloned_msg = sip_msg_shm_clone(msg, &cloned_msg_len, 1);
	if(!cloned_msg) {
		LM_ERR("error cloning sip message\n");
		goto error;
	}
	add_dmq_job(cloned_msg, peer);
	return 0;
error:
	return -1;
}