/*
 * $Id$
 *
 * Copyright (C) 2002-2003 FhG Fokus
 *
 * This file is sipsak, a free sip testing tool.
 *
 * sipsak is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * sipsak is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


/* sipsak written by nils ohlmeier (develop@ohlmeier.de).
   based up on a modified version of shoot.
   set DEBUG on compile will produce much more output. primarily
   it will print out the sent and received messages before or after
   every network action.
*/

/* changes by jiri@iptel.org; now messages can be really received;
   status code returned is 2 for some local errors , 0 for success
   and 1 for remote error -- ICMP/timeout; can be used to test if
   a server is alive; 1xx messages are now ignored; windows support
   dropped
*/

/*
shot written by ashhar farhan, is not bound by any licensing at all.
you are free to use this code as you deem fit. just dont blame the author
for any problems you may have using it.
bouquets and brickbats to farhan@hotfoon.com
*/

/* TO-DO:
   - support for short notation
   - support for IPv6
*/

//set ts=4 :-)

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include <regex.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#include <errno.h>

/* this is the main function with the loops and modes */
void shoot()
{
	struct sockaddr_in	addr, sockname;
	struct timeval	tv, sendtime, recvtime, firstsendt;
	struct timezone tz;
	struct pollfd sockerr;
	int ssock, redirected, retryAfter;
	int sock, i, len, ret, usrlocstep, randretrys;
	int dontsend, cseqcmp, cseqtmp;
	int rem_rand, rem_namebeg;
	char *contact, *crlf, *foo, *bar;
	fd_set	fd;
	socklen_t slen;
	regex_t redexp, proexp, okexp, tmhexp, errexp;
	int bsd_compat, opt_size;

	int nretries=3;
	char *buff="MiniTest";


	/* create a sending socket */
	sock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock==-1) {
		perror("no client socket");
		exit(2);
	}


#ifndef _NO_LISTENER
    /* create a listening socket */
    ssock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ssock==-1) {
        perror("no server socket");
        exit(2);
    }

    sockname.sin_family=AF_INET;
    sockname.sin_addr.s_addr = htonl( INADDR_ANY );
    sockname.sin_port = htons((short)47437);
    if (bind( ssock, (struct sockaddr *) &sockname, sizeof(sockname) )==-1) {
        perror("no bind");
        exit(2);
    }
#endif


	/* destination socket init here because it could be changed in a 
	   case of a redirect */
	addr.sin_addr.s_addr = inet_addr("192.168.99.100");
	addr.sin_port = htons((short)888);
	addr.sin_family = AF_INET;

	/* if we dont connect, even on Linux, nothing will happen */

#ifdef CONNECTED	
	/* we connect as per the RFC 2543 recommendations
	   modified from sendto/recvfrom */
	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret==-1) {
		perror("no connect");
		exit(2);
	}
#endif

	if (getsockopt( sock, SOL_SOCKET, SO_BSDCOMPAT, &bsd_compat, &opt_size )==-1) {
		perror("ERROR");
		exit(1);
	}
	printf("BSD compat: %d\n", bsd_compat);

	/* here we go for the number of nretries which heavily depends on the 
	   mode */
	for (i = 0; i <= nretries; i++)
	{
		/* lets fire the request to the server and store when we did */

		/* if we send too fast, ICMP will arrive back when we are already
		   done and we wont be able to recognize an error
		*/
#ifdef CONNECTED
		ret = send(sock, buff, strlen(buff), 0);
#else
		ret=sendto(sock, buff, strlen(buff), 0, (struct sockaddr *)&addr, sizeof(addr));
#endif
		/* wait 1/10 sec to be safe we receive ICMP */
		usleep(100000);
		if (ret==-1) {
			perror("send failure");
			exit( 1 );
		}
	}

	exit(0);
}

int main(int argc, char *argv[])
{
	shoot();
}

