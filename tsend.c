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
 * send with timeout for stream and datagram sockets
 * 
 * History:
 * --------
 *  2004-02-26  created by andrei
 */

#include <string.h>
#include <errno.h>
#include <sys/poll.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "dprint.h"


/* sends on fd (which must be O_NONBLOCK); if it cannot send any data
 * in timeout miliseconds it will return ERROR
 * returns: -1 on error, or number of bytes written
 *  (if less than len => couldn't send all)
 *  bugs: signals will reset the timer
 */
int tsend_stream(int fd, char* buf, unsigned int len, int timeout)
{
	int n;
	int written;
	int initial_len;
	struct pollfd pf;
	
	written=0;
	initial_len=len;
	pf.fd=fd;
	pf.events=POLLIN;
	
again:
	n=send(fd, buf, len,
#ifdef HAVE_MSG_NOSIGNAL
			MSG_NOSIGNAL
#else
			0
#endif
		);
	if (n<0){
		if (errno==EINTR) goto again;
		else if (errno!=EAGAIN && errno!=EWOULDBLOCK){
			LOG(L_ERR, "ERROR: tsend_stream: failed to send: (%d) %s\n",
						errno, strerror(errno));
			goto error;
		}
	}
	written+=n;
	if (n<len){
		/* partial write */
		buf+=n;
		len-=n;
	}else{
		/* succesfull full write */
		goto end;
	}
	while(1){
		pf.revents=0;
		n=poll(&pf, 1, timeout);
		if (n<0){
			if (errno==EINTR) continue; /* signal, ignore */
			LOG(L_ERR, "ERROR: tsend_stream: poll failed: %s [%d]\n",
					strerror(errno), errno);
			goto error;
		}else if (n==0){
			/* timeout */
			LOG(L_ERR, "ERROR: tsend_stream: send timeout (%d)\n", timeout);
			goto error;
		}
		if (pf.revents&POLLIN){
			/* we can write again */
			goto again;
		}
	}
error:
	return -1;
end:
	return written;
}



/* sends on dram fd (which must be O_NONBLOCK); if it cannot send any data
 * in timeout miliseconds it will return ERROR
 * returns: -1 on error, or number of bytes written
 *  (if less than len => couldn't send all)
 *  bugs: signals will reset the timer
 */
int tsend_dgram(int fd, char* buf, unsigned int len, int timeout,
				const struct sockaddr* to, socklen_t tolen)
{
	int n;
	int written;
	int initial_len;
	struct pollfd pf;
	
	written=0;
	initial_len=len;
	pf.fd=fd;
	pf.events=POLLIN;
	
again:
	n=sendto(fd, buf, len, 0, to, tolen);
	if (n<0){
		if (errno==EINTR) goto again;
		else if (errno!=EAGAIN && errno!=EWOULDBLOCK){
			LOG(L_ERR, "ERROR: tsend_dgram: failed to send: (%d) %s\n",
						errno, strerror(errno));
			goto error;
		}
	}
	written+=n;
	if (n<len){
		/* partial write */
		LOG(L_CRIT, "BUG: tsend_dgram: partial write on datagram socket\n");
		goto error;
	}else{
		/* succesfull full write */
		goto end;
	}
	while(1){
		pf.revents=0;
		n=poll(&pf, 1, timeout);
		if (n<0){
			if (errno==EINTR) continue; /* signal, ignore */
			LOG(L_ERR, "ERROR: tsend_dgram: poll failed: %s [%d]\n",
					strerror(errno), errno);
			goto error;
		}else if (n==0){
			/* timeout */
			LOG(L_ERR, "ERROR: tsend_dgram: send timeout (%d)\n", timeout);
			goto error;
		}
		if (pf.revents&POLLIN){
			/* we can write again */
			goto again;
		}
	}
error:
	return -1;
end:
	return written;
}

	
