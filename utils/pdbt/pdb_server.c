/*
 * Copyright (C) 2009 1&1 Internet AG
 *
 * This file is part of sip-router, a free SIP server.
 *
 * sip-router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * sip-router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pdb_server_backend.h"
#include "log.h"



#define NETBUFSIZE 200
#define DEFAULT_BINDADDR "0.0.0.0"
#define DEFAULT_PORT 5574




void print_usage(char *program) {
	set_log_level(LOG_INFO);
	LINFO("version: pdb_server %d\n", PDB_VERSION);
	LINFO("Listens on a UDP port for queries and sends answer UDP packets back.\n");
	LINFO("\n");
	LINFO("Usage: %s [<option>...]\n", program);
	LINFO("  %s -m <data file> [-i <bind addr>] [-p <port>] [-d <log level>]\n", program);
	LINFO("\n");
	LINFO("  Options:\n");
	LINFO("    -m <file>: Specifies the file containing the backend data.\n");
	LINFO("    -i <bind addr>: Specifies the address to bind the UDP socket to.\n");
	LINFO("                    Default is '%s'.\n", DEFAULT_BINDADDR);
	LINFO("    -p <port>: Specifies the port to listen at in udp_server mode.\n");
	LINFO("               Default is %ld.\n", (long int)DEFAULT_PORT);
	LINFO("    -d <debug level>: %ld for debug level.\n", LOG_DEBUG);
	LINFO("                      %ld for info level.\n", LOG_INFO);
	LINFO("                      %ld for notice level.\n", LOG_NOTICE);
	LINFO("                      %ld for warning level.\n", LOG_WARNING);
	LINFO("                      %ld for error level.\n", LOG_ERR);
	LINFO("                      %ld for critical level.\n", LOG_CRIT);
	LINFO("                      %ld for alert level.\n", LOG_ALERT);
	LINFO("                      %ld for emergency level.\n", LOG_EMERG);
	LINFO("                      %ld to disable all messages.\n", LOG_EMERG-1);
	LINFO("                      Default is warning level.\n");
	LINFO("    -v: Print the version\n");
	LINFO("    -h: Print this help.\n");
}

int pdb_msg_server_send(int so, char *buf, size_t answerlen, struct sockaddr *fromaddr, socklen_t fromaddrlen)
{
	ssize_t bytes_sent;
	int try = 0;
	again:
		bytes_sent = sendto(so, buf, answerlen, 0, fromaddr, fromaddrlen);
		if (bytes_sent < 3) {
			if ((errno == EINTR) && (try < 3)) {
				try++;
				LERR("sendto() failed - trying again. errno=%d (%s)\n", errno, strerror(errno));
				goto again;
			}
			LERR("sendto() failed with errno=%d (%s)\n", errno, strerror(errno));
			if ((errno==EAGAIN)||(errno==EINTR)||(errno==EWOULDBLOCK)) return 0;
			return -1;
		}
		if (bytes_sent != answerlen) {
			LERR("cannot send the whole answer (%ld/%ld).\n", (long int)bytes_sent, (long int)answerlen);
			return 0;
		}

    return 0;
}


/*
 Receive query request and send answer via UDP.
 UDP Payload of request must contain the phone number (only digits allowed).
 The Answer contains the number of the request followed by '\0' and the carrier id.
 Loops until a receive or send error occurs and returns -1.
 However, the following errors are ignored: EAGAIN, EINTR, EWOULDBLOCK.
*/
int udp_server(int so)
{
    struct pdb_msg msg;
	struct sockaddr fromaddr;
	socklen_t fromaddrlen;
	size_t answerlen = 0;
	ssize_t bytes_received;
	carrier_t carrierid;
	char buf[sizeof(struct pdb_msg)];
	int i;

	for (;;) {
		fromaddrlen = sizeof(fromaddr);
		bytes_received = recvfrom(so, buf, sizeof(struct pdb_msg), 0, &fromaddr, &fromaddrlen);
		if (bytes_received<0) {
            LERR("recvfrom() failed with errno=%d (%s)\n", errno, strerror(errno));
			if ((errno==EAGAIN)||(errno==EINTR)||(errno==EWOULDBLOCK)) continue;
			return -1;
		}

        switch (buf[0]) {
            case PDB_VERSION_1:
                /* get received bytes */
                memcpy(&msg, buf, bytes_received);
//                pdb_msg_dbg(msg);
                short int *_id = (short int *)&(msg.hdr.id); /* make gcc happy */
                msg.hdr.id = ntohs(*_id);

                i = 0;
                while (i < strlen(msg.bdy.payload)) {
                    if (msg.bdy.payload[i] < '0' || msg.bdy.payload[i] > '9') {
                        pdb_msg_format_send(&msg, PDB_VERSION_1, PDB_TYPE_REPLY_ID, PDB_CODE_NOT_NUMBER, htons(msg.hdr.id), NULL, 0);
                        goto msg_send;
                    }
                    i++;
                }
                /* lookup pdb_id */
                carrierid=lookup_number(msg.bdy.payload);

                /* check if not found pdb_id */
                if (carrierid == 0) {
                    pdb_msg_format_send(&msg, PDB_VERSION_1, PDB_TYPE_REPLY_ID, PDB_CODE_NOT_FOUND, htons(msg.hdr.id), NULL, 0);
                    goto msg_send;
                }

                /* convert to network byte order*/
                carrierid = htons(carrierid);

                /* prepare the message payload to be sent
                 * add the number string and append the carrier id
                 */
                memcpy(buf, msg.bdy.payload, msg.hdr.length - sizeof(msg.hdr));
                memcpy(buf + msg.hdr.length - sizeof(msg.hdr), &carrierid, sizeof(carrierid));

                /* all ok, send pdb_msg with pdb_id in payload */
                pdb_msg_format_send(&msg, PDB_VERSION_1, PDB_TYPE_REPLY_ID, PDB_CODE_OK, htons(msg.hdr.id), buf, msg.hdr.length - sizeof(msg.hdr) + sizeof(carrierid));
                goto msg_send;

                break;

            /* old pdb version; no pdb_msg used */
            default:
                /* take only digits */
                i=0;
                while ((i<bytes_received) && (buf[i]>='0') && (buf[i]<='9')) i++;
                buf[i]=0; /* terminate string */
                i++;

                /* lookup pdb_id */
                carrierid=lookup_number(buf);

                /* convert to network byte order*/
                carrierid=htons(carrierid);

                /* append carrier id to answer */
                memcpy(&(buf[i]), &carrierid, sizeof(carrierid));
                answerlen=i+sizeof(carrierid);
                goto buf_send;

                break;
        }

msg_send:
//        pdb_msg_dbg(msg);
        if (pdb_msg_server_send(so, (char*)&msg, msg.hdr.length, &fromaddr, fromaddrlen) < 0) {
            return -1;
        }
        continue;

buf_send:
        if (pdb_msg_server_send(so, buf, answerlen, &fromaddr, fromaddrlen)) {
            return -1;
        }
        continue;
    }

	return -1;
}




int main(int argc, char *argv[]) {
	int opt;
	char *backend_data_filename = NULL;
	char *bind_addr = DEFAULT_BINDADDR;
	unsigned short bind_port = DEFAULT_PORT;
	int use_syslog = 0;
	int log_level=LOG_WARNING;

	long int ret;

	int so;
	struct sockaddr_in sa;
		
	while ((opt = getopt(argc, argv, "m:i:p:vhdl:")) != -1) {
		switch (opt) {
		case 'm':
			backend_data_filename = optarg;
			break;
		case 'i':
			bind_addr=optarg;
			break;
		case 'p':
			ret=strtol(optarg, NULL, 10);
			if ((ret<0) || (ret>65535)) {
				init_log("pdb_server", use_syslog);
				LERR("invalid port '%s' specified.\n", optarg);
				return -1;
			}
			bind_port=ret;
			break;
		case 'v':
			set_log_level(LOG_INFO);
			LINFO("version: pdb_server %d\n", PDB_VERSION);
			return 0;
			break;
		case 'h':
			init_log("pdb_server", use_syslog);
			print_usage(argv[0]);
			return 0;
			break;
		case 'd':
			use_syslog=1;
			break;
		case 'l':
			ret=strtol(optarg, NULL, 10);
			if ((ret<LOG_EMERG-1) || (ret>LOG_DEBUG)) {
				init_log("pdb_server", use_syslog);
				LERR("invalid log level '%s' specified.\n", optarg);
				return -1;
			}
			log_level=ret;
			break;
		default:
			init_log("pdb_server", use_syslog);
			LERR("invalid option '%c'.\n", opt);
			print_usage(argv[0]);
			return 1;
		}
	}

	init_log("pdb_server", use_syslog);
	set_log_level(log_level);

	if (backend_data_filename==NULL) {
		LERR("no data file specified.\n");
		return 1;
	}

	if (init_backend(backend_data_filename)<0) {
		LERR("cannot initialize backend.\n");
		return -1;
	}
	
	so = socket(AF_INET, SOCK_DGRAM, 0);
	if (so<0) {
		LERR("socket() failed with errno=%d (%s)\n", errno, strerror(errno));
		return -1;
	}
	
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(bind_port);
	if (inet_aton(bind_addr, &(sa.sin_addr))==0) {
		LERR("invalid address '%s'.\n", bind_addr);
		close(so);
		return -1;
	}
	
	if (bind(so, (struct sockaddr *) &sa, sizeof(sa))<0) {
		LERR("bind() failed with errno=%d (%s)\n", errno, strerror(errno));
		close(so);
		return -1;
	}
	
	udp_server(so);
	close(so);

	return 0;
}
