/* $Id$
 *
 *
 * test programs, list all interfaces and their ip address
 */


#include <sys/ioctl.h>
#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FLAGS 1


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
    -V      version number\n\
    -h      this help message\n\
";



int ls_if(char* name, int family , int options)
{
	struct ifreq ifr;
	int s;
	unsigned char* buf;
	int r;
	
	
	s=socket(family, SOCK_DGRAM, 0);
	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	if (ioctl(s, SIOCGIFADDR, &ifr)==-1){
		if(errno==EBADF) return 0; /* invalid descriptor => no address*/
		fprintf(stderr, "ls_if: ioctl failed: %s\n", strerror(errno));
		goto error;
	};
	
	printf("%s:\n", ifr.ifr_name);
	if (ifr.ifr_addr.sa_family==AF_INET){
		buf=&((struct sockaddr_in*)(&ifr.ifr_addr))->sin_addr;
		printf("        %d.%d.%d.%d\n", buf[0], buf[1], buf[2], buf[3]);
	}else{
		buf=&((struct sockaddr_in6*)(&ifr.ifr_addr))->sin6_addr;
		printf("        ");
		for(r=0; r<16; r++) printf("%02x%s", buf[r], (r%2)?":":"" );
		printf("\n");
	}
	if (options & FLAGS){
		if (ioctl(s, SIOCGIFFLAGS, &ifr)==-1){
			fprintf(stderr, "ls_if: ioctl failed: %s\n", strerror(errno));
			goto error;
		}
		printf("        ");
		if (ifr.ifr_flags & IFF_UP) printf ("UP ");
		if (ifr.ifr_flags & IFF_BROADCAST) printf ("BROADCAST ");
		if (ifr.ifr_flags & IFF_DEBUG) printf ("DEBUG ");
		if (ifr.ifr_flags & IFF_LOOPBACK) printf ("LOOPBACK ");
		if (ifr.ifr_flags & IFF_POINTOPOINT) printf ("POINTOPOINT ");
		if (ifr.ifr_flags & IFF_RUNNING) printf ("RUNNING ");
		if (ifr.ifr_flags & IFF_NOARP) printf ("NOARP ");
		if (ifr.ifr_flags & IFF_PROMISC) printf ("PROMISC ");
		if (ifr.ifr_flags & IFF_NOTRAILERS) printf ("NOTRAILERS ");
		if (ifr.ifr_flags & IFF_ALLMULTI) printf ("ALLMULTI ");
		if (ifr.ifr_flags & IFF_MASTER) printf ("MASTER ");
		if (ifr.ifr_flags & IFF_SLAVE) printf ("SLAVE ");
		if (ifr.ifr_flags & IFF_MULTICAST) printf ("MULTICAST ");
		if (ifr.ifr_flags & IFF_PORTSEL) printf ("PORTSEL ");
		if (ifr.ifr_flags & IFF_AUTOMEDIA) printf ("AUTOMEDIA ");
		/*if (ifr.ifr_flags & IFF_DYNAMIC ) printf ("DYNAMIC ");*/
		printf ("\n");
	};

	return 0;
error:
	return -1;
}



int ls_all(int family, int options)
{
	struct ifconf ifc;
	struct ifreq* ifr;
	char*  last;
	int size;
	int s;
	
	/* ipv4 or ipv6 only*/
	s=socket(family, SOCK_DGRAM, 0);
	for (size=10; ; size*=2){
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
		if (ifc.ifc_len<size*sizeof(struct ifreq)) break;
		/* try a bigger array*/
		free(ifc.ifc_req);
	}
	
	last=(char*)ifc.ifc_req+ifc.ifc_len;
	for(ifr=ifc.ifc_req; (char*)ifr<last;
			ifr=(struct ifreq*)((char*)ifr+sizeof(ifr->ifr_name)+
				( (ifr->ifr_addr.sa_family==AF_INET)?
					sizeof(struct sockaddr_in): sizeof(struct sockaddr_in6)) )
		)
	{
		ls_if(ifr->ifr_name, family, options);
	}
	free(ifc.ifc_req); /*clean up*/
	return  0;
error:
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
	while((c=getopt(argc, argv, "a46fhV"))!=-1){
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
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknow option `-%c´\n", optopt);
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
		if (ipv4) ls_all(AF_INET, options);
		if (ipv6) ls_all(AF_INET6, options);
	}else{
		for(r=0; r<no; r++){
			if (ipv4) ls_if(name[r], AF_INET, options);
			if (ipv6) ls_if(name[r], AF_INET6, options);
		}
	};
	
	
	exit(0);
error:
	exit(-1);
};	
