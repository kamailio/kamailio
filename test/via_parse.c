
/* test program -> via parse */


/* parsing:           compact form:
 */

/* 
 * still TODO/test:
 *  - parse next via
 *  - return a list of header structs
 *  - '[' ']' ipv6 parsing!
 *  - return list of params
 *  - test ^\s...'
 *  - add support for parsing via front (SIP/2.0/UDP)
 */

#include <stdio.h>

/* main via states (uri:port ...) */
enum{	         F_HOST,    P_HOST,
		L_PORT,  F_PORT,    P_PORT,
		L_PARAM, F_PARAM,   P_PARAM,
		L_VIA,   F_VIA,
		         F_COMMENT, P_COMMENT,
				 F_IP6HOST, P_IP6HOST,
				 F_CRLF,
				 F_LF,
				 F_CR
	};

/* first via part state */
enum{	         F_SIP=100,
		SIP1, SIP2, FIN_SIP,
		L_VER, F_VER,
		VER1, VER2, FIN_VER,
		L_PROTO, F_PROTO, P_PROTO
	};

#define LOG(lev, fmt, args...) fprintf(stderr, fmt, ## args)

int main(int argc, char** argv)
{

	char* tmp;
	register int state;
	int saved_state;
	int c_nest;
	int i;
	int port;
	char* host;
	char* port_str;
	char* param;
	char* comment;
	char* next_via;
	char *proto; /* in fact transport*/

	host=port_str=param=comment=next_via=proto=0;

	printf(" %s (%d)\n", argv[0], argc);
	if (argc<2){
			fprintf(stderr, " no parameters\n");
			exit(-1);
	}
	
	/* parse start of via ( SIP/2.0/UDP    )*/
	state=F_SIP;
	for(tmp=argv[1];*tmp;tmp++){
		switch(*tmp){
			case ' ':
			case'\t':
				switch(state){
					case L_VER: /* eat space */
					case L_PROTO:
					case F_SIP:
					case F_VER:
					case F_PROTO:
						break;
					case P_PROTO:
						*tmp=0;  /* finished proto parsing */
						state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_SIP:
						*tmp=0;
						state=L_VER;
						break;
					case FIN_VER:
						*tmp=0;
						state=L_PROTO;
						break;
					case F_LF:
					case F_CRLF:
					case F_CR: /* header continues on this line */
						state=saved_state;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '\n':
				switch(state){
					case L_VER:
					case F_SIP:
					case F_VER:
					case F_PROTO:
					case L_PROTO:
						saved_state=state;
						state=F_LF;
						break;
					case P_PROTO:
						*tmp=0;
						state=F_LF;
						saved_state=F_HOST; /* start looking for host*/
						goto main_via;
					case FIN_SIP:
						*tmp=0;
						state=F_LF;
						saved_state=L_VER;
						break;
					case FIN_VER:
						*tmp=0;
						state=F_LF;
						saved_state=L_PROTO;
						break;
					case F_CR:
						state=F_CRLF;
						break;
					case F_LF:
					case F_CRLF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '\r':
				switch(state){
					case L_VER:
					case F_SIP:
					case F_VER:
					case F_PROTO:
					case L_PROTO:
						saved_state=state;
						state=F_CR;
						break;
					case P_PROTO:
						*tmp=0;
						state=F_CR;
						saved_state=F_HOST;
						goto main_via;
					case FIN_SIP:
						*tmp=0;
						state=F_CR;
						saved_state=L_VER;
						break;
					case FIN_VER:
						*tmp=0;
						state=F_CR;
						saved_state=L_PROTO;
						break;
					case F_LF: /*end of line ?next header?*/
					case F_CR:
					case F_CRLF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			
			case '/':
				switch(state){
					case FIN_SIP:
						*tmp=0;
						state=F_VER;
						break;
					case FIN_VER:
						*tmp=0;
						state=F_PROTO;
						break;
					case L_VER:
						state=F_VER;
						break;
					case L_PROTO:
						state=F_PROTO;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
				/* match SIP*/
			case 'S':
			case 's':
				switch(state){
					case F_SIP:
						state=SIP1;
						break;
					/* allow S in PROTO */
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'I':
			case 'i':
				switch(state){
					case SIP1:
						state=SIP2;
						break;
					/* allow i in PROTO */
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case 'p':
			case 'P':
				switch(state){
					case SIP2:
						state=FIN_SIP;
						break;
					/* allow p in PROTO */
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			/*match 2.0*/
			case '2':
				switch(state){
					case F_VER:
						state=VER1;
						break;
					/* allow 2 in PROTO*/
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			case '.':
				switch(state){
					case VER1:
						state=VER2;
						break;
					/* allow . in PROTO */
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				 break;
			case '0':
				switch(state){
					case VER2:
						state=FIN_VER;
						break;
					/* allow 0 in PROTO*/
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
			
			default:
				switch(state){
					case F_PROTO:
						proto=tmp;
						state=P_PROTO;
						break;
					case P_PROTO:
						break;
					default:
						LOG(L_ERR, "ERROR: parse_via: bad char <%c> on"
								" state %d\n", *tmp, state);
						goto error;
				}
				break;
		}
	} /* for tmp*/

/* we should not be here! if everything is ok > main_via*/
	LOG(L_ERR, "ERROR: parse_via: bad via: end of packet on state=%d\n",
			state);
	goto error;

main_via:
/* inc tmp to point to the next char*/
	tmp++;
	c_nest=0;
	/*state should always be F_HOST here*/;
	for(;*tmp;tmp++){
		switch(*tmp){
			case ' ':
			case '\t':
				switch(state){
					case F_HOST:/*eat the spaces*/
						break;
					case P_HOST:
						 *tmp=0;/*mark end of host*/
						 state=L_PORT;
						 break;
					case L_PORT: /*eat the spaces*/
					case F_PORT:
						break;
					case P_PORT:
						*tmp=0; /*end of port */
						state=L_PARAM;
						break;
					case L_PARAM: /* eat the space */
					case F_PARAM:
						break;
					case P_PARAM:
					/*	*tmp=0;*/ /*!?end of param*/
						state=L_PARAM;
						break;
					case L_VIA:
					case F_VIA: /* eat the space */
						break;
					case F_COMMENT:
					case P_COMMENT:
						break;
					case F_IP6HOST: /*eat the spaces*/
						break;
					case P_IP6HOST:
						*tmp=0; /*mark end of host*/
						state=L_PORT; 
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now =' '*/
						state=saved_state;
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>, state=%d\n",*tmp, state);
						goto  error;
				}
			break;
			case '\n':
				switch(state){
					case F_HOST:/*eat the spaces*/
					case L_PORT: /*eat the spaces*/
					case F_PORT:
					case L_PARAM: /* eat the space */
					case F_PARAM:
					case F_VIA: /* eat the space */
					case L_VIA:
					case F_COMMENT:
					case P_COMMENT:
					case F_IP6HOST:
					case P_IP6HOST:
						saved_state=state;
						state=F_LF;
						break;
					case P_HOST:
						 *tmp=0;/*mark end of host*/
						 saved_state=L_PORT;
						 state=F_LF;
						 break;
					case P_PORT:
						*tmp=0; /*end of port */
						saved_state=L_PARAM;
						state=F_LF;
						break;
					case P_PARAM:
					/*	*tmp=0;*/ /*!?end of param*/
						saved_state=L_PARAM;
						state=F_LF;
						break;
					case F_CR:
						state=F_CRLF;
						break;
					case F_CRLF:
					case F_LF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>\n",*tmp);
						goto  error;
				}
			break;
		case '\r':
				switch(state){
					case F_HOST:/*eat the spaces*/
					case L_PORT: /*eat the spaces*/
					case F_PORT:
					case L_PARAM: /* eat the space */
					case F_PARAM:
					case F_VIA: /* eat the space */
					case L_VIA:
					case F_COMMENT:
					case P_COMMENT:
					case F_IP6HOST:
					case P_IP6HOST:
						saved_state=state;
						state=F_CR;
						break;
					case P_HOST:
						 *tmp=0;/*mark end of host*/
						 saved_state=L_PORT;
						 state=F_CR;
						 break;
					case P_PORT:
						*tmp=0; /*end of port */
						saved_state=L_PARAM;
						state=F_CR;
						break;
					case P_PARAM:
					/*	*tmp=0;*/ /*!?end of param*/
						saved_state=L_PARAM;
						state=F_CR;
						break;
					case F_CRLF:
					case F_CR:
					case F_LF:
						state=saved_state;
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>\n",*tmp);
						goto  error;
				}
			break;
			
			case ':':
				switch(state){
					case F_HOST:
					case F_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
						goto error;
					case P_IP6HOST:
						LOG(L_ERR, "ERROR:parse_via: bad ipv6 reference\n");
						goto error;
					case P_HOST:
						*tmp=0; /*mark  end of host*/
						state=F_PORT;
						break;
					case L_PORT:
						state=F_PORT;
						break;
					case P_PORT:
						LOG(L_ERR, "ERROR:parse_via:"
							" bad port\n");
						goto error;
					case L_PARAM:
					case F_PARAM:
					case P_PARAM:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char <%c> in state %d\n",
							*tmp,state);
						goto error;
					case L_VIA:
					case F_VIA:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char in compact via\n");
						goto error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						comment=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT: /*everything is allowed in a comment*/
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto error;
				}
				break;
			case ';':
				switch(state){
					case F_HOST:
					case F_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
						goto error;
					case P_IP6HOST:
						LOG(L_ERR, "ERROR:parse_via: bad ipv6 reference\n");
						goto error;
					case P_HOST:
					case P_PORT:
						*tmp=0; /*mark the end*/
					case L_PORT:
					case L_PARAM:
						state=F_PARAM;
						break;
					case F_PORT:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char <%c> in state %d\n",
							*tmp,state);
						goto error;
					case F_PARAM:
						LOG(L_ERR,  "ERROR:parse_via:"
							" null param?\n");
						goto error;
					case P_PARAM:
						/*hmm next, param?*/
						state=F_PARAM;
						break;
					case L_VIA:
					case F_VIA:
						LOG(L_ERR, "ERROR:parse_via:"
						" bad char <%c> in next via\n",
							*tmp);
						goto error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						comment=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT: /*everything is allowed in a comment*/
						break;
					
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
			break;
			case ',':
				switch(state){
					case F_HOST:
					case F_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
						goto error;
					case P_IP6HOST:
						LOG(L_ERR, "ERROR:parse_via: bad ipv6 reference\n");
						goto error;
					case P_HOST:
					case P_PORT:
						*tmp=0; /*mark the end*/
					case L_PORT:
					case L_PARAM:
					case P_PARAM:
					case L_VIA:
						state=F_VIA;
						break;
					case F_PORT:
					case F_PARAM:
						LOG(L_ERR, "ERROR:parse_via:"
						" invalid char <%c> in state"
						" %d\n", *tmp,state);
						goto error;
					case F_VIA:
						/* do  nothing,  eat ","*/
						break;	
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						comment=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT: /*everything is allowed in a comment*/
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
			break;
			case '(':
				switch(state){
					case F_HOST:
					case F_PORT:
					case F_PARAM:
					case F_VIA:
					case F_IP6HOST:
					case P_IP6HOST: /*must be terminated in ']'*/
						LOG(_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
					case P_HOST:
					case P_PORT:
					case P_PARAM:
						*tmp=0; /*mark the end*/
					case L_PORT:
					case L_PARAM:
					case L_VIA:
						state=F_COMMENT;
						c_nest++;
						printf("found '(', state=%d, c_nest=%d\n",
								state, c_nest);
						*tmp=0;
						break;
					case P_COMMENT:
					case F_COMMENT:
						c_nest++;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
			break;
			case ')':
				switch(state){
					case F_COMMENT:
					case P_COMMENT:
						if (c_nest){
							c_nest--;
							if(c_nest==0){
								state=L_VIA;
								*tmp=0;
								printf("out of comment\n");
								break;
							}
						}else{
							LOG(L_ERR,"ERROR:"
							    "parse_via: "
							    "missing '(' - "
							    "nesting = %d\n",
							    c_nest);
							 goto error;
						}
						break;
					case F_HOST:
					case F_PORT:
					case F_PARAM:
					case F_VIA:
					case P_HOST:
					case P_PORT:
					case P_PARAM:
					case L_PORT:
					case L_PARAM:
					case L_VIA:
					case F_IP6HOST:
					case P_IP6HOST:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
				break;
			case '[':
				switch(state){
					case F_HOST:
						state=F_IP6HOST;
						break;
					case F_COMMENT:/*everything is allowed in a comment*/
						comment=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
				break;
			case ']':
				switch(state){
					case P_IP6HOST:
						*tmp=0; /*mark the end*/
						state=L_PORT;
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					case F_COMMENT:/*everything is allowed in a comment*/
						comment=tmp;
						state=P_COMMENT;
						break;
					case P_COMMENT:
						break;
					default:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
				}
				break;
						
			default:
				switch(state){
					case F_HOST:
						state=P_HOST;
						host=tmp;
						break;
					case P_HOST:
						break;
					case F_PORT:
						state=P_PORT;
						port_str=tmp;
						break;
					case P_PORT:
						/*check if number?*/
						break;
					case F_PARAM:
						state=P_PARAM;
						param=tmp;
						break;
					case P_PARAM:
						break;
					case F_VIA:
						next_via=tmp;
						printf("found new via on <%c>\n", *tmp);
						goto nextvia;
					case L_PORT:
					case L_PARAM:
					case L_VIA:
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d (default)\n",
							*tmp, state);
						goto  error;
					case F_COMMENT:
						printf("starting comment parsing on %c \n",*tmp);
						state=P_COMMENT;
						comment=tmp;
						break;
					case P_COMMENT:
						break;
					case F_IP6HOST:
						state=P_IP6HOST;
						host=tmp;
						break;
					case P_IP6HOST:
						break;
					case F_CRLF:
					case F_LF:
					case F_CR:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_ERR, "BUG:parse_via:"
							" invalid char <%c>"
							" in state %d\n",
							*tmp, state);
						goto error;
				}
				
					
		}			
	}

	printf("end of packet reached, state=%d\n", state);
	goto endofpacket; /*end of packet, probably should be goto error*/
	
endofheader:
	state=saved_state;
	printf("end of header reached, state=%d\n", state);
endofpacket:
	/* check if error*/
	switch(state){
		case P_HOST:
		case L_PORT:
		case P_PORT:
		case L_PARAM:
		case P_PARAM:
		case L_VIA:
			break;
		default:
			LOG(L_ERR, "ERROR: parse_via: invalid via - end of header in"
					" state %d\n", state);
			goto error;
	}
		
nextvia:
	if (proto) printf("<SIP/2.0/%s<\n", proto);
	if (host) printf("host= <%s>\n", host);
	if (port_str) printf("port= <%s>\n", port_str);
	if (param) printf("params= <%s>\n", param);
	if (comment) printf("comment= <%s>\n", comment);
	if(next_via) printf("next_via= <%s>\n", next_via);
	printf("rest=<%s>\n", tmp);
	
	exit(0);

error:
	fprintf(stderr, "via parse error\n");
	exit(-1);
}

