/*
shot written by ashhar farhan, is not bound by any licensing at all.
you are free to use this code as you deem fit. just dont blame the author
for any problems you may have using it.
bouquets and brickbats to farhan@hotfoon.com
*/

/* changes by jiri@iptel.org; now messages can be really received;
   status code returned is 2 for some local errors , 0 for success
   and 1 for remote error -- ICMP/timeout; can be used to test if
   a server is alive; 1xx messages are now ignored; windows support
   dropped
*/

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

#define RESIZE		1024
#define BUFSIZE		1500
#define VIA_BEGIN_STR "Via: SIP/2.0/UDP "
#define VIA_BEGIN_STR_LEN 17
#define MAX_FRW_STR "Max-Forwards: "
#define MAX_FRW_STR_LEN 14
#define SIP20_STR " SIP/2.0\r\n"
#define SIP20_STR_LEN 10

int verbose=0;

/* take either a dot.decimal string of ip address or a 
domain name and returns a NETWORK ordered long int containing
the address. i chose to internally represent the address as long for speedier
comparisions.

any changes to getaddress have to be patched back to the net library.
contact: farhan@hotfoon.com

  returns zero if there is an error.
  this is convenient as 0 means 'this' host and the traffic of
  a badly behaving dns system remains inside (you send to 0.0.0.0)
*/

long getaddress(char *host)
{
	int i, dotcount=0;
	char *p = host;
	struct hostent* pent;
	long l, *lp;

	/*try understanding if this is a valid ip address
	we are skipping the values of the octets specified here.
	for instance, this code will allow 952.0.320.567 through*/
	while (*p)
	{
		for (i = 0; i < 3; i++, p++)
			if (!isdigit(*p))
				break;
		if (*p != '.')
			break;
		p++;
		dotcount++;
	}

	/* three dots with upto three digits in before, between and after ? */
	if (dotcount == 3 && i > 0 && i <= 3)
		return inet_addr(host);

	/* try the system's own resolution mechanism for dns lookup:
	 required only for domain names.
	 inspite of what the rfc2543 :D Using SRV DNS Records recommends,
	 we are leaving it to the operating system to do the name caching.

	 this is an important implementational issue especially in the light
	 dynamic dns servers like dynip.com or dyndns.com where a dial
	 ip address is dynamically assigned a sub domain like farhan.dynip.com

	 although expensive, this is a must to allow OS to take
	 the decision to expire the DNS records as it deems fit.
	*/
	pent = gethostbyname(host);
	if (!pent) {
		perror("no gethostbyname");
		exit(2);
	}
	lp = (long *) (pent->h_addr);
	l = *lp;
	return l;
}

/* This function check for the existence of a Max-Forwards Header Field.
   If its present it sets it to the given value, if not it will be inserted.*/
void set_maxforw(char *mes, int maxfw){
	char *max, *backup, *crlf;

	if ((max=strstr(mes,"Max-Forwards"))==NULL){
		max=strchr(mes,'\n');
		max++;
		backup=malloc(strlen(max)+1);
		strncpy(backup, max, strlen(max)+1);
		sprintf(max, "%s%i\r\n", MAX_FRW_STR, maxfw);
		max=strchr(max,'\n');
		max++;
		strncpy(max, backup, strlen(backup)+1);
		free(backup);
		if (verbose)
			printf("Max-Forwards %i inserted into header\n", maxfw);
#ifdef DEBUG
		printf("New message with inserted Max-Forwards:\n%s\n", mes);
#endif
	}
	else{
		crlf=strchr(max,'\n');
		crlf++;
		backup=malloc(strlen(crlf)+1);
		strncpy(backup, crlf, strlen(crlf)+1);
		crlf=max + MAX_FRW_STR_LEN;
		sprintf(crlf, "%i\r\n", maxfw);
		crlf=strchr(max,'\n');
		crlf++;
		strncpy(crlf, backup, strlen(backup)+1);
		crlf=crlf+strlen(backup);
		free(backup);
		if (verbose)
			printf("Max-Forwards set to %i\n", maxfw);
#ifdef DEBUG
		printf("New message with changed Max-Forwards:\n%s\n", mes);
#endif
	}
}

/* This function tries to add a Via Header Field in the message. */
void add_via(char *mes, int port)
{
	char *via_line, *via, *backup; 
	char hname[100], dname[100], fqdnname[200];
	size_t namelen=100;

	if (gethostname(&hname[0], namelen) < 0) {
		printf("error: cannot determine domainname\n");
		exit(2);
	}
	if ((strchr(hname, '.'))==NULL) {
#ifdef DEBUG
		printf("hostname without dots. determine domainname...\n");
#endif
		if (getdomainname(&dname[0], namelen) < 0) {
			printf("error: cannot determine domainname\n");
			exit(2);
		}
		sprintf(fqdnname, "%s.%s", hname, dname);
	}
	else {
		strcpy(fqdnname, hname);
	}

#ifdef DEBUG
	printf("fqdnhostname: %s\n", fqdnname);
#endif

	via_line = malloc(VIA_BEGIN_STR_LEN+strlen(fqdnname)+9);
	sprintf(via_line, "%s%s:%i\r\n", VIA_BEGIN_STR, fqdnname, port);
#ifdef DEBUG
	printf("our Via-Line: %s\n", via_line);
#endif

	if (strlen(mes)+strlen(via_line)>= BUFSIZE){
		printf("can't add our Via Header Line because file is too big\n");
		exit(2);
	}
	if ((via=strstr(mes,"Via:"))==NULL){
		/* We doesn't find a Via so we insert our via
		   direct after the first line. */
		via=strchr(mes,'\n');
		via++;
	}
	backup=malloc(strlen(via)+1);
	strncpy(backup, via, strlen(via)+1);
	strncpy(via, via_line, strlen(via_line));
	strncpy(via+strlen(via_line), backup, strlen(backup)+1);
	free(via_line);
	free(backup);
	if (verbose)
		printf("New message with Via-Line:\n%s\n", mes);
}

/* replaces the uri in first line of mes with the other uri */
void uri_replace(char *mes, char *uri)
{
	char *foo, *backup;

	foo=strchr(mes, '\n');
	foo++;
	backup=malloc(strlen(foo)+1);
	strncpy(backup, foo, strlen(foo)+1);
	foo=strstr(mes, "sip");
	strncpy(foo, uri, strlen(uri));
	strncpy(foo+strlen(uri), SIP20_STR, SIP20_STR_LEN);
	strncpy(foo+strlen(uri)+SIP20_STR_LEN, backup, strlen(backup)+1);
	free(backup);
#ifdef DEBUG
	printf("Message with modified uri:\n%s\n", mes);
#endif
}

/*
shoot:
takes:
	1. the text message of buff to 
	2. the address (network orderd byte order)
	3. and port (not network byte ordered).

starting from half a second, times-out on replies and
keeps retrying with exponential back-off that flattens out
at 5 seconds (5000 milliseconds).

* Does not stop sending unless a final response is received.
we are detecting the final response without a '1' as the first
letter.
*/
void shoot(char *buff, long address, int lport, int rport, int maxforw, int trace, int vbool)
{
	struct sockaddr_in	addr;
	struct sockaddr_in	sockname;
	int ssock;
	int redirected=1;
	/*
	char compiledre[ RESIZE ];
	*/
	int retryAfter = 500;
	int	nretries = 10;
	int sock, i, len, ret;
	struct timeval	tv;
	fd_set	fd;
	char	reply[1600];
	socklen_t slen;
	char	*contact, *crlf, *foo, *bar;
	regex_t* regexp;
	regex_t* redexp;
/*	struct hostent *host;
	long l, *lp; */

	/* create a socket */
	sock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock==-1) {
		perror("no client socket");
		exit(2);
	}

	ssock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ssock==-1) {
		perror("no server socket");
		exit(2);
	}

	sockname.sin_family=AF_INET;
	sockname.sin_addr.s_addr = htonl( INADDR_ANY );
	sockname.sin_port = htons((short)lport);
	if (bind( ssock, (struct sockaddr *) &sockname, sizeof(sockname) )==-1) {
		perror("no bind");
		exit(2);
	}

	/* for the via line we need our listening port number */
	if (vbool && lport==0){
		memset(&sockname, 0, sizeof(sockname));
		slen=sizeof(sockname);
		getsockname(ssock, (struct sockaddr *)&sockname, &slen);
		lport=ntohs(sockname.sin_port);
	}

	/* should capture: SIP/2.0 100 Trying */
	/* compile("^SIP/[0-9]\\.[0-9] 1[0-9][0-9] ", compiledre, &compiledre[RESIZE], '\0'); */
	regexp=(regex_t*)malloc(sizeof(regex_t));
	if (!trace)
		regcomp(regexp, "^SIP/[0-9]\\.[0-9] 1[0-9][0-9] ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	else
		regcomp(regexp, "^SIP/[0-9]\\.[0-9] 483 ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	/* catching redirects */
	redexp=(regex_t*)malloc(sizeof(regex_t));
	regcomp(redexp, "^SIP/[0-9]\\.[0-9] 3[0-9][0-9] ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 

	if(maxforw)
		set_maxforw(buff, maxforw);
	if(vbool)
		add_via(buff, lport);
	
	if (trace) {
		if (maxforw)
			nretries=maxforw;
		else
			nretries=255;
	}

	while (redirected) {

		redirected=0;

		addr.sin_addr.s_addr = address;
		addr.sin_port = htons((short)rport);
		addr.sin_family = AF_INET;
	
		/* we connect as per the RFC 2543 recommendations
		modified from sendto/recvfrom */

		ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
		if (ret==-1) {
			perror("no connect");
			exit(2);
		}

		for (i = 0; i < nretries; i++)
		{
			if (!trace)
				printf("** request **\n%s\n", buff);
			else {
				set_maxforw(buff, i+1);
			}

			ret = send(sock, buff, strlen(buff), 0);
			if (ret==-1) {
				perror("send failure");
				exit( 1 );
			}
		

			tv.tv_sec = retryAfter/1000;
			tv.tv_usec = (retryAfter % 1000) * 1000;

			FD_ZERO(&fd);
			FD_SET(ssock, &fd); 

			/* TO-DO: there does appear to be a problem with this select returning a zero
			even when there is data pending in the recv queue. 
			please help, someone! */

			ret = select(6, &fd, NULL, NULL, &tv);
			if (ret == 0)
			{
				printf("** timeout **\n");
				retryAfter = retryAfter * 2;
				if (retryAfter > 5000)
					retryAfter = 5000;
				/* we should have retrieved the error code and displayed
				we are not doing that because there is a great variation
				in the process of retrieveing error codes between
				micro$oft and *nix world*/
				continue;
			} else if ( ret == -1 ) {
				perror("select error");
				exit(2);
			} /* no timeout, no error ... something has happened :-) */
   	              else if (FD_ISSET(ssock, &fd)) {
			 	if (!trace)
					puts ("\nmessage received");
			} else {
				puts("\nselect returned succesfuly, nothing received\n");
				continue;
			}

			/* we are retrieving only the extend of a decent MSS = 1500 bytes */
			len = sizeof(addr);
			ret = recv(ssock, reply, 1500, 0);
			if(ret > 0)
			{
				reply[ret] = 0;
				if (regexec((regex_t*)redexp, reply, 0, 0, 0)==0) {
					printf("** received redirect **\n");
					/* we'll try to handle 301 and 302 here, other 3xx are to complex */
					regcomp(redexp, "^SIP/[0-9]\\.[0-9] 30[1-2] ", REG_EXTENDED|REG_NOSUB|REG_ICASE);
					if (regexec((regex_t*)redexp, reply, 0, 0, 0)==0) {
						if ((foo=strstr(reply, "Contact"))==NULL) {
							printf("error: cannot find Contact in this redirect:\n%s\n", reply);
							exit(2);
						}
						crlf=strchr(foo, '\n');
						if ((contact=strchr(foo, '\r'))!=NULL && contact<crlf)
							crlf=contact;
						bar=malloc(crlf-foo+1);
						strncpy(bar, foo, crlf-foo);
						sprintf(bar+(crlf-foo), "\0");
						if ((contact=strstr(bar, "sip"))==NULL) {
							printf("error: cannot find sip in the Contact of this redirect:\n%s\n", reply);
							exit(2);
						}
						if ((foo=strchr(contact, ';'))!=NULL)
							*foo='\0';
						if ((foo=strchr(contact, '>'))!=NULL)
							*foo='\0';
						if ((crlf=strchr(contact,':'))!=NULL){
							crlf++;
							if ((foo=strchr(crlf,':'))!=NULL){
								*foo='\0';
								foo++;
								rport = atoi(foo);
								if (!rport) {
									printf("error: cannot handle the port in the uri in Contact:\n%s\n", reply);
									exit(2);
								}
							}
							uri_replace(buff, contact);
							if ((foo=strchr(contact,'@'))!=NULL){
								foo++;
								crlf=foo;
							}
							address = getaddress(crlf);
							if (!address){
								printf("error: cannot determine host address from Contact of redirect:\%s\n", reply);
								exit(2);
							}
						}
						else{
							printf("error: missing : in Contact of this redirect:\n%s\n", reply);
							exit(2);
						}
						free(bar);
						memset(&addr, 0, sizeof(addr));
						redirected=1;
						i=nretries;
					}
					else {
						printf("error: cannot handle this redirect:\n%s\n", reply);
						exit(2);
					}
				}
				else if (!trace) {
					printf("** reply **\n%s\n", reply);
					/* if (step( reply, compiledre )) { */
					if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
						puts(" provisional received; still waiting for a final response\n ");
						continue;
					} else {
						puts(" final received; congratulations!\n ");
						exit(0);
					}
				}
				else {
					printf("%i: ", i+1);
					if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
						printf("* (483) \n");
#ifdef DEBUG
						printf("%s\n", reply);
#endif
						continue;
					}
					else {
						crlf=strchr(reply,'\n');
						sprintf(crlf, "\0");
						printf("%s\n", reply);
						crlf++;
						contact=strstr(crlf, "Contact");
						crlf=strchr(contact,'\n');
						sprintf(crlf, "\0");
						printf("   %s\n", contact);
						exit(0);
					}
				}
		
			} 
			else {
				perror("recv error");
				exit(2);
			}
		}

	}
	/* after all the retries, nothing has come back :-( */
	puts("** I give up retransmission....");
	exit(1);
}

int main(int argc, char *argv[])
{
	long	address;
	FILE	*pf;
	char	buff[BUFSIZE];
	int		length, c, fbool, sbool, tbool, vbool;
	int		maxforw=0;
	int		lport=0;
	int		rport=5060;
	char	*delim, *delim2;

	fbool=sbool=tbool=0;
	vbool=1;

	while ((c=getopt(argc,argv,"f:s:l:m:thiv")) != EOF){
		switch(c){
			case 'h':
				printf("\nusage: sipsak [-h] -f filename -s sip:uri [-l port] [-m number] [-t] [-i] [-v]\n"
						"   -h           displays this help message\n"
						"   -f filename  the file which contains the SIP message to send\n"
						"   -s sip:uri   the destination server uri in form sip:[user@]servername[:port]\n"
						"   -l port      the local port to use\n"
						"   -m number    the value for the max-forwards header field\n"
						"   -t           activates the traceroute modus\n"
						"   -i           deactivate the insertion of a Via-Line\n"
						"   -v           be more verbose\n"
						"The manupulation function are only tested with nice RFC conform SIP-messages,\n"
						"so don't expect them to work with ugly or malformed messages.\n");
				exit(0);
				break;
			case 'f':
				/* file is opened in binary mode so that the cr-lf is preserved */
				pf = fopen(optarg, "rb");
				if (!pf){
					puts("unable to open the file.\n");
					exit(2);
				}
				length  = fread(buff, 1, sizeof(buff), pf);
				if (length >= sizeof(buff)){
					printf("error:the file is too big. try files of less than %i bytes.\n", BUFSIZE);
					puts("      or recompile the program with bigger BUFSIZE defined.");
					exit(2);
				}
				fclose(pf);
				buff[length] = '\0';
				fbool=1;
				break;
			case 's':
				if ((delim=strchr(optarg,':'))!=NULL){
					delim++;
					if (!strncmp(optarg,"sip",3)){
						if ((delim2=strchr(delim,'@'))!=NULL){
							/* we don't need the username */
							delim2++;
							delim=delim2;
						}
						if ((delim2=strchr(delim,':'))!=NULL){
							*delim2 = '\0';
							delim2++;
							rport = atoi(delim2);
							if (!rport) {
								puts("error: non-numerical remote port number");
								exit(2);
							}
						}
						address = getaddress(delim);
						if (!address){
							puts("error:unable to determine the remote host address.");
							exit(2);
						}
					}
					else{
						puts("error: sip:uri doesn't not begin with sip");
						exit(2);
					}
				}
				else{
					puts("error: sip:uri doesn't contain a : ?!");
					exit(2);
				}
				sbool=1;
				break;
			case 'l':
				lport=atoi(optarg);
				if (!lport) {
					puts("error: non-numerical local port number");
					exit(2);
				}
				break;
			case 'm':
				maxforw=atoi(optarg);
				if (!maxforw) {
					puts("error: non-numerical number of max-forwards");
					exit(2);
				}
				break;
			case 't':
				tbool=1;
				break;
			case 'i':
				vbool=0;
				break;
			case 'v':
				verbose=1;
				break;
		}
	}

	if (tbool) {
		if (strncmp(buff, "OPTIONS", 7)){
			printf("error: tracerouting only possible with an OPTIONS request.\n"
				"       Give another request file or convert it to an OPTIONS request.\n");
			exit(2);
		}
	}

	if (fbool & sbool)
		shoot(buff, address, lport, rport, maxforw, tbool, vbool);
	else{
		printf("error: you have to give the file to send and the sip:uri at least.\n"
			"       see 'sipsak -h' for more help.\n");
	}

	/* visual studio closes the debug console as soon as the 
	program terminates. this is to hold the window from collapsing
	Uncomment it if needed.
	getchar();*/
	

	return 0;
}


/*
shoot will exercise the all types of sip servers.
it is not to be used to measure round-trips and general connectivity.
use ping for that. 
written by farhan on 10th august, 2000.

TO-DO:
2. understand redirect response and retransmit to the redirected server.

*/

