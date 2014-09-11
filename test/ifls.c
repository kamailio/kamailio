/* $Id$
 *
 *
 * test programs, list all interfaces and their ip address
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
 /*
  * History:
  * --------
  *  2002-09-09  created by andrei
  */



#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#ifdef __sun__
#include <sys/sockio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define FLAGS 1
#define IF_DOWN 2
#define IF_UP   4


static char* version="ifls 0.1";
static char* id="$Id$";
static char* help_msg="\
Usage: ifls [-6hV} [interface...]\n\
(if no interface name is specified it will list all the interfaces)\n\
Options:\n\
    -a      list both ipv4 and ipv6 interfaces (default)\n\
    -4      list only ipv4 interfaces\n\
    -6      list only ipv6 interfaces\n\
    -f      show also the interface flags\n\
    -U      brings all the matching interfaces up\n\
    -D      brings all the matching interfaces down\n\
    -V      version number\n\
    -h      this help message\n\
";


#define MAX(a,b) ( ((a)>(b))?(a):(b))



void print_sockaddr(struct sockaddr* sa)
{
	unsigned char* buf;
	int r;
	
	switch(sa->sa_family){
	case AF_INET:
		buf=(char*)&(((struct sockaddr_in*)sa)->sin_addr).s_addr;
		printf("%d.%d.%d.%d\n", buf[0], buf[1], buf[2], buf[3]);
		break;
	case AF_INET6:
		buf=(((struct sockaddr_in6*)sa)->sin6_addr).s6_addr;
		for(r=0; r<16; r++) 
			printf("%02x%s", buf[r], ((r%2)&&(r!=15))?":":"" );
		printf("\n");
		break;
	default:
		printf("unknown af %d\n", sa->sa_family);
#ifdef __FreeBSD__
		for (r=0; r<sa->sa_len; r++) 
			printf("%02x ", ((unsigned char*)sa)[r]);
		printf("\n");
#endif
	}
}



int ls_ifflags(char* name, int family , int options)
{
	struct ifreq ifr;
	int s;
	
	memset(&ifr, 0, sizeof(ifr)); /* init to 0 (check if filled)*/
	s=socket(family, SOCK_DGRAM, 0);
	strncpy(ifr.ifr_name, name, IFNAMSIZ);
#if 0	
	if (ioctl(s, SIOCGIFADDR, &ifr)==-1){
		if(errno==EBADF) return 0; /* invalid descriptor => no address*/
		fprintf(stderr, "ls_if: ioctl for %s failed: %s\n", name, 
					strerror(errno));
		goto error;
	};
	
	printf("%s:\n", ifr.ifr_name);
	printf("        dbg: family=%d", ifr.ifr_addr.sa_family);
#ifdef __FreeBSD__
	printf(", len=%d\n", ifr.ifr_addr.sa_len);
#else
	printf("\n");
#endif
	if (ifr.ifr_addr.sa_family==0){
		printf("ls_if: OS BUG: SIOCGIFADDR doesn't work!\n");
		goto error;
	}
	
	printf("        ");
	print_sockaddr(&ifr.ifr_addr);

	if (ifr.ifr_addr.sa_family!=family){
		printf("ls_if: strange family %d\n", ifr.ifr_addr.sa_family);
		/*goto error;*/
	}
#endif
	if (options & (FLAGS|IF_DOWN|IF_UP)){
		if (ioctl(s, SIOCGIFFLAGS, &ifr)==-1){
			fprintf(stderr, "ls_if: flags ioctl for %s  failed: %s\n",
					name, strerror(errno));
			goto error;
		}
		if (ifr.ifr_flags & IFF_UP) printf ("UP ");
		if (ifr.ifr_flags & IFF_BROADCAST) printf ("BROADCAST ");
		if (ifr.ifr_flags & IFF_DEBUG) printf ("DEBUG ");
		if (ifr.ifr_flags & IFF_LOOPBACK) printf ("LOOPBACK ");
		if (ifr.ifr_flags & IFF_POINTOPOINT) printf ("POINTOPOINT ");
		if (ifr.ifr_flags & IFF_RUNNING) printf ("RUNNING ");
		if (ifr.ifr_flags & IFF_NOARP) printf ("NOARP ");
		if (ifr.ifr_flags & IFF_PROMISC) printf ("PROMISC ");
		/*if (ifr.ifr_flags & IFF_NOTRAILERS) printf ("NOTRAILERS ");*/
		if (ifr.ifr_flags & IFF_ALLMULTI) printf ("ALLMULTI ");
		/*if (ifr.ifr_flags & IFF_MASTER) printf ("MASTER ");*/
		/*if (ifr.ifr_flags & IFF_SLAVE) printf ("SLAVE ");*/
		if (ifr.ifr_flags & IFF_MULTICAST) printf ("MULTICAST ");
		/*if (ifr.ifr_flags & IFF_PORTSEL) printf ("PORTSEL ");*/
		/*if (ifr.ifr_flags & IFF_AUTOMEDIA) printf ("AUTOMEDIA ");*/
		/*if (ifr.ifr_flags & IFF_DYNAMIC ) printf ("DYNAMIC ");*/
		printf ("\n");
		if (options & IF_DOWN){
			ifr.ifr_flags &= ~IFF_UP;
		}
		if (options & IF_UP){
			ifr.ifr_flags |= IFF_UP;
		}
		if (options & (IF_UP|IF_DOWN)){
			if (ioctl(s, SIOCSIFFLAGS, &ifr)==-1){
				fprintf(stderr, "ls_if: set flags ioctl for %s  failed: %s\n",
						name, strerror(errno));
				goto error;
			}
		}
	};
		
	close(s);
	return 0;
error:
	close(s);
	return -1;
}



int ls_ifs(char* name, int family, int options)
{
	struct ifconf ifc;
	struct ifreq* ifr;
	char*  last;
	int size;
	int lastlen;
	int s;
	
	/* ipv4 or ipv6 only*/
	s=socket(family, SOCK_DGRAM, 0);
	lastlen=0;
	for (size=2; ; size*=2){
		ifc.ifc_len=size*sizeof(struct ifreq);
		ifc.ifc_req=(struct ifreq*) malloc(size*sizeof(struct ifreq));
		if (ifc.ifc_req==0){
			fprintf(stderr, "memory allocation failure\n");
			goto error;
		}
		if (ioctl(s, SIOCGIFCONF, &ifc)==-1){
			if(errno==EBADF) return 0; /* invalid descriptor => no such ifs*/
			fprintf(stderr, "ioctl failed: %s\n", strerror(errno));
			goto error;
		}
		if  ((lastlen) && (ifc.ifc_len==lastlen)) break; /*success,
														   len not changed*/
		lastlen=ifc.ifc_len;
		/* try a bigger array*/
		free(ifc.ifc_req);
	}
	
	last=(char*)ifc.ifc_req+ifc.ifc_len;
	for(ifr=ifc.ifc_req; (char*)ifr<last;
			ifr=(struct ifreq*)((char*)ifr+sizeof(ifr->ifr_name)+
			#ifdef  __FreeBSD__
				MAX(ifr->ifr_addr.sa_len, sizeof(struct sockaddr))
			#else
				( (ifr->ifr_addr.sa_family==AF_INET)?
					sizeof(struct sockaddr_in):
					((ifr->ifr_addr.sa_family==AF_INET6)?
						sizeof(struct sockaddr_in6):sizeof(struct sockaddr)) )
			#endif
				)
		)
	{
/*
		printf("\nls_all dbg: %s family=%d", ifr->ifr_name,
				 						ifr->ifr_addr.sa_family);
#ifdef __FreeBSD__
		printf(", len=%d\n", ifr->ifr_addr.sa_len);
#else
		printf("\n");
#endif
*/
		if (ifr->ifr_addr.sa_family!=family){
			/*printf("strange family %d skipping...\n",
					ifr->ifr_addr.sa_family);*/
			continue;
		}
		if ((name==0)||
			(strncmp(name, ifr->ifr_name, sizeof(ifr->ifr_name))==0)){
			printf("%s:\n", ifr->ifr_name);
			printf("        ");
			print_sockaddr(&(ifr->ifr_addr));
			printf("        ");
			ls_ifflags(ifr->ifr_name, family, options);
			printf("\n");
		}
	}
	free(ifc.ifc_req); /*clean up*/
	close(s);
	return  0;
error:
	close(s);
	return -1;
}


int main(int argc, char**argv)
{
	char** name;
	int no;
	int options;
	int ipv6, ipv4;
	int r;
	char c;
	
	
	options=0;
	ipv6=ipv4=1;
	name=0;
	no=0;
	opterr=0;
	while((c=getopt(argc, argv, "a46fhVUD"))!=-1){
		switch(c){
			case 'a':
				ipv6=ipv4=1;
				break;
			case '4':
				ipv6=0;
				ipv4=1;
				break;
			case '6':
				ipv4=0;
				ipv6=1;
				break;
			case 'f':
				options|=FLAGS;
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n", id);
				exit(0);
				break;
			case 'D':
				options|=IF_DOWN;
				break;
			case 'U':
				options|=IF_UP;
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c´\n", optopt);
				else
					fprintf(stderr, "Unknown character `\\x%x´\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option `-%c´ requires an argument\n",
						optopt);
				goto error;
			default:
				abort();
		};
	};
	/* check if we have non-options */
	if( optind < argc){
		no=argc-optind;
		name=&argv[optind];
	}
	
	if (no==0){
		/* list all interfaces */
		if (ipv4) ls_ifs(0, AF_INET, options);
		if (ipv6) ls_ifs(0, AF_INET6, options);
	}else{
		for(r=0; r<no; r++){
			if (ipv4) ls_ifs(name[r], AF_INET, options);
			if (ipv6) ls_ifs(name[r], AF_INET6, options);
		}
	};
	
	
	exit(0);
error:
	exit(-1);
};
