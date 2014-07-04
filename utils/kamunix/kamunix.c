/*
 * $Id$
 *
 * Copyright (C) 2004 FhG FOKUS
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>


/* AF_LOCAL is not defined on solaris */
#if !defined(AF_LOCAL)
#define AF_LOCAL AF_UNIX
#endif
#if !defined(PF_LOCAL)
#define PF_LOCAL PF_UNIX
#endif


/* solaris doesn't have SUN_LEN */
#ifndef SUN_LEN
#define SUN_LEN(sa)	 ( strlen((sa)->sun_path) + \
					 (size_t)(((struct sockaddr_un*)0)->sun_path) )
#endif


#define BUF_SIZE 65536
#define DEFAULT_TIMEOUT 5

int main(int argc, char** argv)
{
	int sock, len;
	socklen_t from_len;
	struct sockaddr_un from, to;
	char name[256];
	static char buffer[BUF_SIZE];
	char *chroot_dir;

	if (argc != 2) {
		printf("Usage: %s <path_to_socket>\n", argv[0]);
		return 1;
	}

	sock = socket(PF_LOCAL, SOCK_DGRAM, 0);
	if (sock == -1) {
		fprintf(stderr, "Error while opening socket: %s\n", strerror(errno));
		return -1;
	}
	
	memset(&from, 0, sizeof(from));
	from.sun_family = PF_LOCAL;

	chroot_dir = getenv("CHROOT_DIR");
	if (chroot_dir == NULL)
		chroot_dir = "";
	sprintf(name, "%s/tmp/Kamailio.%d.XXXXXX", chroot_dir, getpid());
	umask(0); /* set mode to 0666 for when Kamailio is running as non-root user and kamctl is running as root */

	if (mkstemp(name) == -1) {
		fprintf(stderr, "Error in mkstemp with name=%s: %s\n", name, strerror(errno));
		return -2;
	}
	if (unlink(name) == -1) {
		fprintf(stderr, "Error in unlink of %s: %s\n", name, strerror(errno));
		return -2;
	}
	strncpy(from.sun_path, name, strlen(name));

	if (bind(sock, (struct sockaddr*)&from, SUN_LEN(&from)) == -1) {
		fprintf(stderr, "Error in bind: %s\n", strerror(errno));
		goto err;
	}

	memset(&to, 0, sizeof(to));
	to.sun_family = PF_LOCAL;
	strncpy(to.sun_path, argv[1], sizeof(to.sun_path) - 1);
	
	len = fread(buffer, 1, BUF_SIZE, stdin);

	if (len) {
		if (sendto(sock, buffer, len, 0, (struct sockaddr*)&to, SUN_LEN(&to)) == -1) {
			fprintf(stderr, "Error in sendto: %s\n", strerror(errno));
		        goto err;
		}
		from_len = sizeof(from);
		len = recvfrom(sock, buffer, BUF_SIZE, 0, (struct sockaddr*)&from, &from_len);
		if (len == -1) {
			fprintf(stderr, "Error in recvfrom: %s\n", strerror(errno));
			goto err;
		}

		fprintf(stdout, "%.*s", len, buffer);
	} else {
		fprintf(stderr, "Nothing to send\n");
		goto err;
	}

	close(sock);
	if (unlink(name) == -1)
		fprintf(stderr, "Error in unlink of %s: %s\n", name, strerror(errno));
	return 0;

 err:
	close(sock);
	if (unlink(name) == -1)
		fprintf(stderr, "Error in unlink of %s: %s\n", name, strerror(errno));
	return -1;
}
