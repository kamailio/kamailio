/*
 * $Id: datagram_fnc.c 1133 2007-04-02 17:31:13Z ancuta_onofrei $
 *
 * Copyright (C) 2007 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2007-06-25  first version (ancuta)
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include "../../resolve.h"
#include "mi_datagram.h"
#include "datagram_fnc.h"
#include "mi_datagram_parser.h"
#include "mi_datagram_writer.h"

/* solaris doesn't have SUN_LEN  */
#ifndef SUN_LEN
#define SUN_LEN(sa)	 ( strlen((sa)->sun_path) + \
					 (size_t)(((struct sockaddr_un*)0)->sun_path) )
#endif

int flags;
static char *mi_buf = 0;

static struct sockaddr_un reply_addr;
static unsigned int reply_addr_len;

/* Timeout for sending replies in milliseconds */
extern int mi_socket_timeout;
static unsigned int mi_socket_domain;

static int mi_sock_check(int fd, char* fname);

#define mi_create_dtgram_replysocket(_socketfd,_socket_domain, _err) \
	_socketfd = socket(_socket_domain, SOCK_DGRAM, 0);\
	if (_socketfd == -1) {\
		LOG(L_ERR, "ERROR: datagram_fnc: mi_create_dtgram_replysocket"\
			"cannot create socket: %s\n", strerror(errno));\
		goto _err;\
	}\
	/* Turn non-blocking mode on for tx*/\
	flags = fcntl(_socketfd, F_GETFL);\
	if (flags == -1){\
		LOG(L_ERR, "ERROR: datagram_fnc: mi_create_dtgram_replysocket: "\
		"fcntl failed: %s\n", strerror(errno));\
		goto _err;\
	}\
	if (fcntl(_socketfd, F_SETFL, flags | O_NONBLOCK) == -1) {\
		LOG(L_ERR, "ERROR: datagram_fnc: mi_create_dtgram_replysocket:"\
		"fcntl: set non-blocking failed: %s\n", strerror(errno));\
		goto _err;\
	}


int  mi_init_datagram_server(sockaddr_dtgram *addr, unsigned int socket_domain,
						rx_tx_sockets * socks, int mode, int uid, int gid )
{
	char * socket_name;

	/* create sockets rx and tx ... */
	/***********************************/
	mi_socket_domain = socket_domain;
	/**********************************/

	socks->rx_sock = socket(socket_domain, SOCK_DGRAM, 0);
	if (socks->rx_sock == -1) {
		LOG(L_ERR, "ERROR: datagram_fnc: mi_init_datagram_server: "
			"Cannot create RX socket: %s\n", strerror(errno));
		return -1;
	}

	switch(socket_domain)
	{
	case AF_LOCAL:
			DBG("DBG:datagram_fnc:mi_init_datagram_server: we have a "
				"unix socket, socket_name is %s\n",addr->unix_addr.sun_path);
			socket_name = addr->unix_addr.sun_path;
			if(bind(socks->rx_sock,(struct sockaddr*)&addr->unix_addr, 
					SUN_LEN(&addr->unix_addr))< 0) {
				LOG(L_ERR, "ERROR: datagram_fnc: mi_init_datagram_server: "
				"bind: %s\n", strerror(errno));
				goto err_rx;
			}
			if(mi_sock_check(socks->rx_sock, socket_name)!=0)
				goto err_rx;
			/* change permissions */
			if (mode){
				if (chmod(socket_name, mode)<0){
					LOG(L_ERR, "ERROR: datagram_fnc: mi_mod_init: failed to "
						"change the permissions for %s to %04o: %s[%d]\n",
							socket_name, mode, strerror(errno), errno);
					goto err_rx;
				}
			}
			/* change ownership */
			if ((uid!=-1) || (gid!=-1)){
				if (chown(socket_name, uid, gid)<0){
					LOG(L_ERR, "ERROR: datagram_fnc: mi_mod_init: failed to "
						"change the owner/group for %s  to %d.%d; %s[%d]\n",
						socket_name, uid, gid, strerror(errno), errno);
					goto err_rx;
				}
			}
			break;

	case AF_INET:
			if (bind(socks->rx_sock, (struct sockaddr*)&addr->udp_addr.sin,
						sizeof(addr->udp_addr))	< 0) {
				LOG(L_ERR, "ERROR: datagram_fnc: mi_init_datagram_server: "
					"bind: %s\n", strerror(errno));
				goto err_rx;
			}
			break;
#ifdef USE_IPV6
	case AF_INET6: 
			if(bind(socks->rx_sock, (struct sockaddr*)&addr->udp_addr.sin6,
					sizeof(addr->udp_addr)) < 0) {
				LOG(L_ERR, "ERROR: datagram_fnc: mi_init_datagram_server: "
					"bind: %s\n", strerror(errno));
				goto err_rx;
			}
			break;
#endif
	default:
			LOG(L_ERR, "ERROR: mi_datagram: mi_init_datagram_server: domain "
				"not supported\n");
			goto err_both;

	}
	mi_create_dtgram_replysocket(socks->tx_sock,socket_domain, err_both);

	return 0;
err_both:
	close(socks->tx_sock);
err_rx:
	close(socks->rx_sock);
	return -1;
}



int mi_init_datagram_buffer(void){

	mi_buf = pkg_malloc(DATAGRAM_SOCK_BUF_SIZE);
	if ( mi_buf==NULL) {
		LOG(L_ERR,"ERROR:datagram_fnc:mi_init_datagram_server: no more "
		"pkg memory\n");
		return -1;
	}
	return 0;
}

/* reply socket security checks:
 * checks if fd is a socket, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * returns 0 if ok, <0 if not */
int mi_sock_check(int fd, char* fname)
{
	struct stat fst;
	struct stat lst;

	if (fstat(fd, &fst)<0){
		LOG(L_ERR, "ERROR:datagram_fnc:mi_sock_check: fstat failed: %s\n", 
		strerror(errno));
		return -1;
	}
	/* check if socket */
	if (!S_ISSOCK(fst.st_mode)){
		LOG(L_ERR, "ERROR:datagram_fnc:mi_sock_check: %s is not a sock\n", 
		fname);
		return -1;
	}
	/* check if hard-linked */
	if (fst.st_nlink>1){
		LOG(L_ERR, "ERROR:datagram_fnc:mi_sock_check: security: sock_check: "
			"%s is hard-linked %d times\n", fname, (unsigned)fst.st_nlink);
		return -1;
	}

	/* lstat to check for soft links */
	if (lstat(fname, &lst)<0){
		LOG(L_ERR, "ERROR:datagram_fnc:mi_sock_check: lstat failed: %s\n",
				strerror(errno));
		return -1;
	}
	if (S_ISLNK(lst.st_mode)){
		LOG(L_ERR, "ERROR:datagram_fnc:mi_sock_check: security: sock_check: "
			"%s is a soft link\n", fname);
		return -1;
	}
	/* if this is not a symbolic link, check to see if the inode didn't
	 * change to avoid possible sym.link, rm sym.link & replace w/ sock race
	 */
	/*DBG("for %s lst.st_dev %fl fst.st_dev %i lst.st_ino %i fst.st_ino"
		"%i\n", fname, lst.st_dev, fst.st_dev, lst.st_ino, fst.st_ino);*/
	/*if ((lst.st_dev!=fst.st_dev)||(lst.st_ino!=fst.st_ino)){
		LOG(L_ERR, "ERROR:datagram_fnc:mi_sock_check: security: sock_check: "
			"socket	%s inode/dev number differ: %d %d \n", fname,
			(int)fst.st_ino, (int)lst.st_ino);
	}*/
	/* success */
	return 0;
}



/* this function sends the reply over the reply socket */
static int mi_send_dgram(int fd, char* buf, unsigned int len, 
				const struct sockaddr* to, int tolen, int timeout)
{
	int n;
	size_t total_len;
	total_len = strlen(buf);

	/*DBG("mi_datagram:mi_send_dgram:response is %s \n tolen is %i "
			"and len is %i\n",buf,	tolen,len);*/

	if(total_len == 0 || tolen ==0)
		return -1;
	DBG("DBG: datagram_fnc: mi_send_dgram: datagram_size is %i\n",
			DATAGRAM_SOCK_BUF_SIZE);
	if (total_len>DATAGRAM_SOCK_BUF_SIZE)
	{
		DBG("DBG: datagram_fnc: mi_send_dgram: datagram too big, "
			"truncking, datagram_size is %i\n",DATAGRAM_SOCK_BUF_SIZE);
		len = DATAGRAM_SOCK_BUF_SIZE;
	}
	/*DBG("DBG:mi_datagram:mi_send_dgram: destination address length "
			"is %i\n", tolen);*/
	n=sendto(fd, buf, len, 0, to, tolen);
	return n;
}



/*function that verifyes that the function from the datagram's first 
 * line is correct and exists*/
static int identify_command(datagram_stream * dtgram, struct mi_cmd * *f)
{
	char *command,*p, *start;

	/* default offset for the command: 0 */
	p= dtgram->start;
	start = p;
	if (!p){
		LOG(L_ERR, "ERR:datagram_fnc:identify_command: null pointer  \n");
		return -1;
	}
	
	/*if no command*/
	if ( dtgram->len ==0 ){
		DBG("DBG:datagram_fnc:identify_command:command empty case1 \n");
		goto error;
	}
	if (*p != MI_CMD_SEPARATOR){
		LOG(L_ERR, "ERROR:datagram_fnc:identify_command: command must begin "
			"with: %c \n", MI_CMD_SEPARATOR);
		goto error;
	}
	command = p+1;

	DBG("DBG:datagram_fnc:identify_command: the command starts here: "
				"%s\n", command);
	p=strchr(command, MI_CMD_SEPARATOR );
	if (!p ){
		LOG(L_ERR, "ERROR:datagram_fnc:identify_command: empty command \n");
		goto error;
	}
	if(*(p+1)!='\n'){
		LOG(L_ERR,"ERROR:datagram_fnc:identify_command: the request first line"
			"is invalid :no newline after the second %c\n",MI_CMD_SEPARATOR);
		goto error;
	}

	/* make command zero-terminated */
	*p=0;
	DBG("DBG:datagram_fnc:identify_command: the command "
		"is %s\n",command);
	/*search for the appropiate command*/
	*f=lookup_mi_cmd( command, p-command);
	if(!*f)
		goto error;
	/*the current offset has changed*/
	DBG("mi_datagram:identify_command: dtgram->len is "
		"%i\n", dtgram->len);
	dtgram->current = p+2 ;
	dtgram->len -=p+2 - dtgram->start;
	DBG("mi_datagram:identify_command: dtgram->len is %i\n",dtgram->len);

	return 0;
error:
	return -1;
}


/*************************** async functions ******************************/
static inline void free_async_handler( struct mi_handler *hdl )
{
	if (hdl)
		shm_free(hdl);
}


static void datagram_close_async(struct mi_root *mi_rpl,struct mi_handler *hdl,
																	int done)
{
	datagram_stream dtgram;
	int ret;
	my_socket_address *p;
	int reply_sock, flags;

	p = (my_socket_address *)hdl->param;

	DBG("DEBUG:mi_datagram:datagram_close_async: the socket domain is %i and"
		"af_local is %i\n", p->domain,AF_LOCAL);

	mi_create_dtgram_replysocket(reply_sock,p->domain, err);


	if ( mi_rpl!=0 || done )
	{
		if (mi_rpl!=0) {
			/*allocate the response datagram*/	
			dtgram.start = pkg_malloc(DATAGRAM_SOCK_BUF_SIZE);
			if(!dtgram.start){
				LOG(L_ERR, "ERROR: mi_datagram_datagram_close_async: failed"
					"to allocate the response datagram \n");
				goto err;
			}
			/*build the response*/
			if(mi_datagram_write_tree(&dtgram , mi_rpl) != 0){
				LOG(L_ERR, "ERROR mi_datagram:datagram_close_async: error"
					"while building the response \n");	

				goto err1;
			}
			DBG("mi_datagram:close_async: the response is %s",
				dtgram.start);
		
			/*send the response*/
			ret = mi_send_dgram(reply_sock, dtgram.start,
							dtgram.current - dtgram.start, 
							 (struct sockaddr *)&p->address, 
							 p->address_len, mi_socket_timeout);
			if (ret>0){
				DBG("DBG mi_datagram:datagram_close_async : the "
					"response: %s has been sent in %i octets\n", 
					dtgram.start, ret);
			}else{
				LOG(L_ERR, "ERROR midatagram:mi_datagram_server: an error "
				"occured while sending the response, ret is %i\n",ret);
			}
			free_mi_tree( mi_rpl );
			pkg_free(dtgram.start);
		} else {
			mi_send_dgram(reply_sock, MI_COMMAND_FAILED, MI_COMMAND_FAILED_LEN,
							(struct sockaddr*)&reply_addr, reply_addr_len, 
							mi_socket_timeout);
		}
	}

	if (done)
		free_async_handler( hdl );

	close(reply_sock);
	return;

err1:
	pkg_free(dtgram.start);
err:
	close(reply_sock);
	return;
}



static inline struct mi_handler* build_async_handler(unsigned int sock_domain,
								struct sockaddr_un *reply_addr, 
								unsigned int reply_addr_len)
{
	struct mi_handler *hdl;
	void * p;
	my_socket_address * repl_address;	


	hdl = (struct mi_handler*)shm_malloc( sizeof(struct mi_handler) +
			sizeof(my_socket_address));
	if (hdl==0) {
		LOG(L_ERR,"ERROR:mi_datagram:build_async_handler: no more shm mem\n");
		return 0;
	}

	p = (void *)((hdl) + 1);
	repl_address = p;
	
	switch(sock_domain)
	{/*we can have either of these types of sockets*/
		case AF_LOCAL:	DBG("mi_datagram:mi_datagram_server :"
							"we have an unix socket\n");
						memcpy(&repl_address->address.unix_deb, 
								(struct sockaddr*)reply_addr, 
								reply_addr_len);
						break;
		case AF_INET:	DBG("mi_datagram:mi_datagram_server :"
							"we have an IPv4 socket\n");
						memcpy(&repl_address->address.inet_v4, 
								(struct sockaddr*)reply_addr, 
								reply_addr_len);
						break;
		case AF_INET6:	DBG("mi_datagram:mi_datagram_server :"
						"we have an IPv6 socket\n");
						memcpy(&repl_address->address.inet_v6, 
								(struct sockaddr*)reply_addr, 
								reply_addr_len);
						break;
		default:		LOG(L_CRIT, "BUG: mi_datagram:build_async_handler :"
							"socket_domain has an incorrect value\n");
	
	}
	repl_address->domain = sock_domain;
	repl_address->address_len  = reply_addr_len;

	hdl->handler_f = datagram_close_async;
	hdl->param = (void*)repl_address;

	return hdl;
}



void mi_datagram_server(int rx_sock, int tx_sock)
{
	struct mi_root *mi_cmd;
	struct mi_root *mi_rpl;
	struct mi_handler *hdl;
	struct mi_cmd * f;
	datagram_stream dtgram;

	int ret, len;

	ret = 0;
	f = 0;

	while(1){/*read the datagram*/
		memset(mi_buf, 0, DATAGRAM_SOCK_BUF_SIZE);
		reply_addr_len = sizeof(reply_addr);

		/* get the client's address */
		ret = recvfrom(rx_sock, mi_buf, DATAGRAM_SOCK_BUF_SIZE, 0, 
					(struct sockaddr*)&reply_addr, &reply_addr_len);

		if (ret == -1) {
			LOG(L_ERR, "datagram_fnc: mi_datagram_server: recvfrom: (%d) %s\n",
				errno, strerror(errno));
			if ((errno == EINTR) ||
				(errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == ECONNREFUSED)) {
				DBG("datagram_fnc:mi_datagram_server: Got %d (%s), going on\n",
					errno, strerror(errno));
				continue;
			}
			DBG("DBG:datagram_fnc:mi_datagram_server: error in recvfrom\n");
			continue;
		}

		if(ret == 0)
			continue;

		DBG("DBG: mi_datagram: mi_datagram_server: "
			"process %i has received %.*s\n", getpid(), ret, mi_buf);

		if(ret> DATAGRAM_SOCK_BUF_SIZE){
				LOG(L_ERR, "ERROR: mi_datagram:mi_datagram_server: "
					"buffer overflow\n");
				continue;
		}

		DBG("DBG:mi-datagram:mi_datagram_server: mi_buf is %s and "
			"we have received %i bytes\n",mi_buf, ret);
		dtgram.start 	= mi_buf;
		dtgram.len 		= ret;
		dtgram.current 	= dtgram.start;

		ret = identify_command(&dtgram, &f);
		DBG("DBG: datagram:mi_datagram_server: the function "
				"identify_command exited with the value %i\n", ret);
		/*analyze the command--from the first line*/
		if(ret != 0)
		{
			LOG(L_ERR, "ERROR:datagram_fnc:identify_command: command not "
				"available\n");
			mi_send_dgram(tx_sock, MI_COMMAND_NOT_AVAILABLE, 
						  MI_COMMAND_AVAILABLE_LEN, 
						  (struct sockaddr* )&reply_addr, reply_addr_len, 
						  mi_socket_timeout);
			continue;
		}
		DBG("DBG:datagram_fnc:mi_datagram_server: we have a valid "
			"command \n");

		/* if asyncron cmd, build the async handler */
		if (f->flags&MI_ASYNC_RPL_FLAG) {
			hdl = build_async_handler(mi_socket_domain, &reply_addr, 
										reply_addr_len);
			if (hdl==0) {
				LOG(L_ERR, "ERROR:datagram_fnc:mi_datagram_server: failed to "
					"build async handler\n");
				mi_send_dgram(tx_sock, MI_INTERNAL_ERROR, MI_INTERNAL_ERROR_LEN, 							(struct sockaddr* )&reply_addr, reply_addr_len, 
								mi_socket_timeout);
				continue;
			}
		} else{
			hdl = 0;
		}

		DBG("DBG:datagram_fnc:mi_datagram_server: after identifing "
				"the command, the received datagram is  %s\n",dtgram.current);

		/*if no params required*/
		if (f->flags&MI_NO_INPUT_FLAG) {
			DBG("DBG:datagram_fnc:mi_datagram_server: the function "
					"has no params\n");
			mi_cmd = 0;
		} else {
			DBG("DBG:datagram_fnc:mi_datagram_server: parsing the "
					"function's params\n");
			mi_cmd = mi_datagram_parse_tree(&dtgram);
			if (mi_cmd==NULL){
				LOG(L_ERR,"ERROR:datagram_fnc:mi_datagram_server:error parsing"
					"MI tree\n");
				mi_send_dgram(tx_sock, MI_PARSE_ERROR, MI_PARSE_ERROR_LEN,
							  (struct sockaddr* )&reply_addr, reply_addr_len,
							  mi_socket_timeout);
				free_async_handler(hdl);
				continue;
			}
			mi_cmd->async_hdl = hdl;
		}

		DBG("DBG:datagram_fnc:mi_datagram_server:done parsing "
			"the mi tree\n");
		if ( (mi_rpl=run_mi_cmd(f, mi_cmd))==0 ){
		/*error while running the command*/
			LOG(L_ERR, "ERROR:datagram_fnc:mi_datagram_server: command "
				"processing failed\n");
			mi_send_dgram(tx_sock, MI_COMMAND_FAILED, MI_COMMAND_FAILED_LEN,
							(struct sockaddr* )&reply_addr, reply_addr_len, 
							mi_socket_timeout);
			goto failure;
		}

		/*the command exited well*/
		DBG("DBG: mi_datagram:mi_datagram_server: "
			"command process (%s)succeded\n",f->name.s); 

		if (mi_rpl!=MI_ROOT_ASYNC_RPL) {
			if(mi_datagram_write_tree(&dtgram , mi_rpl) != 0){
				LOG(L_ERR, "ERROR mi_datagram:mi_datagram_server: error"
					"while building the response \n");	
				goto failure;
			}

			len = dtgram.current - dtgram.start;
			ret = mi_send_dgram(tx_sock, dtgram.start,len, 
							(struct sockaddr* )&reply_addr, 
							reply_addr_len, mi_socket_timeout);
			if (ret>0){
				DBG("DEBUG mi_datagram:mi_datagram_server : the "
					"response: %s has been sent in %i octets\n", 
					dtgram.start, ret);
			}else{
				LOG(L_ERR, "ERROR midatagram:mi_datagram_server: an error "
					"occured while sending the response\n");
			}
			free_mi_tree( mi_rpl );
			free_async_handler(hdl);
			if (mi_cmd) free_mi_tree( mi_cmd );
		}else {	
			if (mi_cmd) free_mi_tree( mi_cmd );
		}

		continue;

failure:
		free_async_handler(hdl);
		/* destroy request tree */
		if (mi_cmd) free_mi_tree( mi_cmd );
		/* destroy the reply tree */
		if (mi_rpl) free_mi_tree(mi_rpl);
		continue;
	}
}

