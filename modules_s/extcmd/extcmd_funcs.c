/*
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "extcmd_funcs.h"
#include "clients.h"
#include "../../error.h"
#include "../../str.h"
#include "../../ip_addr.h"
#include "../../data_lump_rpl.h"
#include "../../mem/shm_mem.h"
#include "../../parser/parse_content.h"
#include "../../parser/parse_from.h"
#include "../tm/tm_load.h"


#define NO_CMD   0
#define MSG_CMD  1
#define RPL_CMD  2
#define BYE_CMD  3
#define FTH_CMD  4
#define VOID_CMD 5
#define INV_CMD  6
#define ACK_CMD  7

#define BUFFER_SIZE  2048
#define HEADER_SIZE  5
#define TYPE_LEN     1
#define LEN_LEN      4

#define append_str(_p,_s,_l) \
	{memcpy((_p),(_s),(_l));\
	(_p) += (_l);}


typedef struct anchor_struct
{
	int fd;
	str cmd;
} anchor_t;


/* global variable */
struct tm_binds tmb;
int    rpl_pipe[2];
int    req_pipe[2];



inline int get_int_n2h( char *b, int l)
{
	int n,i;
	for(i=0,n=0;i<l;i++)
		n = (n<<8) + (unsigned char)b[i];
	return n;
}




inline void put_int_h2n( int x, char *b, int l)
{
	int i;
	for(i=0;i<l;i++)
		b[i] = (unsigned char) ((x>>((l-i-1)*8)) & 0x000000FF);
}




inline int extcmd_add_contact(struct sip_msg* msg , str* to_uri)
{
	struct lump_rpl *lump;
	char *buf, *p;
	int len;

	len = 9 /*"Contact: "*/ + to_uri->len + 2/*"<>"*/ + CRLF_LEN;

	buf = pkg_malloc( len );
	if(!buf) {
		LOG(L_ERR,"ERROR:extcmd_add_contact: out of memory! \n");
		return -1;
	}

	p = buf;
	append_str( p, "Contact: " , 9);
	*(p++) = '<';
	append_str( p, to_uri->s, to_uri->len);
	*(p++) = '>';
	append_str( p, CRLF, CRLF_LEN);

	lump = build_lump_rpl( buf , len );
	if(!lump) {
		LOG(L_ERR,"ERROR:extcmd_add_contact: unable to build lump_rpl! \n");
		pkg_free( buf );
		return -1;
	}
	add_lump_rpl( msg , lump );

	pkg_free(buf);
	return 1;
}




int dump_request(struct sip_msg *msg, char *para1, char *para2)
{
	anchor_t anchor;
	str    body;
	struct to_body  *from;
	char   *cmd;
	int    cmd_len;
	char   *p;

	/* get th content-type and content-length headers */
	if (parse_headers( msg, HDR_CONTENTLENGTH|HDR_CONTENTTYPE, 0)==-1
	|| !msg->content_type || !msg->content_length) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: fetching content-lenght and "
			"content_type failed! -> parse error or headers missing!\n");
		goto error;
	}

	/* check the content-type value */
	if ( (int)msg->content_type->parsed!=CONTENT_TYPE_TEXT_PLAIN
	&& (int)msg->content_type->parsed!=CONTENT_TYPE_MESSAGE_CPIM ) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: invalid content-type for a "
			"message request! type found=%d\n",(int)msg->content_type->parsed);
		goto error;
	}

	/* get the message's body */
	body.s = get_body( msg );
	if (body.s==0) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: cannot extract body from msg!\n");
		goto error;
	}
	body.len = (int)msg->content_length->parsed;

	if (!msg->to) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: no TO header found!\n");
		goto error;
	}

	if ( parse_from_header(msg)==-1 ) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: cannot get FROM header!\n");
		goto error;
	}
	from = (struct to_body*)msg->from->parsed;

	/* adds contact header into reply */
	if (extcmd_add_contact(msg,&(get_to(msg)->uri))==-1) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: can't build contact for reply\n");
		goto error;
	}

	/*-------------BUILD AND FILL THE COMMAND --------------------*/
	/* computes the amount of memory needed */
	cmd_len = HEADER_SIZE + /* commnad header */
		+ 4 + body.len /*body + size */
		+ 4 + from->uri.len /* from + size */
		+ 4 + get_to(msg)->uri.len /* to + size */
		+ 4 + 0; /* extra_headers + size */
	/* allocs a chunk of shm memory */
	cmd = (char*)shm_malloc( cmd_len );
	if (!cmd) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: cannot get shm memory!\n");
		goto error;
	}

	/* start filling the commnad (in revert order, from end)*/
	p = cmd + cmd_len;
	/* copy body's body */
	p = p - body.len;
	memcpy( p, body.s, body.len);
	/* put body's lengh */
	p = p-4;
	put_int_h2n( body.len, p, 4);
	/* copy hdr's body - nothing for the moment*/
	/* put hdr's lengh */
	p = p-4;
	put_int_h2n( 0, p, 4);
	/* copy to's body */
	p = p - get_to(msg)->uri.len;
	memcpy( p, get_to(msg)->uri.s, get_to(msg)->uri.len);
	/* put to's lengh */
	p = p-4;
	put_int_h2n( get_to(msg)->uri.len, p, 4);
	/* copy from's body */
	p = p - from->uri.len;
	memcpy( p, from->uri.s, from->uri.len);
	/* put from's lengh */
	p = p-4;
	put_int_h2n( from->uri.len, p, 4);
	/* add the cmd size */
	p = p-4;
	put_int_h2n( cmd_len-HEADER_SIZE, p, 4);
	/* add the cmd type */
	p = p-1;
	put_int_h2n( MSG_CMD, p, 1);


	/* fill in the anchor */
	anchor.fd = 0;
	anchor.cmd.s = cmd;
	anchor.cmd.len = cmd_len;
	/* send the anchor throught pipe to the extcmd server process */
	DBG("DEBUG:extcmd:dump_msg: posting request in pipe!\n");
	if (write( req_pipe[1], &anchor, sizeof(anchor))!=sizeof(anchor) ) {
		LOG(L_ERR,"ERROR:extcmd:dump_msg: cannot write to request pipe"
			" : %s\n",strerror(errno) );
		goto error;
	}

	//if (buf) pkg_free(buf);
	return 1;
error:
	//if (buf) pkg_free(buf);
	return -1;
}




inline int push_reply_to_client(int code, char* reason_s, int reason_l,
																int client_fd)
{
	anchor_t anchor;
	str reply;
	char *p;

	/* compose the reply commnad theat will be sent back to the client */
	reply.len = 1/*type*/+4/*cmd_size*/+4/*rpl_code*/+4/*rpl_reason_size*/+
		reason_l/*rpl_reason_string*/;
	reply.s = (char*) shm_malloc( reply.len );
	if (reply.s==0) {
		LOG(L_ERR,"ERROR::extcmd:tuac_callback: no more shm_mem free!\n");
		return -1;
	}
	/* fill up the rpl_cmd - in revert order (from end)*/
	p = reply.s + reply.len;
	/* add reason string */
	p = p-reason_l;
	memcpy( p, reason_s, reason_l);
	/* add reason string len */
	p = p-4;
	put_int_h2n( reason_l, p, 4);
	/* add the rpl code */
	p = p-4;
	put_int_h2n( code, p, 4);
	/* add the cmd size */
	p = p-4;
	put_int_h2n( 4+4+reason_l, p, 4);
	/* add the cmd type */
	p = p-1;
	put_int_h2n( RPL_CMD, p, 1);

	/* push reply on rpl_pipe */
	anchor.fd = client_fd;
	anchor.cmd.s = reply.s;
	anchor.cmd.len = reply.len;
	if (write( rpl_pipe[1] , &anchor, sizeof(anchor) )!=sizeof(anchor)) {
		LOG(L_ERR,"ERROR::extcmd:push_reply_to_client: cannot write "
			"to rpl_pipe : Reason: %s !\n",strerror(errno) );
		return -1;
	}
	return 1;
}




void tuac_callback( struct cell *t, struct sip_msg *msg, int code, void *param)
{
	str  reason;

	DBG("DEBUG:extcmd:tuac_callback: reply status=%d\n", code);
	if(!t->cbp)
	{
		LOG(L_ERR,"ERROR:extcmd:tuac_callback: parameter not received\n");
		return;
	}

	/* get the status text for this reply code */
	get_reply_status( &reason, msg, code);

	push_reply_to_client( code, reason.s, reason.len, *((int*)t->cbp));
	if (reason.s)
		pkg_free(reason.s);
}




int send_sip_req(str* msg_type, str *msg, int client_fd)
{
	char err_buf[256];
	str to;
	str from;
	str hdrs;
	str body;
	int *pcbp;
	char *p;
	char *end;
	int len;
	int ret;
	int err_ret;
	int sip_error;

	/* split the msg into from, to hdrs, body */
	end = msg->s + msg->len;
	p = msg->s;

	/* get from len */
	len = get_int_n2h( p, 4);
	p = p + 4;
	if ( p+len>end) {
		LOG(L_ERR,"ERROR:extcmd:send_sip_req: FROM size to big (%d)\n",len);
		push_reply_to_client(400,"extcmd: invalid FROM header",27,client_fd);
		goto error;
	}
	/* get from body */
	from.s = p;
	from.len = len;
	p += len;
	DBG("DEBUG:extcmd:send_sip_req: from=%d<%.*s>\n",from.len,from.len,from.s);

	/* get to len */
	len = get_int_n2h( p, 4);
	p = p + 4;
	if ( p+len>end) {
		LOG(L_ERR,"ERROR:extcmd:send_sip_req: TO size to big (%d)\n",len);
		push_reply_to_client(400,"extcmd: invalid TO header",25,client_fd);
		goto error;
	}
	/* get to body */
	to.s = p;
	to.len = len;
	p += len;
	DBG("DEBUG:extcmd:send_sip_req: to=%d<%.*s>\n",to.len,to.len,to.s);

	/* get headers len */
	len = get_int_n2h( p, 4);
	p = p + 4;
	if ( p+len>end) {
		LOG(L_ERR,"ERROR:extcmd:send_sip_req: HDRS size to big (%d)\n",len);
		push_reply_to_client(400,"extcmd: invalid HEADERS field",29,client_fd);
		goto error;
	}
	/* get headers body */
	hdrs.s = p;
	hdrs.len = len;
	p += len;
	DBG("DEBUG:extcmd:send_sip_req: hdrs=%d<%.*s>\n",hdrs.len,hdrs.len,hdrs.s);

	/* get body len */
	len = get_int_n2h( p, 4);
	p = p + 4;
	if ( p+len>end) {
		LOG(L_ERR,"ERROR:extcmd:send_sip_req: BODY size to big (%d)\n",len);
		push_reply_to_client(400,"extcmd: invalid BODY field",26,client_fd);
		goto error;
	}
	/* get body's body */
	body.s = p;
	body.len = len;
	DBG("DEBUG:extcmd:send_sip_req: body=%d<%.*s>\n",body.len,body.len,body.s);

	/* allocate the param in shm */
	if( !(pcbp = (int*)shm_malloc(sizeof(int*))) ) {
		LOG(L_ERR,"ERROR:extcmd:send_sip_req: no more shm mem free!!\n");
		push_reply_to_client(500,"extcmd: no shm memory free",26,client_fd);
		goto error;
	}
	*pcbp = client_fd;

	/* send the message */
	ret = tmb.t_uac( msg_type, &to, &hdrs, &body, &from,
		tuac_callback, (void*)pcbp, 0);
	if (ret<=0) {
		err_ret=err2reason_phrase(ret, &sip_error, err_buf,
				sizeof(err_buf), "EXTCMD" ) ;
		if (err_ret > 0 )
		{
			push_reply_to_client( sip_error, err_buf, err_ret, client_fd);
		} else {
			push_reply_to_client(500,"EXTCMD unknown error\n",20,client_fd);
		}
	}
	return 1;

error:
	return -1;
}




inline int forward_reply_to_client(int pipe_fd)
{
	anchor_t anchor;

	anchor.cmd.s = 0;
	anchor.cmd.len = 0;

	/* get cmd from pipe */
	if (read(pipe_fd,&anchor,sizeof(anchor))!=sizeof(anchor) ) {
		LOG(L_ERR,"ERROR:extcmd:send_cmd_from_pipe_to_client: cannot read from"
			" pipe: %s \n", strerror(errno));
		goto error;
	}

	/* send the reply */
	if (write( anchor.fd, anchor.cmd.s, anchor.cmd.len)!=anchor.cmd.len ) {
		LOG(L_ERR,"ERROR:extcmd:send_cmd_from_pipe_to_client: cannot write to "
			"socket: %s \n", strerror(errno));
		goto error;
	}

	/* free the mem */
	shm_free( anchor.cmd.s );

	return 1;
error:
	if (anchor.cmd.s)
		shm_free( anchor.cmd.s );
	return -1;
}




inline int forward_request_to_all_clients(int pipe_fd)
{
	anchor_t anchor;
	client_t *client;
	int fd;
	int i;

	anchor.cmd.s = 0;
	anchor.cmd.len = 0;

	/* get request cmd from pipe */
	if (read(pipe_fd,&anchor,sizeof(anchor))!=sizeof(anchor) ) {
		LOG(L_ERR,"ERROR:extcmd:send_cmd_from_pipe_to_client: cannot read from"
			" pipe: %s \n", strerror(errno));
		goto error;
	}

	/* send the reply to all clients */
	for(i=0; i<get_nr_clients(); i++) {
		client = get_client( i );
		fd = client->fd;
		if (write( fd, anchor.cmd.s, anchor.cmd.len)!=anchor.cmd.len ) {
			LOG(L_ERR,"ERROR:extcmd:forward_request_to_all_client: cannot "
				"write to socket: %s \n", strerror(errno));
			continue;
		}
	}

	/* free the mem */
	shm_free( anchor.cmd.s );

	return 1;
error:
	if (anchor.cmd.s)
		shm_free( anchor.cmd.s );
	return -1;
}




inline int read_n( int fd, int n, char *b)
{
	int l;
	int c;

	l=0;
	c=0;
	while( l<n && (c=read(fd,b+l,n))>0 )
		l += c;
	if (c<0)
		return c;
	return l;
}




inline int get_cmd( int fd, str *cmd)
{
	static char buffer[BUFFER_SIZE];
	int cmd_type;
	int cmd_len;
	int  len;

	/* get command header */
	len = read_n( fd, HEADER_SIZE , buffer);
	if (len<HEADER_SIZE) {
		LOG(L_ERR,"ERROR:extcmd:get_cmd: cannot read command's header: %s \n",
			len<0?strerror(len):"incomplet string");
		return NO_CMD;
	}

	/* decode command header */
	cmd_type = get_int_n2h( buffer, TYPE_LEN);
	cmd_len = get_int_n2h( buffer+TYPE_LEN, LEN_LEN);
	DBG("DEBUG:extcmd:get_cmd: type=%d, len=%d \n",cmd_type, cmd_len);

	/* read the command body */
	len = read_n( fd, cmd_len , buffer);
	if (len<cmd_len) {
		LOG(L_ERR,"ERROR:extcmd:get_cmd: cannot read command's body: %s \n",
			len<0?strerror(len):"incomplet string");
		return NO_CMD;
	}
	DBG("DEBUG:extcmd:get_cmd: command = [%.*s]\n",cmd_len,buffer);

	/* return the command body */
	cmd->s = buffer;
	cmd->len = cmd_len;

	return cmd_type;
}




inline int send_void_command( int sock_fd )
{
	char buf[HEADER_SIZE];

	/* add command type */
	put_int_h2n( VOID_CMD, buf, 1);
	/* add the cmd len */
	put_int_h2n( 0, buf+1, 4);
	/* send the reply */
	if (write( sock_fd, buf, HEADER_SIZE)!=HEADER_SIZE ) {
		LOG(L_ERR,"ERROR:extcmd:send_void_commnad: cannot write to "
			"socket: %s \n", strerror(errno));
		return -1;
	}
	return 1;
}




inline void FD_SET_AND_MAX( fd_set *fdset, int fd, int *max_fd)
{
	FD_SET( fd, fdset );
	if (fd>*max_fd)
		*max_fd = fd;
}




inline void FD_CLR_AND_MAX( fd_set *fdset, int fd, int *max_fd)
{
	int i;
	int new_max = 0;

	FD_CLR( fd, fdset );
	if (fd==*max_fd) {
		for(i=0;i<=*max_fd;i++)
			if ( FD_ISSET(i,fdset) && i>new_max )
				new_max = i;
		*max_fd = new_max;
	}
}




void extcmd_server_process( int server_sock )
{
	str message_req = { "MESSAGE", 7};
	//str invite_req = { "INVITE", 6};
	fd_set read_set;
	fd_set wait_set;
	client_t *client;
	union sockaddr_union sau;
	int sau_len;
	str cmd;
	int cmd_type;
	int max_fd;
	int fd = -1;
	int index;

	/* set SIG_PIPE to be ignored by this proccess */
	if (signal( SIGPIPE, SIG_IGN)==SIG_ERR) {
		LOG(L_ERR,"ERROR:extcmd_server_process: cannot set SIGPIPE to be "
			"ignored! \n");
		goto error;
	}

	/* set the socket for listening */
	if ( listen( server_sock, 10)==-1 ) {
		LOG(L_ERR,"ERROR:extcmd_server_process: cannot listen to the server"
			" socket: %s \n", strerror(errno) );
		goto error;
	}

	sau_len = sizeof( union sockaddr_union );
	/* prepare fd set for select */
	FD_ZERO( &wait_set );
	max_fd = 0;
	FD_SET_AND_MAX( &wait_set, server_sock, &max_fd);
	FD_SET_AND_MAX( &wait_set, req_pipe[0], &max_fd);
	FD_SET_AND_MAX( &wait_set, rpl_pipe[0], &max_fd);

	while(1) {
		/* what fd should we listen to? */
		read_set = wait_set;
		if (clients_is_full())
			FD_CLR_AND_MAX( &read_set, server_sock, &max_fd);
		/* wait for read something */
		DBG("DEBUG:extcmd_server_process: waitting to read!\n");
		if (select( max_fd+1, &read_set, 0, 0, 0)==-1) {
			LOG(L_ERR,"ERROR:extcmd_server_process: select faild : %s\n",
				strerror(errno) );
			sleep(1);
			continue;
		}

		/* let's see what we read */
		/* maybe is a connect request !?*/
		if ( FD_ISSET(server_sock, &read_set) ) {
			fd=accept(server_sock,(struct sockaddr*)&sau,(socklen_t*)&sau_len);
			if (fd==-1) {
				LOG(L_ERR,"ERROR: extcmd_server_process: accept faild : %s\n",
					strerror(errno) );
			} else {
				DBG("DEBUG:extcmd_server_process: connextion accepted \n");
				add_client(fd);
				/* add this socket to wait_sock to read from it */
				FD_SET_AND_MAX( &wait_set, fd, &max_fd);
			}
		}

		/* maybe is a from req_pipe ?! */
		if ( FD_ISSET(req_pipe[0], &read_set) ) {
			forward_request_to_all_clients( req_pipe[0] );
		}

		/* maybe is a SIP reply from rpl_pipe ?! */
		if ( FD_ISSET(rpl_pipe[0], &read_set) ) {
			forward_reply_to_client( rpl_pipe[0] );
		}

		/* maybe is from a client -> look for it's fd */
		for( fd=-1,index=0 ; index<get_nr_clients() ; index++ ) {
			client = get_client(index);
			if ( !client || !FD_ISSET(client->fd,&read_set) )
				continue;
			/* we can read something from this client */
			fd = client->fd;
			/* get the command from the client */
			cmd_type = get_cmd(fd,&cmd);
			switch (cmd_type) {
				/* NO commnad - nothing was read - probably disconnection */
				case NO_CMD :
				/* BYE command - used by the client to end the connection */
				case BYE_CMD:
					DBG("DEBUG:extcmd_server_process: BYE received->close\n");
					/* remove clinet's fd from wait_sock set */
					FD_CLR_AND_MAX( &wait_set, fd, &max_fd);
					/* remove client */
					del_client(index);
					index--;
					/* close the connection */
					close(fd);
					break;
				/* MSG commnad - client wants to send a SIP message request */
				case MSG_CMD:
					DBG("DEBUG:extcmd_server_process: MSG received\n");
					/* send the message */
					send_sip_req( &message_req, &cmd, fd);
					break;
				/* unknown commnad was received - ignore it */
				default:
					LOG(L_ERR,"ERROR:extcmd_server_process: unknown command "
					"[%d] received from client (fd=%d)\n",cmd_type,fd);
			} /* end switch */
		} /* end for */
	}

error:
	return;
}

