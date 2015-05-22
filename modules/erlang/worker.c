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

#include <stdlib.h>
#include "worker.h"
#include "cnode.h"
#include "../../mem/mem.h"

#include "erl_api.h"

int worker_rpc_impl(ei_cnode *ec, int s, int wpid);
int worker_reg_send_impl(ei_cnode *ec, int s, int wpid);
int worker_send_impl(ei_cnode *ec, int s, int wpid);

int worker_init(worker_handler_t *phandler, int fd, const ei_cnode *ec)
{
	if (erl_set_nonblock(fd)){
		LM_ERR("set non blocking failed\n");
	}

	phandler->handle_f = handle_worker;
	phandler->wait_tmo_f = wait_tmo_worker;
	phandler->destroy_f = NULL;
	phandler->sockfd = fd;
	phandler->ec = *ec;
	phandler->next = NULL;

	return 0;
}

int handle_worker(handler_common_t *phandler)
{
	worker_handler_t* w = (worker_handler_t*)phandler;
	struct msghdr msg;
	struct iovec cnt[2];
	int wpid = 0;
	eapi_t api;
	int rc;

	/* ensure be connected */
	enode_connect();

	memset((void*)&msg,0,sizeof(msg));

	/* Kamailio worker PID */
	cnt[0].iov_base = &wpid;
	cnt[0].iov_len  = sizeof(wpid);

	/* method */
	cnt[1].iov_base = &api;
	cnt[1].iov_len  = sizeof(api);

	msg.msg_iov = cnt;
	msg.msg_iovlen = 2;

	while ((rc = recvmsg(w->sockfd, &msg, MSG_WAITALL)) == -1 && errno == EAGAIN)
		;

	if (rc < 0){
		LM_ERR("recvmsg failed (socket=%d): %s\n",w->sockfd,strerror(errno));
		return -1;
	}

	switch(api) {
	case API_RPC_CALL:
		if (worker_rpc_impl(&w->ec,w->sockfd,wpid))
			return -1;
		break;
	case API_REG_SEND:
		if (worker_reg_send_impl(&w->ec,w->sockfd,wpid))
			return -1;
		break;
	case API_SEND:
		if (worker_send_impl(&w->ec,w->sockfd,wpid))
			return -1;
		break;
	default:
		LM_ERR("BUG: bad method or not implemented!\n");
		return 1;
	}

	return 0;
}

int wait_tmo_worker(handler_common_t* phandler){

	return 0;
}

/**
 * internal implementation
 */
int worker_rpc_impl(ei_cnode *ec, int s,int wpid)
{
	str module = STR_NULL;
	str function = STR_NULL;
	ei_x_buff args;
	ei_x_buff reply;
	struct msghdr msgh;
	struct iovec cnt[6];
	int rc;

	memset((void*)&args,0,sizeof(args));
	memset((void*)&reply,0,sizeof(reply));
	memset((void*)&msgh,0,sizeof(msgh));

	/* module name length */
	cnt[0].iov_base = &module.len;
	cnt[0].iov_len  = sizeof(int);

	/* function name length */
	cnt[1].iov_base = &function.len;
	cnt[1].iov_len  = sizeof(int);

	/* Erlang args size */
	cnt[2].iov_base = &args.buffsz;
	cnt[2].iov_len  = sizeof(int);

	/* get data size */
	msgh.msg_iov    = cnt;
	msgh.msg_iovlen = 3;
	while ((rc = recvmsg(s, &msgh, MSG_PEEK)) == -1 && errno == EAGAIN)
		;

	if (rc == -1){
		LM_ERR("recvmsg failed (socket=%d): %s\n",s,strerror(errno));
		goto err;
	}

	/* allocate space */
	module.s = (char*)pkg_malloc(module.len+1);
	if (!module.s) {
		LM_ERR("not enough memory\n");
		goto err;
	}

	function.s = (char*)pkg_malloc(function.len+1);
	if (!function.s) {
		LM_ERR("not enough memory\n");
		goto err;
	}

	args.buff = (char*)malloc(args.buffsz);
	if (!args.buff) {
		LM_ERR("malloc: not enough memory\n");
		goto err;
	}

	/* buffers */
	cnt[3].iov_base = module.s;
	cnt[3].iov_len  = module.len;

	cnt[4].iov_base = function.s;
	cnt[4].iov_len  = function.len;

	cnt[5].iov_base = args.buff;
	cnt[5].iov_len  = args.buffsz;

	/* get whole data */
	msgh.msg_iovlen = 6;
	while ((rc = recvmsg(s, &msgh, MSG_WAITALL)) == -1 && errno == EAGAIN)
		;

	if (rc == -1){
		LM_ERR("recvmsg failed (socket=%d): %s\n",s,strerror(errno));
		goto err;
	}

	/* fix str */
	module.s[module.len] = 0;
	function.s[function.len] = 0;

	LM_DBG("rpc: %.*s:%.*s(args)\n",STR_FMT(&module),STR_FMT(&function));

	EI_X_BUFF_PRINT(&args);

	if(!enode) {
		LM_NOTICE("there is no connected Erlang node\n");
		/* reply up with error */
		ei_x_format(&reply, "{error,cnode,~a}", "no_erlang_node");
		goto reply;
	}

	/* do RPC */
	if ((rc = ei_rpc(ec,enode->sockfd,module.s,function.s,args.buff,args.buffsz,&reply)) == ERL_ERROR)
	{
		reply.index = 0; /* re-use reply buffer */

		if (erl_errno)
		{
			ei_x_format(&reply, "{error,cnode,~s}", strerror(erl_errno));
			LM_ERR("ei_rpc failed on node=<%s> socket=<%d>: %s\n",enode->conn.nodename,enode->sockfd,strerror(erl_errno));
		}
		else if (errno)
		{
			ei_x_format(&reply, "{error,cnode,~s}", strerror(errno));
			LM_ERR("ei_rpc failed on node=<%s> socket=<%d>: %s\n",enode->conn.nodename,enode->sockfd,strerror(errno));
		}
		else
		{
			ei_x_format(&reply, "{error,cnode,~s}", "Unknown error.");
			LM_ERR("ei_rpc failed on node=<%s> socket=<%d>, Unknown error.\n",ec->thisalivename,enode->sockfd);
		}
	}

reply:
	EI_X_BUFF_PRINT(&reply);

	cnt[0].iov_base = (void*)&wpid;
	cnt[0].iov_len  = sizeof(int);

	/* send reply to Kamailio worker */
	cnt[1].iov_base = (void*)&reply.buffsz;
	cnt[1].iov_len  = sizeof(int);

	cnt[2].iov_base = (void*)reply.buff;
	cnt[2].iov_len  = reply.buffsz;

	msgh.msg_iovlen = 3;
	while ((rc = sendmsg(s, &msgh, 0)) == -1 && errno == EAGAIN)
		;

	if (rc == -1) {
		LM_ERR("sendmsg failed on socket=%d rpid_no=%d; %s\n",s, wpid, strerror(errno));
		goto err;
	};

	pkg_free(module.s);
	pkg_free(function.s);
	free(args.buff);
	ei_x_free(&reply);
	return 0;

err:
	pkg_free(module.s);
	pkg_free(function.s);
	free(args.buff);
	ei_x_free(&reply);
	abort(); /* cant't recover */
	return -1;
}

int worker_reg_send_impl(ei_cnode *ec, int s,int wpid)
{
	str server = STR_NULL;
	ei_x_buff emsg;
	struct msghdr msgh;
	struct iovec cnt[6];
	int rc;

	memset((void*)&emsg,0,sizeof(emsg));

	memset((void*)&msgh,0,sizeof(msgh));

	/* server name length */
	cnt[0].iov_base = &server.len;
	cnt[0].iov_len  = sizeof(int);

	/* Erlang args size */
	cnt[1].iov_base = &emsg.buffsz;
	cnt[1].iov_len  = sizeof(int);

	/* get data size */
	msgh.msg_iov    = cnt;
	msgh.msg_iovlen = 2;

	while ((rc = recvmsg(s, &msgh, MSG_PEEK)) == -1 && errno == EAGAIN)
		;

	if (rc == -1){
		LM_ERR("recvmsg failed (socket=%d): %s\n",s,strerror(errno));
		return -1;
	}

	/* allocate space */
	server.s = (char*)pkg_malloc(server.len+1);
	if (!server.s) {
		LM_ERR("not enough memory\n");
		goto err;
	}

	emsg.buff = (char*)malloc(emsg.buffsz);
	if (!emsg.buff) {
		LM_ERR("malloc: not enough memory\n");
		goto err;
	}

	/* buffers */
	cnt[2].iov_base = server.s;
	cnt[2].iov_len  = server.len;

	cnt[3].iov_base = emsg.buff;
	cnt[3].iov_len  = emsg.buffsz;

	/* get whole data */
	msgh.msg_iovlen = 4;
	while ((rc = recvmsg(s, &msgh, MSG_WAITALL)) == -1 && errno == EAGAIN)
		;

	if (rc == -1){
		LM_ERR("recvmsg failed (socket=%d): %s\n",s,strerror(errno));
		goto err;
	}

	/* fix str */
	server.s[server.len] = 0;

	if(!enode) {
		LM_NOTICE("there is no connected Erlang node\n");
		goto err;
	}

	LM_DBG(">> {%.*s,'%s'} ! emsg\n",STR_FMT(&server),enode->conn.nodename);

	EI_X_BUFF_PRINT(&emsg);

	/* do ERL_REG_SEND */
	if ((rc = ei_reg_send(ec,enode->sockfd,server.s,emsg.buff,emsg.buffsz)) == ERL_ERROR)
	{
		if (erl_errno)
		{
			LM_ERR("ei_reg_send failed on node=<%s> socket=<%d>: %s\n",enode->conn.nodename,enode->sockfd,strerror(erl_errno));
		}
		else if (errno)
		{
			LM_ERR("ei_reg_send failed on node=<%s> socket=<%d>: %s\n",enode->conn.nodename,enode->sockfd,strerror(errno));
		}
		else
		{
			LM_ERR("ei_reg_send failed on node=<%s> socket=<%d>, Unknown error.\n",ec->thisalivename,enode->sockfd);
		}
	}

	pkg_free(server.s);
	free(emsg.buff);

	return 0;

err:
	pkg_free(server.s);
	free(emsg.buff);

	return -1;
}

int worker_send_impl(ei_cnode *ec, int s,int wpid)
{
	erlang_pid pid;
	ei_x_buff emsg;
	struct msghdr msgh;
	struct iovec cnt[6];
	int rc;

	memset((void*)&emsg,0,sizeof(emsg));

	memset((void*)&msgh,0,sizeof(msgh));

	/* Erlang args size */
	cnt[0].iov_base = &emsg.buffsz;
	cnt[0].iov_len  = sizeof(int);

	/* get data size */
	msgh.msg_iov    = cnt;
	msgh.msg_iovlen = 1;

	while ((rc = recvmsg(s, &msgh, MSG_PEEK)) == -1 && errno == EAGAIN)
		;

	if (rc == -1){
		LM_ERR("recvmsg failed (socket=%d): %s\n",s,strerror(errno));
		return -1;
	}

	emsg.buff = (char*)malloc(emsg.buffsz);
	if (!emsg.buff) {
		LM_ERR("malloc: not enough memory\n");
		goto err;
	}

	/* buffers */
	cnt[1].iov_base = &pid;
	cnt[1].iov_len  = sizeof(erlang_pid);

	cnt[2].iov_base = emsg.buff;
	cnt[2].iov_len  = emsg.buffsz;

	/* get whole data */
	msgh.msg_iovlen = 3;
	while ((rc = recvmsg(s, &msgh, MSG_WAITALL)) == -1 && errno == EAGAIN)
		;

	if (rc == -1){
		LM_ERR("recvmsg failed (socket=%d): %s\n",s,strerror(errno));
		goto err;
	}

	if(!enode) {
		LM_NOTICE("there is no connected Erlang node\n");
		goto err;
	}

	LM_DBG(">> <%s.%d.%d> ! emsg\n",pid.node,pid.num,pid.serial);

	EI_X_BUFF_PRINT(&emsg);

	/* do ERL_SEND */
	if ((rc = ei_send(enode->sockfd,&pid,emsg.buff,emsg.buffsz)) == ERL_ERROR)
	{
		if (erl_errno)
		{
			LM_ERR("ei_send failed on node=<%s> socket=<%d>: %s\n",enode->conn.nodename,enode->sockfd,strerror(erl_errno));
		}
		else if (errno)
		{
			LM_ERR("ei_send failed on node=<%s> socket=<%d>: %s\n",enode->conn.nodename,enode->sockfd,strerror(errno));
		}
		else
		{
			LM_ERR("ei_send failed on node=<%s> socket=<%d>, Unknown error.\n",ec->thisalivename,enode->sockfd);
		}
	}

	free(emsg.buff);

	return 0;

err:

	free(emsg.buff);

	return -1;
}
