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

#ifdef USE_TCP

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdlib.h> /* for NULL definition on openbsd */

#include "dprint.h"


/* at least 1 byte must be sent! */
int send_fd(int unix_socket, void* data, int data_len, int fd)
{
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr* cmsg;
	int ret;
	union {
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(fd))];
	}control_un;
	
	msg.msg_control=control_un.control;
	msg.msg_controllen=sizeof(control_un.control);
	
	msg.msg_name=0;
	msg.msg_namelen=0;
	
	iov[0].iov_base=data;
	iov[0].iov_len=data_len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	
	cmsg=CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
	*(int*)CMSG_DATA(cmsg)=fd;
	msg.msg_flags=0;
	
	ret=sendmsg(unix_socket, &msg, 0);
	
	return ret;
}



int receive_fd(int unix_socket, void* data, int data_len, int* fd)
{
	struct msghdr msg;
	struct iovec iov[1];
	struct cmsghdr* cmsg;
	int new_fd;
	int ret;
	union{
		struct cmsghdr cm;
		char control[CMSG_SPACE(sizeof(new_fd))];
	}control_un;
	
	msg.msg_control=control_un.control;
	msg.msg_controllen=sizeof(control_un.control);
	
	msg.msg_name=0;
	msg.msg_namelen=0;
	
	iov[0].iov_base=data;
	iov[0].iov_len=data_len;
	msg.msg_iov=iov;
	msg.msg_iovlen=1;
	
	ret=recvmsg(unix_socket, &msg, 0);
	if (ret<=0) goto error;
	
	cmsg=CMSG_FIRSTHDR(&msg);
	if ((cmsg!=0) && (cmsg->cmsg_len==CMSG_LEN(sizeof(new_fd)))){
		if (cmsg->cmsg_type!= SCM_RIGHTS){
			LOG(L_ERR, "receive_fd: msg control type != SCM_RIGHTS\n");
			ret=-1;
			goto error;
		}
		if (cmsg->cmsg_level!= SOL_SOCKET){
			LOG(L_ERR, "receive_fd: msg level != SOL_SOCKET\n");
			ret=-1;
			goto error;
		}
		*fd=*((int*) CMSG_DATA(cmsg));
	}else{
		LOG(L_ERR, "receive_fd: no descriptor passed, cmsg=%p, len=%d\n",
				cmsg, cmsg->cmsg_len);
		*fd=-1;
		/* it's not really an error */
	}
	
error:
	return ret;
}

#endif
