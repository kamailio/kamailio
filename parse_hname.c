/*
 * $Id$
 *
 * header name parsing automaton:

 * parsing:           compact form:
 * Via:               v:
 * From:              f:
 * To:                t:
 * Cseq:              n/a
 * Call-ID:           i:
 * Contact:           m:
 * Max-Forwards:      n/a
 * Route:             n/a
 * Record-Route:      n/a
 */


#include "msg_parser.h"
#include "dprint.h"

enum { INITIAL=0,
		VIA1, VIA2,
		FROM1, FROM2, FROM3,
		TO1,
		C_START, CSEQ2, CSEQ3,
		         CALLID2, CALLID3, CALLID4, CALLID5, CALLID6,
		         CONTACT2, CONTACT3, CONTACT4, CONTACT5, CONTACT6,
		M_START,      MAXFORWARDS2, MAXFORWARDS3, MAXFORWARDS4, MAXFORWARDS5,
		MAXFORWARDS6, MAXFORWARDS7, MAXFORWARDS8, MAXFORWARDS9, MAXFORWARDS10,
		MAXFORWARDS11,
		ROUTE1, ROUTE2, ROUTE3, ROUTE4,
                RECROUTE1, RECROUTE2, RECROUTE3, RECROUTE4, RECROUTE5, 
                RECROUTE6, RECROUTE7, RECROUTE8, RECROUTE9, RECROUTE10,
       
		/* final states*/
		F_VIA=1000, F_FROM, F_TO, F_CSEQ, F_CALLID, F_CONTACT, F_MAXFORWARDS,
                F_ROUTE, F_RECROUTE,
		I_START,

		UNKNOWN_HEADER=200,
		BODY=220,
		LF=25000,
	};


/* returns end or pointer to next elem*/
char* parse_hname1(char* p, char* end, struct hdr_field* hdr)
{

	char* t;
	register int state;
	int i;
	int err;
	err=0;

	state=INITIAL;
		
	for(t=p;t<end;t++){
		switch(*t){
			case 'V':
			case 'v':
					switch(state){
						case INITIAL:
							state=VIA1;
							hdr->name.s=t;
							break;
						case UNKNOWN_HEADER: break;
						default:
							state=UNKNOWN_HEADER;
					}
					break;
			case 'I':
			case 'i':
					switch(state){
						case VIA1:
							state=VIA2;
							break;
						case CALLID5:
							state=CALLID6;
							break;
						case INITIAL:
							state=I_START;
							hdr->name.s=t;
							break;
						case UNKNOWN_HEADER: break;
						default:
							state=UNKNOWN_HEADER;
					}
					break;
			case 'A':
			case 'a':
					switch(state){
						case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
						case VIA2:
								state=F_VIA;
								break;
						case C_START:  /*CALLID1*/
								state=CALLID2;
								break;
						case CONTACT4:
								state=CONTACT5;
								break;
						case M_START:
								state=MAXFORWARDS2;
								break;
						case MAXFORWARDS8:
								state=MAXFORWARDS9;
								break;
						case UNKNOWN_HEADER: break;
						default:
								state=UNKNOWN_HEADER;
					}
					break;
			case 'F':
			case 'f':
					switch(state){
						case INITIAL:
								state=FROM1;
								hdr->name.s=t;
								break;
						case MAXFORWARDS4:
								state=MAXFORWARDS5;
								break;
						case UNKNOWN_HEADER: break;
						default:
								state=UNKNOWN_HEADER;
					}
					break;
			case 'R':
			case 'r':
					switch(state){
						case INITIAL:
							state=ROUTE1;
							hdr->name.s=t;
							break;
						case FROM1:
							state=FROM2;
							break;
						case MAXFORWARDS6:
							state=MAXFORWARDS7;
							break;
						case MAXFORWARDS9:
							state=MAXFORWARDS10;
							break;
					        case RECROUTE3:
							state=RECROUTE4;
							break;
					        case RECROUTE6:
							state=RECROUTE7;
						case UNKNOWN_HEADER: break;
						default:
							state=UNKNOWN_HEADER;
					}
					break;
			case 'O':
			case 'o':
					switch(state){
						case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
						case FROM2:
							state=FROM3;
							break;
						case TO1:
							state=F_TO;
							break;
						case C_START: /*CONTACT1 */
							state=CONTACT2;
							break;
						case ROUTE1:
							state=ROUTE2;
							break;
						case MAXFORWARDS5:
							state=MAXFORWARDS6;
							break;
					        case RECROUTE2:
							state=RECROUTE3;
							break;
					        case RECROUTE7:
							state=RECROUTE8;
							break;
						case UNKNOWN_HEADER: break;
						default:
							state=UNKNOWN_HEADER;
					}
					break;
			case 'M':
			case 'm':
						switch(state){
							case INITIAL:
								state=M_START;
								hdr->name.s=t;
								break;
							case FROM3:
								state=F_FROM;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'T':
			case 't':
						switch(state){
							case INITIAL:
								state=TO1;
								hdr->name.s=t;
								break;
							case CONTACT3:
								state=CONTACT4;
								break;
							case CONTACT6:
								state=F_CONTACT;
								break;
							case ROUTE3:
								state=ROUTE4;
								break;
						        case RECROUTE9:
								state=RECROUTE10;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'C':
			case 'c':
						switch(state){
							case INITIAL:
								state=C_START;
								hdr->name.s=t;
								break;
							case CONTACT5:
								state=CONTACT6;
								break;
						        case RECROUTE1:
								state=RECROUTE2;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'S':
			case 's':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case C_START:
								state=CSEQ2;
								break;
							case MAXFORWARDS11:
								state=F_MAXFORWARDS;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'E':
			case 'e':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case CSEQ2:
								state=CSEQ3;
								break;
							case ROUTE4:
								state=F_ROUTE;
								break;
						        case ROUTE1:
								state=RECROUTE1;
								break;
						        case RECROUTE10:
								state=F_RECROUTE;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'Q':
			case 'q':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case CSEQ3:
								state=F_CSEQ;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'L':
			case 'l':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case CALLID2:
								state=CALLID3;
								break;
							case CALLID3:
								state=CALLID4;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'D':
			case 'd':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case CALLID6:
								state=F_CALLID;
								break;
							case MAXFORWARDS10:
								state=MAXFORWARDS11;
								break;
						        case RECROUTE4:
								state=RECROUTE5;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'N':
			case 'n':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case CONTACT2:
								state=CONTACT3;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'X':
			case 'x':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case MAXFORWARDS2:
								state=MAXFORWARDS3;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case '-':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case CALLID4:
								state=CALLID5;
								break;
							case MAXFORWARDS3:
								state=MAXFORWARDS4;
								break;
						        case RECROUTE5:
								state=RECROUTE6;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'W':
			case 'w':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case MAXFORWARDS7:
								state=MAXFORWARDS8;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'U':
			case 'u':
						switch(state){
							case INITIAL:
								state=UNKNOWN_HEADER;
								hdr->name.s=t;
								break;
							case ROUTE2:
								state=ROUTE3;
								break;
						        case RECROUTE8:
								state=RECROUTE9;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case ' ':
						switch(state){
							case INITIAL:
								break; /*eat space */
							case F_VIA:
							case F_FROM:
							case F_TO:
							case F_CSEQ:
							case F_CALLID:
							case F_CONTACT:
							case F_MAXFORWARDS:
						        case F_ROUTE:
						        case F_RECROUTE:
								break; /* eat trailing space*/
							case VIA1:
								/*compact form: v: */
								state=F_VIA;
								break;
							case FROM1:
								/*compact form f:*/
								state=F_FROM;
								break;
							case TO1:
								/*compact form t:*/
								state=F_TO;
								break;
							case I_START:
								/*compact form i: (Call-ID)*/
								state=F_CALLID;
								break;
							case M_START:
								/*compact form m: (Contact)*/
								state=F_CONTACT;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case ':':
						switch(state){
							case F_VIA:
							case VIA1: /* compact form*/
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_VIA;
								goto skip;
							case F_FROM:
							case FROM1: /*compact form*/
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_FROM;
								goto skip;
							case F_TO:
							case TO1: /*compact form*/
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_TO;
								goto skip;
							case F_CSEQ:
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_CSEQ;
								goto skip;
							case F_CALLID:
							case I_START: /*compact form*/
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_CALLID;
								goto skip;
							case F_CONTACT:
							case M_START: /*compact form*/
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_CONTACT;
								goto skip;
							case F_MAXFORWARDS:
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_MAXFORWARDS;
								goto skip;
							case F_ROUTE:
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_ROUTE;
								goto skip;
						        case F_RECROUTE:
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_RECORDROUTE;
								goto skip;
							case UNKNOWN_HEADER:
								*t=0;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_OTHER;
								goto skip;
							default:
								/* unknown header, e.g: "c:"*/
								*t=0;
								DBG("unknown header found, prev. state=%d\n",
										state);
								state=UNKNOWN_HEADER;
								hdr->name.len=t-hdr->name.s;
								hdr->type=HDR_OTHER;
								goto skip;
						}
						break;
						
			case '\n':
			case '\r': /*not allowed in hname*/
						goto error;
			
			default:
					switch(state){
						case INITIAL:
							hdr->name.s=t;
							state=UNKNOWN_HEADER;
							break;
						case UNKNOWN_HEADER:
							break;
						default:
							state=UNKNOWN_HEADER;
					}
		}
	}
/* if we are here => we didn't find ':' => error*/
	
error:
	hdr->type=HDR_ERROR;
	return t;

skip:
	DBG("end of header name found, state=%d\n", state);
	t++;
	return t;

}

