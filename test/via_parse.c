
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

enum{	         F_HOST,    P_HOST,
		L_PORT,  F_PORT,    P_PORT,
		L_PARAM, F_PARAM,   P_PARAM,
		L_VIA,   F_VIA,
		         F_COMMENT, P_COMMENT,
				 F_CRLF,
				 F_LF
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

	host=port_str=param=comment=next_via=0;

	printf(" %s (%d)\n", argv[0], argc);
	if (argc<2){
			fprintf(stderr, " no parameters\n");
			exit(-1);
	}
	
	
	c_nest=0;
	state=F_HOST;
	for(tmp=argv[1];*tmp;tmp++){
		switch(*tmp){
			case ' ':
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
					case F_CRLF:
					case F_LF:
						/*previous=crlf and now =' '*/
						state=saved_state;
						break;
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>\n",*tmp);
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
					default:
						LOG(L_CRIT,"BUG: parse_via"
							" on <%c>\n",*tmp);
						goto  error;
				}
			break;
			case ':':
				switch(state){
					case F_HOST:
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
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
					case F_COMMENT:
					case P_COMMENT:
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
						/*previous=crlf and now !=' '*/
						goto endofheader;
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
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
						goto error;
					case P_HOST:
					case P_PORT:
						*tmp=0; /*mark the end*/
					case L_PORT:
					case L_PARAM:
						state=F_PARAM;
						break;
					case F_PORT:
					case F_COMMENT:
					case P_COMMENT:
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
						/*previous=crlf and now !=' '*/
						goto endofheader;
					
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
						LOG(L_ERR,"ERROR:parse_via:"
							" no host found\n");
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
					case F_COMMENT:
					case P_COMMENT:
						LOG(L_ERR, "ERROR:parse_via:"
						" invalid char <%c> in state"
						" %d\n", *tmp,state);
						goto error;
					case F_VIA:
						/* do  nothing,  eat ","*/
						break;	
					case F_CRLF:
					case F_LF:
						/*previous=crlf and now !=' '*/
						goto endofheader;
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
						LOG(L_ERR,"ERROR:parse_via"
							" on <%c> state %d\n",
							*tmp, state);
						goto  error;
					case F_CRLF:
					case F_LF:
						/*previous=crlf and now !=' '*/
						goto endofheader;
					default:
						LOG(L_CRIT,"BUG: parse_via"
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
						goto skip;
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
					case F_CRLF:
					case F_LF:
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
goto endofheader;
error:
	fprintf(stderr, "error\n");
	
skip:
	printf("skipping\n");
endofheader:
	printf("end of header reached, state=%d\n", state);
	if (host) printf("host=%s\n", host);
	if (port) printf("port=%s\n", port_str);
	if (param) printf("params=%s\n", param);
	if (comment) printf("comment=%s\n", comment);
	if(next_via) printf("next_via=%s\n", next_via);
	printf("rest=<%s>\n", tmp);
	
	exit(0);
}

