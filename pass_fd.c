/*
 * $Id$
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
 /*
  * History:
  * --------
  *  2002-11-29  created by andrei
  *  2003-02-20  added solaris support (! HAVE_MSGHDR_MSG_CONTROL) (andrei)
  *  2003-11-03  added send_all, recv_all  and updated send/get_fd
  *               to handle signals  (andrei)
  */

#ifdef USE_TCP

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdlib.h> /* for NULL definition on openbsd */
#include <errno.h>
#include <string.h>

#include "dprint.h"



/* receive all the data or returns error (handles EINTR etc.)
 * returns: bytes read or error (<0)
 * can return < data_len if EOF */
int recv_all(int socket, void* data, int data_len)
{
	int b_read;
	int n;
	
	b_read=0;
	do{
		n=recv(socket, (char*)data+b_read, data_len-b_read, MSG_WAITALL);
		if (n<0){
			/* error */
			if (errno==EINTR) continue; /* signal, try again */
			LOG(L_CRIT, "ERROR: recv_all: recv on %d failed: %s\n",
					socket, strerror(errno));
			return n;
		}
		b_read+=n;
	}while( (b_read!=data_len) && (n));
	return b_read;
}


/* sends all data (takes care of signals) (assumes blocking fd)
 * returns number of bytes sent or < 0 for an error */
int send_all(int socket, void* data, int data_len)
{
	int n;
	
again:
	n=send(socket, data, data_len, 0);
	if (n<0){
			/* error */
		if (errno==EINTR) goto again; /* signal, try again */
		LOG(L_CRIT, "ERROR: send_all: send on %d failed: %s\n",
					socket, strerror(errno));
	}
	return n;
}


/* at least 1 byte must be sent! */
int send_fd(int unix_socket, void* data, int data_len, int fd)
{
	struct msghdr msg;
	struct iovec iov[1];
	int ret;
#ifdef HAVE_MSGHDR_MSG_CONTROL
	struct cmsghdr* cmsg;
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(fd))];
	}control_un;
	
	msg.msg_control=control_un.control;
	msg.msg_controllen=sizeof(control_un.control);
	
	cmsg=CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	*(int*)CMSG_DATA(cmsg)=fd;
	msg.msg_flags=0;
#else
	msg.msg_accrights=(caddr_t) &fd;
	msg.msg_accrightslen=sizeof(fd);
#endif
	
	msg.msg_name=0;
	msg.msg_namelen=0;
	
	iov[0].iov_base=data;
	iov[0].iov_len=data_len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	
again:
	ret=sendmsg(unix_socket, &msg, 0);
	if (ret<0){
		if (errno==EINTR) goto again;
		LOG(L_CRIT, "ERROR: send_fd: sendmsg failed on %d: %s\n",
				unix_socket, strerror(errno));
	}
	
	return ret;
}



int receive_fd(int unix_socket, void* data, int data_len, int* fd)
{
	struct msghdr msg;
	struct iovec iov[1];
	int new_fd;
	int ret;
	int n;
#ifdef HAVE_MSGHDR_MSG_CONTROL
	struct cmsghdr* cmsg;
	union{
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(new_fd))];
	}control_un;
	
	msg.msg_control=control_un.control;
	msg.msg_controllen=sizeof(control_un.control);
#else
	msg.msg_accrights=(caddr_t) &new_fd;
	msg.msg_accrightslen=sizeof(int);
#endif
	
	msg.msg_name=0;
	msg.msg_namelen=0;
	
	iov[0].iov_base=data;
	iov[0].iov_len=data_len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	
again:
	ret=recvmsg(unix_socket, &msg, MSG_WAITALL);
	if (ret<0){
		if (errno==EINTR) goto again;
		LOG(L_CRIT, "ERROR: receive_fd: recvmsg on %d failed: %s\n",
				unix_socket, strerror(errno));
		goto error;
	}
	if (ret==0){
		/* EOF */
		LOG(L_CRIT, "ERROR: receive_fd: EOF on %d\n", unix_socket);
		goto error;
	}
	if (ret<data_len){
		LOG(L_WARN, "WARNING: receive_fd: too few bytes read (%d from %d)"
				    "trying to fix...\n", ret, data_len);
		n=recv_all(unix_socket, (char*)data+ret, data_len-ret);
		if (n>=0) ret+=n;
		else{
			ret=n;
			goto error;
		}
	}
	
#ifdef HAVE_MSGHDR_MSG_CONTROL
	cmsg=CMSG_FIRSTHDR(&msg);
	if ((cmsg!=0) && (cmsg->cmsg_len==CMSG_LEN(sizeof(new_fd)))){
		if (cmsg->cmsg_type!= SCM_RIGHTS){
			LOG(L_ERR, "ERROR: receive_fd: msg control type != SCM_RIGHTS\n");
			ret=-1;
			goto error;
		}
		if (cmsg->cmsg_level!= SOL_SOCKET){
			LOG(L_ERR, "ERROR: receive_fd: msg level != SOL_SOCKET\n");
			ret=-1;
			goto error;
		}
		*fd=*((int*) CMSG_DATA(cmsg));
	}else{
		LOG(L_ERR, "ERROR: receive_fd: no descriptor passed, cmsg=%p,"
				"len=%d\n", cmsg, cmsg->cmsg_len);
		*fd=-1;
		/* it's not really an error */
	}
#else
	if (msg.msg_accrightslen==sizeof(int)){
		*fd=new_fd;
	}else{
		LOG(L_ERR, "ERROR: receive_fd: no descriptor passed,"
				" accrightslen=%d\n", msg.msg_accrightslen);
		*fd=-1;
	}
#endif
	
error:
	return ret;
}
#endif
