
#include <stdio.h>
#include <string.h>
#include "../str.h"

#define DBG printf
#define LOG(lev, fmt, args...) printf(fmt, ## args)

struct sip_uri {
	str user;     /* Username */
	str passwd;   /* Password */
	str host;     /* Host name */
	str port;     /* Port number */
	str params;   /* Parameters */
	str headers;  
	unsigned short port_no;
};



int parse_uri(char* buf, int len, struct sip_uri* uri)
{
	enum states  {	URI_INIT, URI_USER, URI_PASSWORD, URI_HOST, URI_HOST_P,
					URI_HOST6_P, URI_HOST6_END, URI_PORT,
					URI_PARAM, URI_HEADERS };
	enum states state;
	char* s;
	char* p;
	char* end;
	char* pass;
	int found_user;
	int error_headers;
	
#define still_at_user  \
						if (found_user==0){ \
							uri->user.s=uri->host.s; \
							if (pass){\
								uri->user.len=pass-uri->user.s; \
								uri->passwd.s=pass+1; \
								uri->passwd.len=p-uri->passwd.s; \
								pass=0; \
							}else{ \
								uri->user.len=p-uri->user.s; \
							}\
							/* everything else is 0 */ \
							uri->host.len=0; \
							uri->port.s=0; \
							uri->port.len=0; \
							uri->params.s=0; \
							uri->params.len=0; \
							uri->headers.s=0; \
							uri->headers.len=0; \
							/* more zeroing */ \
							s=p+1; \
							found_user=1;\
							error_headers=0; \
							state=URI_HOST; \
						}else goto error_bad_char 
#define check_host_end \
					case ':': \
						/* found the host */ \
						uri->host.s=s; \
						uri->host.len=p-s; \
						state=URI_PORT; \
						s=p+1; \
						break; \
					case ';': \
						uri->host.s=s; \
						uri->host.len=p-s; \
						state=URI_PARAM; \
						s=p+1; \
						break; \
					case '?': \
						uri->host.s=s; \
						uri->host.len=p-s; \
						state=URI_HEADERS; \
						s=p+1; \
						break; \
					case '&': \
					case '@': \
						goto error_bad_char 
	
	end=buf+len;
	p=buf+4;
	found_user=0;
	error_headers=0;
	pass=0;
	state=URI_INIT;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure*/
	/*look for sip:*/
	if (len<4) goto error_too_short;
	if (! ( ((buf[0]|0x20)=='s')&&((buf[1]|0x20)=='i')&&((buf[2]|0x20)=='p')&&
		     (buf[3]==':') ) ) goto error_bad_uri;
	
	s=p;
	for(;p<end; p++){
		switch(state){
			case URI_INIT:
				switch(*p){
					case '[':
						/* uri =  [ipv6address]... */
						state=URI_HOST6_P;
						s=p;
						break;
					case ']':
						/* invalid, no uri can start with ']' */
					case ':':
						/* the same as above for ':' */
						goto error_bad_char;
					case '@': /* error no user part, or
								 be forgiving and accept it ? */
					default:
						state=URI_USER;
				}
				break; 
			case URI_USER:
				switch(*p){
					case '@':
						/* found the user*/
						uri->user.s=s;
						uri->user.len=p-s;
						state=URI_HOST;
						found_user=1;
						s=p+1; /* skip '@' */
						break;
					case ':':
						/* found the user, or the host? */
						uri->user.s=s;
						uri->user.len=p-s;
						state=URI_PASSWORD;
						s=p+1; /* skip ':' */
						break;
					case ';':
						/* this could be still the user or
						 * params?*/
						uri->host.s=s;
						uri->host.len=p-s;
						state=URI_PARAM;
						s=p+1;
						break;
					case '?': /* still user or headers? */
						uri->host.s=s;
						uri->host.len=p-s;
						state=URI_HEADERS;
						s=p+1;
						break;
						/* almost anything permitted in the user part */
					case '[':
					case ']': /* the user part cannot contain "[]" */
						goto error_bad_char;
				}
				break;
			case URI_PASSWORD: /* this can also be the port (missing user)*/
				switch(*p){
					case '@':
						/* found the password*/
						uri->passwd.s=s;
						uri->passwd.len=p-s;
						state=URI_HOST;
						found_user=1;
						s=p+1; /* skip '@' */
						break;
					case ';':
						/* upps this is the port */
						uri->port.s=s;
						uri->port.len=p-s;
						/* user contains in fact the host */
						uri->host.s=uri->user.s;
						uri->host.len=uri->user.len;
						uri->user.s=0;
						uri->user.len=0;
						state=URI_PARAM;
						found_user=1; /*  there is no user part */
						s=p+1;
						break;
					case '?':
						/* upps this is the port */
						uri->port.s=s;
						uri->port.len=p-s;
						/* user contains in fact the host */
						uri->host.s=uri->user.s;
						uri->host.len=uri->user.len;
						uri->user.s=0;
						uri->user.len=0;
						state=URI_HEADERS;
						found_user=1; /*  there is no user part */
						s=p+1;
						break;
					case '[':
					case ']':
					case ':':
						goto error_bad_char;
				}
				break;
			case URI_HOST:
				switch(*p){
					case '[':
						state=URI_HOST6_P;
						break;
					case ':': 
					case ';':
					case '?': /* null host name ->invalid */
					case '&':
					case '@': /*chars not allowed in hosts names */
						goto error_bad_host;
					default:
						state=URI_HOST_P;
				}
				break;
			case URI_HOST_P:
				switch(*p){
					check_host_end;
				}
				break;
			case URI_HOST6_END:
				switch(*p){
					check_host_end;
					default: /*no chars allowed after [ipv6] */
						goto error_bad_host;
				}
				break;
			case URI_HOST6_P:
				switch(*p){
					case ']':
						state=URI_HOST6_END;
						break;
					case '[':
					case '&':
					case '@':
					case ';':
					case '?':
						goto error_bad_host;
				}
				break;
			case URI_PORT:
				switch(*p){
					case ';':
						uri->port.s=s;
						uri->port.len=p-s;
						state=URI_PARAM;
						s=p+1;
						break;
					case '?':
						uri->port.s=s;
						uri->port.len=p-s;
						state=URI_HEADERS;
						s=p+1;
						break;
					case '&':
					case '@':
					case ':':
						goto error_bad_char;
				}
				break;
			case URI_PARAM:
				/* supported params:
				 *  maddr, transport, ttl, lr  */
				switch(*p){
					case '@':
						/* ughhh, this is still the user */
						still_at_user;
						break;
					case ';':
						if (pass){
							found_user=1; /* no user, pass cannot contain ';'*/
							pass=0;
						}
						break;
					case '?':
						uri->params.s=s;
						uri->params.len=p-s;
						state=URI_HEADERS;
						s=p+1;
						if (pass){
							found_user=1; /* no user, pass cannot contain '?'*/
							pass=0;
						}
						break;
					case ':':
						if (found_user==0){
							/*might be pass but only if user not found yet*/
							if (pass){
								found_user=1; /* no user */
								pass=0;
							}else{
								pass=p;
							}
						}
						break;
				};
				break;
			case URI_HEADERS:
				/* for now nobody needs them so we completely ignore the 
				 * headers (they are not allowed in request uri) --andrei */
				switch(*p){
					case '@':
						/* yak, we are still at user */
						still_at_user;
						break;
					case ';':
						/* we might be still parsing user, try it */
						if (found_user) goto error_bad_char;
						error_headers=1; /* if this is not the user
											we have an error */
						/* if pass is set => it cannot be user:pass
						 * => error (';') is illegal in a header */
						if (pass) goto error_headers;
						break;
					case ':':
						if (found_user==0){
							/*might be pass but only if user not found yet*/
							if (pass){
								found_user=1; /* no user */
								pass=0;
							}else{
								pass=p;
							}
						}
						break;
					case '?':
						if (pass){
							found_user=1; /* no user, pass cannot conaint '?'*/
							pass=0;
						}
						break;
				}
				break;
			default:
				goto error_bug;
		}
	}
	/*end of uri */
	switch (state){
		case URI_INIT: /* error empy uri */
			goto error_too_short;
		case URI_USER:
			/* this is the host, it can't be the user */
			if (found_user) goto error_bad_uri;
			uri->host.s=s;
			uri->host.len=p-s;
			state=URI_HOST;
			break;
		case URI_PASSWORD:
			/* this is the port, it can't be the passwd */
			if (found_user) goto error_bad_uri;
			uri->port.s=s;
			uri->port.len=p-s;
			uri->host=uri->user;
			uri->user.s=0;
			uri->user.len=0;
			break;
		case URI_HOST_P:
		case URI_HOST6_END:
			uri->host.s=s;
			uri->host.len=p-s;
			break;
		case URI_HOST: /* error: null host */
		case URI_HOST6_P: /* error: unterminated ipv6 reference*/
			goto error_bad_host;
		case URI_PORT:
			uri->port.s=s;
			uri->port.len=p-s;
			break;
		case URI_PARAM:
			uri->params.s=s;
			uri->params.len=p-s;
			break;
		case URI_HEADERS:
			uri->headers.s=s;
			uri->headers.len=p-s;
			if (error_headers) goto error_headers;
			break;
		default:
			goto error_bug;
	}
	
	/* do stuff */
	DBG("parsed uri:\n user=<%.*s>(%d)\n passwd=<%.*s>(%d)\n host=<%.*s>(%d)\n"
			" port=<%.*s>(%d)\n params=<%.*s>(%d)\n headers=<%.*s>(%d)\n",
			uri->user.len, uri->user.s, uri->user.len,
			uri->passwd.len, uri->passwd.s, uri->passwd.len,
			uri->host.len, uri->host.s, uri->host.len,
			uri->port.len, uri->port.s, uri->port.len,
			uri->params.len, uri->params.s, uri->params.len,
			uri->headers.len, uri->headers.s, uri->headers.len
		);
	return 0;
	
error_too_short:
	LOG(L_ERR, "ERROR: parse_uri: uri too short: <%.*s> (%d)\n",
			len, buf, len);
	return -1;
error_bad_char:
	LOG(L_ERR, "ERROR: parse_uri: bad char '%c' in state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			*p, state, (p-buf), buf, (p-buf), len, buf, len);
	return -1;
error_bad_host:
	LOG(L_ERR, "ERROR: parse_uri: bad host in uri (error at char %c in"
			" state %d) parsed: <%.*s>(%d) /<%.*s> (%d)\n",
			*p, state, (p-buf), buf, (p-buf), len, buf, len);
	return -1;
error_bad_uri:
	LOG(L_ERR, "ERROR: parse_uri: bad uri,  state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			 state, (p-buf), buf, (p-buf), len, buf, len);
	return -1;
error_headers:
	LOG(L_ERR, "ERROR: parse_uri: bad uri headers: <%.*s>(%d)"
			" / <%.*s>(%d)\n",
			uri->headers.len, uri->headers.s, uri->headers.len,
			len, buf, len);
	return -1;
error_bug:
	LOG(L_CRIT, "BUG: parse_uri: bad  state %d"
			" parsed: <%.*s> (%d) / <%.*s> (%d)\n",
			 state, (p-buf), buf, (p-buf), len, buf, len);
	return -1;
}



int main (int argc, char** argv)
{

	int r;
	struct sip_uri uri;

	if (argc<2){
		printf("usage:    %s  uri [, uri...]\n", argv[0]);
		exit(1);
	}
	
	for (r=1; r<argc; r++){
		if (parse_uri(argv[r], strlen(argv[r]), &uri)<0){
			printf("error: parsing %s\n", argv[r]);
		}
	}
	return 0;
}
