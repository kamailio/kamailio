/*
 * $Id$
 */

/* sipsak written by nils ohlmeier (ohlmeier@fokus.gmd.de).
based up on a modifyed version of shoot.
set DEBUG on compile will produce much more output, primary
it will print out the sended and received messages before or after
every network action.
*/

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
#include <arpa/inet.h>
#include <sys/poll.h>

#define SIPSAK_VERSION "v0.5"
#define RESIZE		1024
#define BUFSIZE		4096
#define FQDN_SIZE   200
#define REQ_INV 1
#define REQ_REG 2
#define REQ_OPT 3
#define REQ_FLOOD 4
#define REQ_RAND 5
#define VIA_STR "Via: SIP/2.0/UDP "
#define VIA_STR_LEN 17
#define MAX_FRW_STR "Max-Forwards: "
#define MAX_FRW_STR_LEN 14
#define SIP20_STR " SIP/2.0\r\n"
#define SIP20_STR_LEN 10
#define SIP200_STR "SIP/2.0 200 OK\r\n"
#define SIP200_STR_LEN 16
#define REG_STR "REGISTER"
#define REG_STR_LEN 8
#define OPT_STR "OPTIONS"
#define OPT_STR_LEN 7
#define MES_STR "MESSAGE"
#define MES_STR_LEN 7
#define FROM_STR "From: "
#define FROM_STR_LEN 6
#define TO_STR "To: "
#define TO_STR_LEN 4
#define CALL_STR "Call-ID: "
#define CALL_STR_LEN 9
#define CSEQ_STR "CSeq: "
#define CSEQ_STR_LEN 6
#define CONT_STR "Contact: "
#define CONT_STR_LEN 9
#define CON_TXT_STR "Content-Type: text/plain\r\n"
#define CON_TXT_STR_LEN 26
#define CON_LEN_STR "Content-Length: "
#define CON_LEN_STR_LEN 16
#define SIPSAK_MES_STR "USRLOC test message from SIPsak for user "
#define SIPSAK_MES_STR_LEN 41
#define EXP_STR "Expires: "
#define EXP_STR_LEN 9
#define USRLOC_EXP_DEF 15
#define FLOOD_METH "OPTIONS"

long address;
int verbose, nameend, namebeg, expires_t, flood, warning_ext;
int maxforw, lport, rport, randtrash, trashchar, numeric;
int file_b, uri_b, trace, via_ins, usrloc, redirects;
char *username, *domainname;
char fqdn[FQDN_SIZE], messusern[FQDN_SIZE];
char message[BUFSIZE], mes_reply[BUFSIZE];

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

/* because the full qualified domain name is needed by many other
   functions it will be determined by this function.*/
void get_fqdn(){
	char hname[100], dname[100], hlp[18];
	size_t namelen=100;
	struct hostent* he;
	int i;
	unsigned char *addrp;
	char *fqdnp;

	if (gethostname(&hname[0], namelen) < 0) {
		printf("error: cannot determine hostname\n");
		exit(2);
	}
	/* a hostname with dots should be a domainname */
	if ((strchr(hname, '.'))==NULL) {
#ifdef DEBUG
		printf("hostname without dots. determine domainname...\n");
#endif
		if (getdomainname(&dname[0], namelen) < 0) {
			printf("error: cannot determine domainname\n");
			exit(2);
		}
		sprintf(fqdn, "%s.%s", hname, dname);
	}
	else {
		strcpy(fqdn, hname);
	}

	if (numeric) {
		he=gethostbyname(fqdn);
		if (he) {
			addrp = he->h_addr_list[0];
			hlp[0]=fqdn[0]='\0';
			fqdnp = &fqdn[0];
			for (i = 0; i < 3; i++) {
				sprintf(hlp, "%i.", addrp[i]);
				fqdnp = strcat(fqdn, hlp);
			}
			sprintf(hlp, "%i", addrp[3]);
			fqdnp = strcat(fqdn, hlp);
		}
		else {
			printf("error: can not resolve hostname\n");
			exit(2);
		}
	}

#ifdef DEBUG
	printf("fqdnhostname: %s\n", fqdn);
#endif
}

/* add a Via Header Field in the message.
*/
void add_via(char *mes)
{
	char *via_line, *via, *backup; 

	/* first build our own Via-header-line */
	via_line = malloc(VIA_STR_LEN+strlen(fqdn)+9);
	sprintf(via_line, "%s%s:%i\r\n", VIA_STR, fqdn, lport);
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
	/* finnaly make a backup, insert our via and append the backup */
	backup=malloc(strlen(via)+1);
	strncpy(backup, via, strlen(via)+1);
	strncpy(via, via_line, strlen(via_line));
	strncpy(via+strlen(via_line), backup, strlen(backup)+1);
	free(via_line);
	free(backup);
	if (verbose)
		printf("New message with Via-Line:\n%s\n", mes);
}

/* copy the via lines from the message to the message 
   reply for correct routing of our reply.*/
void cpy_vias(char *reply){
	char *first_via, *middle_via, *last_via, *backup;

	/* lets see if we find any via */
	if ((first_via=strstr(reply, "Via:"))==NULL){
		printf("error: the received message doesn't contain a Via header\n");
		exit(1);
	}
	last_via=first_via+4;
	middle_via=last_via;
	/* proceed additional via lines */
	while ((middle_via=strstr(last_via, "Via:"))!=NULL)
		last_via=middle_via+4;
	last_via=strchr(last_via, '\n');
	middle_via=strchr(mes_reply, '\n')+1;
	/* make a backup, insert the vias after the first line and append 
	backup */
	backup=malloc(strlen(middle_via)+1);
	strcpy(backup, middle_via);
	strncpy(middle_via, first_via, last_via-first_via+1);
	strcpy(middle_via+(last_via-first_via+1), backup);
	free(backup);
#ifdef DEBUG
	printf("message reply with vias included:\n%s\n", mes_reply);
#endif
}

/* create a valid sip header out of the given parameters */
void create_msg(char *buff, int action){
	unsigned int c;
	char *usern;

	c=rand();
	switch (action){
		case REQ_REG:
#ifdef DEBUG
			printf("username: %s\ndomainname: %s\n", username, domainname);
#endif
			usern=malloc(strlen(username)+10);
			sprintf(messusern, "%s sip:%s%i", MES_STR, username, namebeg);
			sprintf(usern, "%s%i", username, namebeg);
			/* build the register, message and the 200 we need in for 
			   USRLOC on one function call*/
			sprintf(buff, "%s sip:%s%s%s%s:%i\r\n%s<sip:%s@%s>\r\n%s<sip:%s@%s>\r\n%s%u@%s\r\n%s%i %s\r\n%s<sip:%s@%s:%i>\r\n%s%i\r\n\r\n", REG_STR, domainname, SIP20_STR, VIA_STR, fqdn, lport, FROM_STR, usern, domainname, TO_STR, usern, domainname, CALL_STR, c, fqdn, CSEQ_STR, 3*namebeg+1, REG_STR, CONT_STR, usern, fqdn, lport, EXP_STR, expires_t);
			c=rand();
			sprintf(message, "%s sip:%s@%s%s%s%s:%i\r\n%s<sip:sipsak@%s:%i>\r\n%s<sip:%s@%s>\r\n%s%u@%s\r\n%s%i %s\r\n%s%s%i\r\n\r\n%s%s%i.", MES_STR, usern, domainname, SIP20_STR, VIA_STR, fqdn, lport, FROM_STR, fqdn, lport, TO_STR, usern, domainname, CALL_STR, c, fqdn, CSEQ_STR, 3*namebeg+2, MES_STR, CON_TXT_STR, CON_LEN_STR, SIPSAK_MES_STR_LEN+strlen(usern), SIPSAK_MES_STR, username, namebeg);
			sprintf(mes_reply, "%s%s<sip:sipsak@%s:%i>\r\n%s<sip:%s@%s>\r\n%s%u@%s\r\n%s%i %s\r\n%s 0\r\n\r\n", SIP200_STR, FROM_STR, fqdn, lport, TO_STR, usern, domainname, CALL_STR, c, fqdn, CSEQ_STR, 3*namebeg+2, MES_STR, CON_LEN_STR);
#ifdef DEBUG
			printf("message:\n%s\n", message);
			printf("message reply:\n%s\n", mes_reply);
#endif
			free(usern);
			break;
		case REQ_OPT:
			sprintf(buff, "%s sip:%s@%s%s%s<sip:sipsak@%s:%i>\r\n%s<sip:%s@%s>\r\n%s%u@%s\r\n%s%i %s\r\n%s<sip:sipsak@%s:%i>\r\n\r\n", OPT_STR, username, domainname, SIP20_STR, FROM_STR, fqdn, lport, TO_STR, username, domainname, CALL_STR, c, fqdn, CSEQ_STR, namebeg, OPT_STR, CONT_STR, fqdn, lport);
			break;
		case REQ_FLOOD:
			sprintf(buff, "%s sip:%s%s%s%s:9\r\n%s<sip:sipsak@%s:9>\r\n%s<sip:%s>\r\n%s%u@%s\r\n%s%i %s\r\n%s<sipsak@%s:9>\r\n\r\n", FLOOD_METH, domainname, SIP20_STR, VIA_STR, fqdn, FROM_STR, fqdn, TO_STR, domainname, CALL_STR, c, fqdn, CSEQ_STR, namebeg, FLOOD_METH, CONT_STR, fqdn);
			break;
		case REQ_RAND:
			sprintf(buff, "%s sip:%s%s%s%s:%i\r\n%s<sip:sipsak@%s:%i>\r\n%s<sip:%s>\r\n%s%u@%s\r\n%s%i %s\r\n%s<sipsak@%s:%i>\r\n\r\n", OPT_STR, domainname, SIP20_STR, VIA_STR, fqdn, lport, FROM_STR, fqdn, lport, TO_STR, domainname, CALL_STR, c, fqdn, CSEQ_STR, namebeg, OPT_STR, CONT_STR, fqdn, lport);
			break;
		default:
			printf("error: unknown request type to create\n");
			exit(2);
			break;
	}
#ifdef DEBUG
	printf("request:\n%s", buff);
#endif
}

/* check for the existence of a Max-Forwards header field. if its 
   present it sets it to the given value, if not it will be inserted.*/
void set_maxforw(char *mes){
	char *max, *backup, *crlf;

	if ((max=strstr(mes,"Max-Forwards"))==NULL){
		/* no max-forwards found so insert it after the first line*/
		max=strchr(mes,'\n');
		max++;
		backup=malloc(strlen(max)+1);
		strncpy(backup, max, strlen(max)+1);
		sprintf(max, "%s%i\r\n", MAX_FRW_STR, maxforw);
		max=strchr(max,'\n');
		max++;
		strncpy(max, backup, strlen(backup)+1);
		free(backup);
		if (verbose)
			printf("Max-Forwards %i inserted into header\n", maxforw);
#ifdef DEBUG
		printf("New message with inserted Max-Forwards:\n%s\n", mes);
#endif
	}
	else{
		/* found max-forwards => overwrite the value with maxforw*/
		crlf=strchr(max,'\n');
		crlf++;
		backup=malloc(strlen(crlf)+1);
		strncpy(backup, crlf, strlen(crlf)+1);
		crlf=max + MAX_FRW_STR_LEN;
		sprintf(crlf, "%i\r\n", maxforw);
		crlf=strchr(max,'\n');
		crlf++;
		strncpy(crlf, backup, strlen(backup)+1);
		crlf=crlf+strlen(backup);
		free(backup);
		if (verbose)
			printf("Max-Forwards set to %i\n", maxforw);
#ifdef DEBUG
		printf("New message with changed Max-Forwards:\n%s\n", mes);
#endif
	}
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

/* trashes one character in buff radnomly */
void trash_random(char *message)
{
	int r;
	float t;
	char *position;

	t=(float)rand()/RAND_MAX;
	r=t * (float)strlen(message);
	position=message+r;
	r=t*(float)255;
	*position=(char)r;
#ifdef DEBUG
	printf("request:\n%s\n", message);
#endif
}

/* tryes to find the warning header filed and prints out the IP */
void warning_extract(char *message)
{
	char *warning, *end, *mid, *server;
	int srvsize;

	warning=strstr(message, "Warning");
	if (warning) {
		end=strchr(warning, '"');
		end--;
		warning=strchr(warning, '3');
		warning=warning+4;
		mid=strchr(warning, ':');
		if (mid) end=mid;
		srvsize=end - warning + 1;
		server=malloc(srvsize);
		memset(server, 0, srvsize);
		server=strncpy(server, warning, srvsize - 1);
		printf("%s ", server);
	}
	else {
		printf("no Warning header found\n");
	}
}

/* this function is taken from traceroute-1.4_p12 
   which is distributed under the GPL */
double deltaT(struct timeval *t1p, struct timeval *t2p)
{
        register double dt;

        dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
             (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
        return (dt);
}

/*
shoot:
takes:
	1. the text message of buff to 
	2. the address (network orderd byte order)
	3. local- and remote-port (not network byte ordered).
	4. and lots of boolean for the different modi

starting from half a second, times-out on replies and
keeps retrying with exponential back-off that flattens out
at 5 seconds (5000 milliseconds).
*/
void shoot(char *buff)
{
	struct sockaddr_in	addr, sockname;
	struct timeval	tv, sendtime, recvtime, firstsendt;
	struct timezone tz;
	struct pollfd sockerr;
	int ssock, redirected, retryAfter, nretries;
	int sock, i, len, ret, usrlocstep, randretrys;
	char *contact, *crlf, *foo, *bar;
	char reply[BUFSIZE];
	fd_set	fd;
	socklen_t slen;
	regex_t* regexp;
	regex_t* redexp;

	redirected = 1;
	nretries = 5;
	retryAfter = 5000;
	usrlocstep = 0;

	/* create a sending socket */
	sock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock==-1) {
		perror("no client socket");
		exit(2);
	}

	/* create a listening socket */
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
	if ((via_ins||usrloc) && lport==0){
		memset(&sockname, 0, sizeof(sockname));
		slen=sizeof(sockname);
		getsockname(ssock, (struct sockaddr *)&sockname, &slen);
		lport=ntohs(sockname.sin_port);
	}

	/* set a regular expression according to the modus */
	regexp=(regex_t*)malloc(sizeof(regex_t));
	if (trace)
		regcomp(regexp, "^SIP/[0-9]\\.[0-9] 483 ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	else if (usrloc)
		regcomp(regexp, "^SIP/[0-9]\\.[0-9] 200 ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	else if (randtrash)
		regcomp(regexp, "^SIP/[0-9]\\.[0-9] 4[0-9][0-9] ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	else
		regcomp(regexp, "^SIP/[0-9]\\.[0-9] 1[0-9][0-9] ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	/* catching redirects */
	redexp=(regex_t*)malloc(sizeof(regex_t));
	regcomp(redexp, "^SIP/[0-9]\\.[0-9] 3[0-9][0-9] ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 

	if (usrloc){
		nretries=3*(nameend-namebeg)+3;
		create_msg(buff, REQ_REG);
	}
	else if (trace){
		if (maxforw!=-1)
			nretries=maxforw;
		else
			nretries=255;
		namebeg=1;
		maxforw=0;
		create_msg(buff, REQ_OPT);
		add_via(buff);
	}
	else if (flood){
		if (namebeg==-1) namebeg=2147483647;
		nretries=namebeg;
		namebeg=1;
		create_msg(buff, REQ_FLOOD);
	}
	else if (randtrash){
		randretrys=0;
		namebeg=1;
		create_msg(buff, REQ_RAND);
		nameend=strlen(buff);
		if (trashchar){
			if (trashchar < nameend)
				nameend=trashchar;
			else
				printf("warning: number of trashed chars to big. setting to request lenght\n");
		}
		nretries=nameend-1;
		trash_random(buff);
	}
	else {
		if (!file_b) {
			namebeg=1;
			create_msg(buff, REQ_OPT);
		}
		retryAfter = 500;
		if(maxforw!=-1)
			set_maxforw(buff);
		if(via_ins)
			add_via(buff);
	}

	/* if we got a redirect this loop ensures sending to the 
	   redirected server*/
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

		for (i = 0; i <= nretries; i++)
		{
			if (trace) {
				set_maxforw(buff);
			}
			else if (usrloc && verbose) {
				switch (usrlocstep) {
					case 0:
						printf("registering user %s%i... ", username, namebeg);
						break;
					case 1:
						printf("sending message... ");
						break;
					case 2:
						printf("sending message reply... ");
						break;
				}
			}
			else if (flood && verbose) {
				printf("flooding message number %i\n", i+1);
			}
			else if (randtrash && verbose) {
				printf("message with %i randomized chars\n", i+1);
#ifdef DEBUG
				printf("request:\n%s\n", buff);
#endif
			}
			else if (!trace && !usrloc && !flood && !randtrash && verbose){
				printf("** request **\n%s\n", buff);
			}

			ret = send(sock, buff, strlen(buff), 0);
			(void)gettimeofday(&sendtime, &tz);
			if (ret==-1) {
				perror("send failure");
				exit( 1 );
			}

			if (!flood) {
				tv.tv_sec = retryAfter/1000;
				tv.tv_usec = (retryAfter % 1000) * 1000;

				FD_ZERO(&fd);
				FD_SET(ssock, &fd); 

				ret = select(FD_SETSIZE, &fd, NULL, NULL, &tv);
				(void)gettimeofday(&recvtime, &tz);
				if (ret == 0)
				{
					sockerr.fd=sock;
					sockerr.events=POLLERR;
					if ((poll(&sockerr, 1, 10))==1) {
						if (sockerr.revents && POLLERR) {
							recv(sock, reply, strlen(reply), 0);
							perror("send failure: ");
							if (randtrash) 
								printf ("last message before send failure:\n%s\n", buff);
							exit(1);
						}
					}
					if (trace) printf("%i: timeout after %i ms\n", i, retryAfter);
					else if (verbose) printf("** timeout after %i ms**\n", retryAfter);
					if (i==0) memcpy(&firstsendt, &sendtime, sizeof(struct timeval));
					if (randtrash) {
						printf("did not get a response on this request:\n%s\n", buff);
						if (i+1 < nameend) {
							if (randretrys == 2) {
								printf("sended the following message three times without getting a response:\n%s\ngive up further retransmissions...\n", buff);
								exit(1);
							}
							else {
								printf("resending it without additional random changes...\n\n");
								randretrys++;
							}
						}
					}
					retryAfter = retryAfter * 2;
					if (retryAfter > 5000)
						retryAfter = 5000;
					continue;
				} else if ( ret == -1 ) {
					perror("select error");
					exit(2);
				} /* no timeout, no error ... something has happened :-) */
				else if (FD_ISSET(ssock, &fd)) {
				 	if (!trace && !usrloc && !randtrash)
						printf ("\nmessage received\n");
				} else {
					puts("\nselect returned succesfuly, nothing received\n");
					continue;
				}

				/* we are retrieving only the extend of a decent MSS = 1500 bytes */
				len = sizeof(addr);
				ret = recv(ssock, reply, BUFSIZE, 0);
				if(ret > 0)
				{
					reply[ret] = 0;
					if (redirects && regexec((regex_t*)redexp, reply, 0, 0, 0)==0) {
						printf("** received redirect ");
						if (warning_ext) {
							printf("from ");
							warning_extract(reply);
							printf("\n");
						}
						else printf("\n");
						/* we'll try to handle 301 and 302 here, other 3xx are to complex */
						regcomp(redexp, "^SIP/[0-9]\\.[0-9] 30[1-2] ", REG_EXTENDED|REG_NOSUB|REG_ICASE);
						if (regexec((regex_t*)redexp, reply, 0, 0, 0)==0) {
							/* try to find the contact in the redirect */
							if ((foo=strstr(reply, "Contact"))==NULL) {
								printf("error: cannot find Contact in this redirect:\n%s\n", reply);
								exit(2);
							}
							crlf=strchr(foo, '\n');
							if ((contact=strchr(foo, '\r'))!=NULL && contact<crlf)
								crlf=contact;
							bar=malloc(crlf-foo+1);
							strncpy(bar, foo, crlf-foo);
							sprintf(bar+(crlf-foo), "0");
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
								/* extract the needed information*/
								if ((foo=strchr(crlf,':'))!=NULL){
									*foo='\0';
									foo++;
									rport = atoi(foo);
									if (!rport) {
										printf("error: cannot handle the port in the uri in Contact:\n%s\n", reply);
										exit(2);
									}
								}
								/* correct our request */
								uri_replace(buff, contact);
								if ((foo=strchr(contact,'@'))!=NULL){
									foo++;
									crlf=foo;
								}
								/* get the new destination IP*/
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
					else if (trace) {
						/* in trace we only look for 483, anything else is 
						   treated as the final reply*/
						printf("%i: ", i);
						if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
							printf("* (483) ");
							warning_extract(reply);
							printf(" %.3f ms\n", deltaT(&sendtime, &recvtime));
#ifdef DEBUG
							printf("%s\n", reply);
#endif
							namebeg++;
							maxforw++;
							create_msg(buff, REQ_OPT);
							add_via(buff);
							continue;
						}
						else {
							crlf=strchr(reply,'\n');
							sprintf(crlf, "0");
							crlf++;
							contact=strstr(crlf, "Contact");
							printf("received reply from ");
							warning_extract(reply);
							printf(" after %.3f ms", deltaT(&sendtime, &recvtime));
							if (contact){
								crlf=strchr(contact,'\n');
								sprintf(crlf, "0");
								printf(":\n     %s\n", contact);
							}
							else {
								printf(" without contact:\n     %s\n", reply);
							}
							exit(0);
						}
					}
					else if (usrloc) {
						switch (usrlocstep) {
							case 0:
								/* at first we have sended a register a look at the 
								   response now*/
								if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
									if (verbose)
										printf ("  OK\n");
#ifdef DEBUG
									printf("\n%s\n", reply);
#endif
									strcpy(buff, message);
									usrlocstep=1;
								}
								else {
									if (verbose)
										printf("received:\n%s\n", reply);
									printf("error: didn't received '200 OK' on regsiter. aborting\n");
									exit(1);
								}
								break;
							case 1:
								/* now we sended the message and look if its 
								   forwarded to us*/
								if (!strncmp(reply, messusern, strlen(messusern))) {
									if (verbose) {
										crlf=strstr(reply, "\r\n\r\n");
										crlf=crlf+4;
										printf("         received message\n  '%s'\n", crlf);
									}
#ifdef DEBUG
									printf("\n%s\n", reply);
#endif
									cpy_vias(reply);
									strcpy(buff, mes_reply);
									usrlocstep=2;
								}
								else {
									if (verbose)
										printf("\nreceived:\n%s\n", reply);
									printf("error: didn't received the 'MESSAGE' we sended. aborting\n");
									exit(1);
								}
								break;
							case 2:
								/* finnaly we sended our reply on the message and 
								   look if this is also forwarded to us*/
								while (!strncmp(reply, messusern, strlen(messusern))){
									printf("warning: received 'MESSAGE' retransmission!\n");
									ret = recv(ssock, reply, BUFSIZE, 0);
								}
								if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
									if (verbose)
										printf("   reply received\n\n");
									else
										printf("USRLOC for %s%i completed successful\n", username, namebeg);
									if (namebeg==nameend) {
										printf("All USRLOC tests completed successful.\n");
										exit(0);
									}
									namebeg++;
									create_msg(buff, REQ_REG);
									usrlocstep=0;
								}
								else {
									if (verbose)
										printf("\nreceived:\n%s\n", reply);
									printf("error: didn't received the '200 OK' that we sended as the reply on the message\n");
									exit(1);
								}
							break;
						}
					}
					else if (randtrash) {
						/* in randomzing trash we are expexting 4?? error codes
						 * everything should not be normal */
						if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
#ifdef DEBUG
							printf("received:\n%s\n", reply);
#endif
							if (verbose) printf("received expected 4xx ");
							if (warning_ext) {
								printf ("from ");
								warning_extract(reply);
								printf("\n");
							}
							else printf("\n");
						}
						else {
							printf("warning: did not received 4xx\n");
							if (verbose) printf("sended:\n%s\nreceived:\n%s\n", buff, reply);
						}
						if (nameend==(i+1)) {
							if (randretrys == 0) {
								printf("random end reached. server survived :) respect!\n");
							}
							else {
								printf("maximum sendings reached but did not get a response on this request:\n%s\n", buff);
							}
							exit(0);
						}
						else trash_random(buff);
					}
					else {
						/* in the normal send and reply case anything other 
						   then 1xx will be treated as final response*/
						printf("** reply received ");
						if (i==0) printf("after %.3f ms **\n", deltaT(&sendtime, &recvtime));
						else printf("%.3f ms after first send\n   and %.3f ms after last send **\n", deltaT(&firstsendt, &recvtime), deltaT(&sendtime, &recvtime));
						if (verbose) printf("%s\n", reply);
						if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
							puts(" provisional received; still waiting for a final response\n ");
							continue;
						} else {
							puts(" final received; congratulations!\n ");
							exit(0);
						}
					}
		
				} /* ret > 0 */
				else {
					perror("recv error");
					exit(2);
				}
			} /* !flood */
			else {
				if (namebeg==nretries) {
					printf("flood end reached\n");
					exit(0);
				}
				namebeg++;
				create_msg(buff, REQ_FLOOD);
			}
		} /* for nretries */

	} /* while redirected */
	if (randtrash) exit(0);
	printf("** I give up retransmission....\n");
	exit(1);
}

void print_help() {
	printf("sipsak %s ", SIPSAK_VERSION);
#ifdef DEBUG
	printf("(compiled with DEBUG) ");
#endif
	printf("\n\n"
		" shoot : sipsak [-f filename] -s sip:uri\n"
		" trace : sipsak -t [-f filename] -s sip:uri\n"
		" USRLOC: sipsak -u [-b number] -e number [-E number] -s sip:uri\n"
		" flood : sipsak -F [-c number] -s sip:uri\n"
		" random: sipsak -R [-T number] -s sip:uri\n\n"
		" additional parameter in every modus:\n"
		"                [-d] [-i] [-l port] [-m number] [-n] [-r port] [-v] [-w]\n"
		"   -h           displays this help message\n"
		"   -f filename  the file which contains the SIP message to send\n"
		"   -s sip:uri   the destination server uri in form sip:[user@]servername[:port]\n"
		"   -t           activates the traceroute modus\n"
		"   -u           activates the USRLOC modus\n"
		"   -b number    the starting number appendix to the user name in USRLOC modus\n"
		"                (default: 0)\n"
		"   -e number    the ending numer of the appendix to the user name in USRLOC\n"
		"                modus\n"
		"   -E number    the expires header field value (default: 15)\n"
		"   -F           activates the flood modus\n"
		"   -c number    the maximum CSeq number for flood modus (default: 2^31)\n"
		"   -R           activates the random modues (dangerous)\n"
		"   -T number    the maximum number of trashed character in random modus\n"
		"                (default: request length)\n"
		"   -l port      the local port to use (default: any)\n"
		"   -r port      the remote port to use (default: 5060)\n"
		"   -m number    the value for the max-forwards header field\n"
		"   -n           use IPs instead of fqdn in the Via-Line\n"
		"   -i           deactivate the insertion of a Via-Line\n"
		"   -d           ignore redirects\n"
		"   -v           be more verbose\n"
		"   -w           extract IP from the warning in reply\n\n"
		"The manupulation function are only tested with nice RFC conform SIP-messages,\n"
		"so don't expect them to work with ugly or malformed messages.\n");
	exit(0);
};

int main(int argc, char *argv[])
{
	FILE	*pf;
	char	buff[BUFSIZE];
	int		length, c;
	char	*delim, *delim2;

	/* some initialisation to be shure */
	file_b=uri_b=trace=lport=usrloc=flood=verbose=randtrash=trashchar = 0;
	numeric=warning_ext = 0;
	namebeg=nameend=maxforw = -1;
	via_ins=redirects = 1;
	username = NULL;
	address = 0;
    rport = 5060;
	expires_t = USRLOC_EXP_DEF;
	memset(buff, 0, BUFSIZE);
	memset(message, 0, BUFSIZE);
	memset(mes_reply, 0, BUFSIZE);
	memset(fqdn, 0, FQDN_SIZE);
	memset(messusern, 0, FQDN_SIZE);

	if (argc==1) print_help();

	/* lots of command line switches to handle*/
	while ((c=getopt(argc,argv,"b:c:dE:e:Ff:hil:m:nr:Rs:tT:uvw")) != EOF){
		switch(c){
			case 'b':
				//namebeg=atoi(optarg);
				if ((namebeg=atoi(optarg))==-1) {
					printf("error: non-numerical appendix begin for the username\n");
					exit(2);
				}
				break;
			case 'c':
				if ((namebeg=atoi(optarg))==-1) {
					printf("error: non-numerical CSeq maximum\n");
					exit(2);
				}
				break;
			case 'd':
				redirects=0;
				break;
			case 'E':
				expires_t=atoi(optarg);
				break;
			case 'e':
				//nameend=atoi(optarg);
				if ((nameend=atoi(optarg))==-1) {
					printf("error: non-numerical appendix end for the username\n");
					exit(2);
				}
				break;
			case 'F':
				flood=1;
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
					printf("      or recompile the program with bigger BUFSIZE defined.\n");
					exit(2);
				}
				fclose(pf);
				buff[length] = '\0';
				file_b=1;
				break;
			case 'h':
				print_help();
				break;
			case 'i':
				via_ins=0;
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
				if (maxforw==-1) {
					printf("error: non-numerical number of max-forwards\n");
					exit(2);
				}
				break;
			case 'n':
				numeric = 1;
				break;
			case 'r':
				rport=atoi(optarg);
				if (!rport) {
					printf("error: non-numerical remote port number\n");
					exit(2);
				}
				break;
			case 'R':
				randtrash=1;
				break;
			case 's':
				if (!strncmp(optarg,"sip",3)){
					if ((delim=strchr(optarg,':'))!=NULL){
						delim++;
						if ((delim2=strchr(delim,'@'))!=NULL){
							username=malloc(delim2-delim+1);
							strncpy(username, delim, delim2-delim);
							delim2++;
							delim=delim2;
						}
						if ((delim2=strchr(delim,':'))!=NULL){
							*delim2 = '\0';
							delim2++;
							rport = atoi(delim2);
							if (!rport) {
								printf("error: non-numerical remote port number\n");
								exit(2);
							}
						}
						domainname=malloc(strlen(delim)+1);
						strncpy(domainname, delim, strlen(delim));
						address = getaddress(delim);
						if (!address){
							printf("error:unable to determine the remote host address\n");
							exit(2);
						}
					}
					else{
						printf("error: sip:uri doesn't contain a : ?!\n");
						exit(2);
					}
				}
				else{
					printf("error: sip:uri doesn't not begin with sip\n");
					exit(2);
				}
				uri_b=1;
				break;			break;
			case 't':
				trace=1;
				break;
			case 'T':
				trashchar=atoi(optarg);
				if (!trashchar) {
					printf("error: non-numerical number of trashed character\n");
					exit(2);
				}
				break;
			case 'u':
				usrloc=1;
				break;
			case 'v':
				verbose=1;
				break;
			case 'w':
				warning_ext=1;
				break;
			default:
				printf("error: unknown parameter %c\n", c);
				exit(2);
				break;
		}
	}

	/* lots of conditions to check */
	if (trace) {
		if (usrloc || flood || randtrash) {
			printf("error: trace can't be combined with usrloc, random or flood\n");
			exit(2);
		}
		if (!uri_b) {
			printf("error: for trace modus a sip:uri is realy needed\n");
			exit(2);
		}
		if (file_b) {
			printf("warning: file will be ignored for tracing.");
		}
		if (!username) {
			printf("error: for trace modus without a file the sip:uir have to contain a username\n");
			exit(2);
		}
		if (!via_ins){
			printf("warning: Via-Line is needed for tracing. Ignoring -i\n");
			via_ins=1;
		}
		if (!warning_ext) {
			printf("warning: IP extract from warning activated to be more informational\n");
			warning_ext=1;
		}
		if (maxforw==-1) maxforw=255;
	}
	else if (usrloc) {
		if (trace || flood || randtrash) {
			printf("error: usrloc can't be combined with trace, random or flood\n");
			exit(2);
		}
		if (!username || !uri_b || nameend==-1) {
			printf("error: for the USRLOC modus you have to give a sip:uri with a "
					"username and the\n       username appendix end at least\n");
			exit(2);
		}
		if (via_ins) {
			via_ins=0;
		}
		if (redirects) {
			printf("warning: redirects are not expected in USRLOC. Disableing\n");
			redirects=0;
		}
		if (namebeg==-1)
			namebeg=0;
	}
	else if (flood) {
		if (trace || usrloc || randtrash) {
			printf("error: flood can't be combined with trace, random or usrloc\n");
			exit(2);
		}
		if (!uri_b) {
			printf("error: we need at least a sip uri for flood\n");
			exit(2);
		}
		if (redirects) {
			printf("warning: redirects are not expected in flood. Disableing\n");
			redirects=0;
		}
	}
	else if (randtrash) {
		if (trace || usrloc || flood) {
			printf("error: random can't be combined with trace, flood or usrloc\n");
			exit(2);
		}
		if (!uri_b) {
			printf("error: need at least a sip uri for random\n");
			exit(2);
		}
		if (redirects) {
			printf("warning: redirects are not expected in random. Disableing\n");
			redirects=0;
		}
		if (verbose) {
			printf("warning: random characters may destroy your terminal output\n");
		}
	}
	else {
		if (!uri_b) {
			printf("error: a spi uri is needed at least\n");
			exit(2);
		}
		if (!(username || file_b)) {
			printf("error: ether a file or an username in the sip uri is required\n");
			exit(2);
		}
		
	}
	/* determine our hostname */
	get_fqdn();
	
	/* this is not a cryptographic random number generator,
	   but hey this is only a test-tool => should be satisfying*/
	srand(time(0));

	/* here we go...*/
	shoot(buff);

	/* normaly we won't come back here, but to satisfy the compiler */
	return 0;
}


/*
shoot will exercise the all types of sip servers.
it is not to be used to measure round-trips and general connectivity.
use ping for that. 
written by farhan on 10th august, 2000.
*/

