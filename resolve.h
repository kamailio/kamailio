/*
 * $Id$
 *
 * resolver related functions
 */


#ifndef resolve_h
#define resolve_h

#include <netdb.h>


/* gethostbyname wrappers
 * use this, someday htey will use a local cache */



static inline struct hostent* resolvehost(const char* name)
{
	struct hostent* he;
	
#ifdef DNS_IP_HACK
#endif

	he=gethostbyname(name); /*ipv4*/

#ifdef USE_IPV6
	if(he==0){
		/*try ipv6*/
		he=gethostbyname2(name, AF_INET6);
	}
#endif
	return he;
}



#define rev_resolvehost(ip) gethostbyaddr((ip)->u.addr, (ip)->len, (ip)->af);


#endif
