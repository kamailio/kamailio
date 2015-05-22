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

#include "mod_erlang.h"
#include "erl_helpers.h"
#include "erl_api.h"
#include "pv_xbuff.h"
#include "cnode.h"

#include <sys/socket.h>
#include <ei.h>

int _impl_api_rpc_call(ei_x_buff* reply, const str *module,const str *function, const ei_x_buff *args);
int _impl_reg_send(const str *server, const ei_x_buff *msg);
int _impl_send(const erlang_pid *pid, const ei_x_buff *msg);
int _impl_reply(const ei_x_buff *msg);
int xavp2xbuff(ei_x_buff *xbuff, sr_xavp_t *xavp);
int xbuff2xavp(sr_xavp_t **xavp, ei_x_buff *xbuff);

/*!
* \brief Function exported by module - it will load the other functions
 * \param erl_api Erlang API export binding
 * \return 1
 */
int load_erl( erl_api_t *erl_api )
{
	erl_api->rpc = _impl_api_rpc_call;
	erl_api->reg_send = _impl_reg_send;
	erl_api->send = _impl_send;
	erl_api->reply = _impl_reply;
	erl_api->xavp2xbuff = xavp2xbuff;
	erl_api->xbuff2xavp = xbuff2xavp;

	return 1;
}

/**
 * API implementation
 */
int xavp2xbuff(ei_x_buff *xbuff, sr_xavp_t *xavp)
{
	return xavp_encode(xbuff,xavp,0);
}


int xbuff2xavp(sr_xavp_t **xavp, ei_x_buff *xbuff)
{
	int i=0, version=0;
	if (ei_decode_version(xbuff->buff,&i,&version))
	{
		LM_DBG("no version byte encoded in reply\n");
	}

	return xavp_decode(xbuff,&i,xavp,0);
}

int _impl_api_rpc_call(ei_x_buff *reply, const str *module,const str *function, const ei_x_buff *args)
{
	struct msghdr msgh;
	struct iovec cnt[8];
	int pid_no = my_pid();
	eapi_t api = API_RPC_CALL;
	int buffsz=0;
	int rc;

	memset(&msgh, 0, sizeof(msgh));
	memset(&cnt, 0, sizeof(cnt));

	/* Kamailio PID */
	cnt[0].iov_base = (void*)&pid_no;
	cnt[0].iov_len  = sizeof(pid_no);

	/* method */
	cnt[1].iov_base = (void*)&api;
	cnt[1].iov_len = sizeof(api);

	/* put size of following data */
	cnt[2].iov_base = (void*)&module->len;
	cnt[2].iov_len  = sizeof(int);

	cnt[3].iov_base = (void*)&function->len;
	cnt[3].iov_len  = sizeof(int);

	buffsz = args->index; /* occupied size */
	cnt[4].iov_base = (void*) &buffsz;
	cnt[4].iov_len = sizeof(buffsz);

	/* module name */
	cnt[5].iov_base = (void*)module->s;
	cnt[5].iov_len  = module->len;

	/* function name */
	cnt[6].iov_base = (void*)function->s;
	cnt[6].iov_len  = function->len;

	/* Erlang arguments content */
	cnt[7].iov_base = (void*)args->buff;
	cnt[7].iov_len = buffsz; /* occupied size */

	msgh.msg_iov = cnt;
	msgh.msg_iovlen = 8;

	while ((rc = sendmsg(csockfd, &msgh, 0)) == -1 && errno == EAGAIN)
		;

	if (rc == -1) {
		LM_ERR("sendmsg failed: %s\n",strerror(errno));
		return -1;
	}

	/*receive into reply buffer */
	cnt[1].iov_base = &buffsz;
	cnt[1].iov_len  = sizeof(buffsz);

	/* peek reply size safe */
	msgh.msg_iovlen = 2;
	while ((rc = recvmsg(csockfd, &msgh, MSG_PEEK)) == -1 && errno == EAGAIN)
		;

	if (rc == -1) {
		LM_ERR("recvmsg failed: %s\n",strerror(errno));
		return -1;
	}

	if (reply->buffsz < buffsz) {
		ei_x_free(reply);
		reply->buffsz = buffsz + 1;
		reply->buff = (char*)malloc(reply->buffsz);
	}

	cnt[2].iov_base = (void*)reply->buff;
	cnt[2].iov_len  = buffsz;

	msgh.msg_iovlen = 3;
	while ((rc = recvmsg(csockfd, &msgh, MSG_WAITALL)) == -1 && errno == EAGAIN)
		;

	if (rc == -1) {
		LM_ERR("recvmsg failed: %s\n",strerror(errno));
		return -1;
	}

	if(pid_no != my_pid()) {
		/* should never happened */
		LM_CRIT("BUG: got other process reply (pid_no=%d)\n",pid_no);
		return -1;
	}

	return 0;
}

int _impl_reg_send(const str *server, const ei_x_buff *msg)
{
	struct msghdr msgh;
	struct iovec cnt[6];
	int pid_no = my_pid();
	eapi_t api = API_REG_SEND;
	int buffsz;
	int rc;
	int i=0,version;

	memset(&msgh, 0, sizeof(msgh));
	memset(&cnt, 0, sizeof(cnt));

	if (ei_decode_version(msg->buff,&i,&version)) {
		LM_ERR("msg must be encoded with version\n");
		return -1;
	}

	/* Kamailio PID */
	cnt[0].iov_base = (void*)&pid_no;
	cnt[0].iov_len  = sizeof(pid_no);

	/* method */
	cnt[1].iov_base = (void*)&api;
	cnt[1].iov_len = sizeof(api);

	/* put size of following data */
	cnt[2].iov_base = (void*)&server->len;
	cnt[2].iov_len  = sizeof(int);

	buffsz = msg->index; /* occupied size */
	cnt[3].iov_base = (void*)&buffsz;
	cnt[3].iov_len = sizeof(buffsz);

	/* module name */
	cnt[4].iov_base = (void*)server->s;
	cnt[4].iov_len  = server->len;

	/* Erlang arguments content */
	cnt[5].iov_base = (void*)msg->buff;
	cnt[5].iov_len = buffsz; /* occupied size */

	msgh.msg_iov = cnt;
	msgh.msg_iovlen = 6;

	while ((rc = sendmsg(csockfd, &msgh, 0)) == -1 && errno == EAGAIN)
		;

	if (rc == -1) {
		LM_ERR("sendmsg failed: %s\n",strerror(errno));
		return -1;
	}

	/* no reply */

	return 0;
}

int _impl_reply(const ei_x_buff *msg)
{
	int i=0,version;

	if (ei_decode_version(msg->buff,&i,&version)) {
		LM_ERR("msg must be encoded with version\n");
		return -1;
	}

	/* must be in call back / event route */

	if (csockfd) {
		LM_ERR("not in callback\n");
		return -1;
	} else if (!enode) {
		LM_ERR("not connected\n");
		return -1;
	}
	/* copy into reply */
	if (enode->response.buffsz < msg->buffsz) {
		/* realocate */
		enode->response.buff=realloc(enode->response.buff,msg->buffsz);
		if (!enode->response.buff) {
			LM_ERR("realloc failed: not enough memory\n");
			return -1;
		}
		enode->response.buffsz = msg->buffsz;
	}

	memcpy((void*)enode->response.buff,(void*)msg->buff,msg->buffsz);
	enode->response.index = msg->index;

	return 0;
}

int _impl_send(const erlang_pid *pid, const ei_x_buff *msg)
{
	struct msghdr msgh;
	struct iovec cnt[6];
	int pid_no = my_pid();
	eapi_t api = API_SEND;
	int buffsz;
	int rc;
	int i=0,version;

	if (ei_decode_version(msg->buff,&i,&version)) {
		LM_ERR("msg must be encoded with version\n");
		return -1;
	}

	if (enode) {

		/* copy into reply */
		if (enode->response.buffsz < msg->buffsz) {
			/* realocate */
			enode->response.buff=realloc(enode->response.buff,msg->buffsz);
			if (!enode->response.buff) {
				LM_ERR("realloc failed: not enough memory\n");
				return -1;
			}
			enode->response.buffsz = msg->buffsz;
		}

		memcpy((void*)enode->response.buff,(void*)msg->buff,msg->buffsz);
		enode->response.index = msg->index;

		/* address process */
		cnode_reply_to_pid = (erlang_pid *)pid;
		return 0;
	} else if (csockfd) {

		/* send via cnode */
		memset(&msgh, 0, sizeof(msgh));
		memset(&cnt, 0, sizeof(cnt));

		/* Kamailio PID */
		cnt[0].iov_base = (void*)&pid_no;
		cnt[0].iov_len  = sizeof(pid_no);

		/* method */
		cnt[1].iov_base = (void*)&api;
		cnt[1].iov_len = sizeof(api);

		/* put size of following data */
		buffsz = msg->index; /* occupied size */
		cnt[2].iov_base = (void*)&buffsz;
		cnt[2].iov_len = sizeof(buffsz);

		/* module name */
		cnt[3].iov_base = (void*)pid;
		cnt[3].iov_len  = sizeof(erlang_pid);

		/* Erlang arguments content */
		cnt[4].iov_base = (void*)msg->buff;
		cnt[4].iov_len = buffsz; /* occupied size */

		msgh.msg_iov = cnt;
		msgh.msg_iovlen = 5;

		while ((rc = sendmsg(csockfd, &msgh, 0)) == -1 && errno == EAGAIN)
			;

		if (rc == -1) {
			LM_ERR("sendmsg failed: %s\n",strerror(errno));
			return -1;
		}
	} else {
		LM_ERR("not connected\n");
		return -1;
	}

	/* no reply */
	return 0;
}
