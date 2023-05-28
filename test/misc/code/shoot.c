/*
shot written by ashhar farhan, is not bound by any licensing at all.
you are free to use this code as you deem fit. Just don't blame the author
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

#include <regex.h>
regex_t* regexp;

#define RESIZE		1024

/* take either a dot.decimal string of ip address or a 
domain name and returns a NETWORK ordered long int containing
the address. i chose to internally represent the address as long for speedier
comparisons.

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

	/* three dots with up to three digits in before, between and after ? */
	if (dotcount == 3 && i > 0 && i <= 3)
		return inet_addr(host);

	/* try the system's own resolution mechanism for dns lookup:
	 required only for domain names.
	 inspite of what the rfc2543 :D Using SRV DNS Records recommends,
	 we are leaving it to the operating system to do the name caching.

	 this is an important implementation issue especially in the light
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


/*
shoot:
takes:
	1. the text message of buff to 
	2. the address (network ordered byte order)
	3. and port (not network byte ordered).

starting from half a second, times-out on replies and
keeps retrying with exponential back-off that flattens out
at 5 seconds (5000 milliseconds).

* Does not stop sending unless a final response is received.
we are detecting the final response without a '1' as the first
letter.
*/
void shoot(char *buff, long address, int lport, int rport )
{
	struct sockaddr_in	addr;
	/* jku - b  server structures */
	struct sockaddr_in	sockname;
	int ssock;
	/*
	char compiledre[ RESIZE ];
	*/
	/* jku - e */
	int retryAfter = 500, i, len, ret;
	int	nretries = 10;
	int	sock;
	struct timeval	tv;
	fd_set	fd;
	char	reply[1600];

	/* create a socket */
	sock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock==-1) {
		perror("no client socket");
		exit(2);
	}

	/* jku - b */
	ssock = (int)socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock==-1) {
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

	/* should capture: SIP/2.0 100 Trying */
	/* compile("^SIP/[0-9]\\.[0-9] 1[0-9][0-9] ", compiledre, &compiledre[RESIZE], '\0'); */
	regexp=(regex_t*)malloc(sizeof(regex_t));
	regcomp(regexp, "^SIP/[0-9]\\.[0-9] 1[0-9][0-9] ", REG_EXTENDED|REG_NOSUB|REG_ICASE); 
	

	/* jku - e */

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
	/* jku - e */

	for (i = 0; i < nretries; i++)
	{
		puts("/* request */");
		puts(buff);
		putchar('\n');

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
			puts("\n/* timeout */\n");
			retryAfter = retryAfter * 2;
			if (retryAfter > 5000)
				retryAfter = 5000;
			/* we should have retrieved the error code and displayed
			we are not doing that because there is a great variation
			in the process of retrieving error codes between
			micro$oft and *nix world*/
			continue;
		} else if ( ret == -1 ) {
			perror("select error");
			exit(2);
		} /* no timeout, no error ... something has happened :-) */
                 else if (FD_ISSET(ssock, &fd)) {
			puts ("\nmessage received\n");
		} else {
			puts("\nselect returned successfully, nothing received\n");
			continue;
		}

		/* we are retrieving only the extend of a decent MSS = 1500 bytes */
		len = sizeof(addr);
		ret = recv(ssock, reply, 1500, 0);
		if(ret > 0)
		{
			reply[ret] = 0;
			puts("/* reply */");
			puts(reply);
			putchar('\n');
			/* if (step( reply, compiledre )) { */
			if (regexec((regex_t*)regexp, reply, 0, 0, 0)==0) {
				puts(" provisional received; still waiting for a final response\n ");
				continue;
			} else {
				puts(" final received; congratulations!\n ");
				exit(0);
			}
		
		} 
		else	{
			perror("recv error");
			exit(2);
			}
	}
	/* after all the retries, nothing has come back :-( */
	puts("/* I give up retransmission....");
	exit(1);
}

int main(int argc, char *argv[])
{
	long	address;
	FILE	*pf;
	char	buff[1600];
	int		length;
	int	lport=0;
	int	rport=5060;

	if (! (argc >= 3 && argc <= 5))
	{
		puts("usage: shoot file host [rport] [lport]");
		exit(2);
	}

	address = getaddress(argv[2]);
	if (!address)
	{
		puts("error:unable to determine the remote host address.");
		exit(2);
	}

	/* take the port as 5060 even if it is incorrectly specified */
	if (argc >= 4)
	{
		rport = atoi(argv[3]);
		if (!rport) {
			puts("error: non-numerical remote port number");
			exit(1);
		}
		if (argc==5) {
			lport=atoi(argv[4]);
			if (!lport) {
				puts("error: non-numerical local port number");
				exit(1);
			}
		}
	}

	/* file is opened in binary mode so that the cr-lf is preserved */
	pf = fopen(argv[1], "rb");
	if (!pf)
	{
		puts("unable to open the file.\n");
		return 1;
	}
	length  = fread(buff, 1, sizeof(buff), pf);
	if (length >= sizeof(buff))
	{
		puts("error:the file is too big. try files of less than 1500 bytes.");
		return 1;
	}
	fclose(pf);
	buff[length] = 0;

	shoot(buff, address, lport, rport );

	/* visual studio closes the debug console as soon as the 
	program terminates. this is to hold the window from collapsing
	Uncomment it if needed.
	getchar();*/
	

	return 0;
}


/*
shoot will exercise all the types of sip servers.
it is not to be used to measure round-trips and general connectivity.
use ping for that. 
written by farhan on 10th august, 2000.

TO-DO:
1. replace the command line arguments with just a sip url like this:
	shoot invite.txt sip:farhan@sip.hotfoon.com:5060

2. understand redirect response and retransmit to the redirected server.

*/

