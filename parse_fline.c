/*
 * $Id$
 * 
 * sip first line parsing automaton
 * 
 */



#include "dprint.h"
#include "msg_parser.h"


/* grammar:
	request  =  method SP uri SP version CRLF
	response =  version SP status  SP reason  CRLF
	(version = "SIP/2.0")
*/

/*known methods: INVITE, ACK, CANCEL, BYE*/

enum{ START,
		INVITE1, INVITE2, INVITE3, INVITE4, INVITE5,
		ACK1,    ACK2,
		CANCEL1, CANCEL2, CANCEL3, CANCEL4, CANCEL5,
		BYE1, BYE2,
		SIP1, SIP2, SIP3, SIP4, SIP5, SIP6,
		FIN_INVITE=100, FIN_ACK, FIN_CANCEL, FIN_BYE, FIN_SIP,
		P_METHOD=200, L_URI, P_URI, L_VER, 
		VER1, VER2, VER3, VER4, VER5, VER6, FIN_VER,
		L_STATUS, P_STATUS, L_REASON, P_REASON,
		L_LF, F_CR, F_LF
};



char* parse_fline(char* buffer, char* end, struct msg_start* fl)
{
	char* tmp;
	register int state;
	unsigned short stat;

	stat=0;
	fl->type=SIP_REQUEST;
	state=START;
	for(tmp=buffer;tmp<end;tmp++){
		switch(*tmp){
			case ' ':
			case '\t':
				switch(state){
					case START: /*allow space at the beginnig, althoug not
								  legal*/
						break;
					case L_URI:
					case L_VER:
					case L_STATUS:
					case L_REASON:
					case L_LF:
						 /*eat  space*/
						break;
					case FIN_INVITE:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_INVITE;
						state=L_URI;
						break;
					case FIN_ACK:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_ACK;
						state=L_URI;
						break;
					case FIN_CANCEL:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_CANCEL;
						state=L_URI;
						break;
					case FIN_BYE:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_BYE;
						state=L_URI;
						break;
					case FIN_SIP:
						*tmp=0;
						fl->u.reply.version.len=tmp-fl->u.reply.version.s;
						state=L_STATUS;
						fl->type=SIP_REPLY;
						break;
					case P_URI:
						*tmp=0;
						fl->u.request.uri.len=tmp-fl->u.request.uri.s;
						state=L_VER;
						break;
					case FIN_VER:
						*tmp=0;
						fl->u.request.version.len=tmp-fl->u.request.version.s;
						state=L_LF;
						break;
					case P_STATUS:
						*tmp=0;
						fl->u.reply.status.len=tmp-fl->u.reply.status.s;
						state=L_REASON;
						break;
					case P_REASON:
					 /*	*tmp=0;
						fl->u.reply.reason.len=tmp-fl->u.reply.reason.s;
						*/
						break;
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					case P_METHOD:
					default:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_OTHER;
						state=L_URI;
				}
				break;
			case 's':
			case 'S':
				switch(state){
					case START:
						state=SIP1;
						fl->u.reply.version.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
						fl->u.request.version.s=tmp;
						state=VER1;
						break;
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
					
			case 'i':
			case 'I':
				switch(state){
					case START:
						state=INVITE1;
						fl->u.request.method.s=tmp;
						break;
					case INVITE3:
						state=INVITE4;
						break;
					case SIP1:
						state=SIP2;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER1:
						state=VER2;
						break;
					case L_VER:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
				
			case 'p':
			case 'P':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP2:
						state=SIP3;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER2:
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			
			case '/':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP3:
						state=SIP4;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER3:
						state=VER4;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case '2':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP4:
						state=SIP5;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
						stat=stat*10+*tmp-'0';
						break;
					case L_STATUS:
						stat=*tmp-'0';
						fl->u.reply.status.s=tmp;
						break;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER4:
						state=VER5;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case '.':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP5:
						state=SIP6;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER5:
						state=VER6;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case '0':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP6:
						state=FIN_SIP;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
						stat=stat*10;
						break;
					case L_STATUS:
						stat=0;
						fl->u.reply.status.s=tmp;
						break;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER6:
						state=FIN_VER;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case 'n':
			case 'N':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE1:
						state=INVITE2;
						break;
					case CANCEL2:
						state=CANCEL3;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'v':
			case 'V':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE2:
						state=INVITE3;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 't':
			case 'T':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE4:
						state=INVITE5;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'e':
			case 'E':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE5:
						state=FIN_INVITE;
						break;
					case CANCEL4:
						state=CANCEL5;
						break;
					case BYE2:
						state=FIN_BYE;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'a':
			case 'A':
				switch(state){
					case START:
						state=ACK1;
						fl->u.request.method.s=tmp;
						break;
					case CANCEL1:
						state=CANCEL2;
						break;
					case BYE2:
						state=FIN_BYE;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'c':
			case 'C':
				switch(state){
					case START:
						state=CANCEL1;
						fl->u.request.method.s=tmp;
						break;
					case CANCEL3:
						state=CANCEL4;
						break;
					case ACK1:
						state=ACK2;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'k':
			case 'K':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case ACK2:
						state=FIN_ACK;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'l':
			case 'L':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case CANCEL5:
						state=FIN_CANCEL;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'b':
			case 'B':
				switch(state){
					case START:
						state=BYE1;
						fl->u.request.method.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'y':
			case 'Y':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case BYE1:
						state=BYE2;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case '\r':
				switch(state){
					case P_REASON:
						*tmp=0;
						fl->u.reply.reason.len=tmp-fl->u.reply.reason.s;
						state=F_CR;
						break;
					case L_LF:
						state=F_CR;
						break;
					case FIN_VER:
						*tmp=0;
						fl->u.request.version.len=tmp-fl->u.request.version.s;
						state=F_CR;
						break;
					case L_REASON:
						state=F_CR;
						break;
					default:
						LOG(L_ERR, "ERROR: parse_first_line: invalid" 
								"message\n");
						goto error;
				}
				break;

			case '\n':
				switch(state){
					case P_REASON:
						*tmp=0;
						fl->u.reply.reason.len=tmp-fl->u.reply.reason.s;
						state=F_LF;
						goto skip;
					case FIN_VER:
						*tmp=0;
						fl->u.request.version.len=tmp-fl->u.request.version.s;
						state=F_LF;
						goto skip;
					case L_REASON:
					case L_LF:
					case F_CR:
						state=F_LF;
						goto skip;
					default:
						LOG(L_ERR, "ERROR: parse_first_line: invalid"
								" message\n");
						goto error;
				}
				break;

			case '1':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
						stat=stat*10+*tmp-'0';
						break;
					case L_STATUS:
						stat=*tmp-'0';
						state=P_STATUS;
						fl->u.reply.status.s=tmp;
						break;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
						
			default:
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LOG(L_ERR, "ERROR: parse_first_line: non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LOG(L_ERR, "ERROR: parse_first_line: invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LOG(L_ERR, "ERROR: parse_first_line: invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
		}
	}
skip:
	if (fl->type==SIP_REPLY){
		fl->u.reply.statuscode=stat;
		/* fl->u.reply.statusclass=stat/100; */
	}
	return tmp;
	
error:
	LOG(L_ERR, "ERROR: while parsing first line (state=%d)\n", state);
	fl->type=SIP_INVALID;
	return tmp;
}
