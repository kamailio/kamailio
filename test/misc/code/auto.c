
/* test program -> switch speed */
/*
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */



/* parsing:           compact form:
 * Via:               v:
 * From:              f:
 * To:                t:
 * Cseq:              n/a
 * Call-ID:           i:
 * Contact:           m:
 * Max-Forwards:      n/a
 * Route:             n/a
 */

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
		/* final states*/
		F_VIA=1000, F_FROM, F_TO, F_CSEQ, F_CALLID, F_CONTACT, F_MAXFORWARDS,
		F_ROUTE,
		I_START,

		UNKNOWN_HEADER=200,
		BODY=220,
		LF=25000,
	};

#include <stdio.h>

int main(int argc, char** argv)
{

	char* t;
	register int state;
	int i;
	int err;
	err=0;

	state=INITIAL;
	printf(" %s (%d)\n", argv[0], argc);
	if (argc<2){
			fprintf(stderr, " no parameters\n");
			exit(-1);
	}
	
	for (i=0;i<10000000;i++){
		
	for(t=argv[1];*t;t++){
		switch(*t){
			case 'V':
			case 'v':
					switch(state){
						case INITIAL:
							state=VIA1;
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
							break;
						case UNKNOWN_HEADER: break;
						default:
							state=UNKNOWN_HEADER;
					}
					break;
			case 'A':
			case 'a':
					switch(state){
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
						case UNKNOWN_HEADER: break;
						default:
							state=UNKNOWN_HEADER;
					}
					break;
			case 'O':
			case 'o':
					switch(state){
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
								break;
							case CONTACT5:
								state=CONTACT6;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'S':
			case 's':
						switch(state){
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
							case CSEQ2:
								state=CSEQ3;
								break;
							case ROUTE4:
								state=F_ROUTE;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'Q':
			case 'q':
						switch(state){
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
							case CALLID6:
								state=F_CALLID;
								break;
							case MAXFORWARDS10:
								state=MAXFORWARDS11;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'N':
			case 'n':
						switch(state){
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
							case CALLID4:
								state=CALLID5;
								break;
							case MAXFORWARDS3:
								state=MAXFORWARDS4;
								break;
							case UNKNOWN_HEADER: break;
							default:
								state=UNKNOWN_HEADER;
						}
						break;
			case 'W':
			case 'w':
						switch(state){
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
							case ROUTE2:
								state=ROUTE3;
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
							case F_FROM:
							case FROM1: /*compact form*/
							case F_TO:
							case TO1: /*compact form*/
							case F_CSEQ:
							case F_CALLID:
							case I_START: /*compact form*/
							case F_CONTACT:
							case M_START: /*compact form*/
							case F_MAXFORWARDS:
							case F_ROUTE:
							//	printf("found header, state=%d\n", state);
								state=INITIAL; /* reset to test*/
								break;
							case UNKNOWN_HEADER:
							default:
								/*printf("found unknown header, state=%d\n", 
											state);*/
								err=1;
								state=INITIAL;
						}
						break;
			default:
					/*fprintf(stderr, "Unexpected char <%c> encountered"
										" state=%d\n", *t, state);
						exit(-1);*/
					state=UNKNOWN_HEADER;
		}
	}

	} //for i
	if (err) printf("Error unknown header\n");
	printf("final state=%d\n", state);

	exit(0);

}

