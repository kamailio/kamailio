/*
 * $Id$
 */


#ifndef _CRC_H_
#define _CRC_H_

extern unsigned long int crc_32_tab[];
extern unsigned short int ccitt_tab[];
extern unsigned short int crc_16_tab[];

#endif


#include "hash_func.h"
#include "../../dprint.h"
#include "../../crc.h"
#include "../../ut.h"

int old_hash( str  call_id, str cseq_nr )
{
   int  hash_code = 0;
   int  i;
	
#ifdef i386
   int ci_len, cs_len;
   char *ci, *cs;
   
	trim_len( ci_len, ci, call_id );
	trim_len( cs_len, cs, cseq_nr );

		int dummy1;
		if (call_id.len>=4){
			asm(
				"1: \n\r"
				"addl (%1), %0 \n\r"
				"add $4, %1 \n\r"
				"cmp %2, %1 \n\r"
				"jl 1b  \n\r"
				: "=r"(hash_code), "=r"(dummy1)
				:  "0" (hash_code), "1"(ci),
				"r"( (ci_len & (~3)) +ci)
			);
		}
#else
    if ( call_id.len>0 )
      for( i=0 ; i<call_id.len ; hash_code+=call_id.s[i++]  );
#endif

#ifdef i386

		int dummy2;
		if (cseq_nr.len>=4){
			asm(
				"1: \n\r"
				"addl (%1), %0 \n\r"
				"add $4, %1 \n\r"
				"cmp %2, %1 \n\r"
				"jl 1b  \n\r"
				: "=r"(hash_code), "=r"(dummy2)
				:  "0" (hash_code), "1"(cs),
				"r"((cs_len & (~3) )+ cs)
			);
		}
#else
    if ( cseq_nr.len>0 )
      for( i=0 ; i<cseq_nr.len ; hash_code+=cseq_nr.s[i++] );
#endif
   return hash_code &= (TABLE_ENTRIES-1); /* TABLE_ENTRIES = 2^k */
}

int new_hash( str call_id, str cseq_nr )
{
	int hash_code = 0;
	int i,j, k, third;
	int ci_len, cs_len;
	char c;
	char *ci, *cs;

	/* trim EoLs */
/*
	ci_len = call_id.len;
	while (ci_len && ((c=call_id.s[ci_len-1])==0 || c=='\r' || c=='\n'))
		ci_len--;
	cs_len = cseq_nr.len;
	while (cs_len && ((c=cseq_nr.s[cs_len-1])==0 || c=='\r' || c=='\n'))
		cs_len--;
*/
	trim_len( ci_len, ci, call_id );
	trim_len( cs_len, cs, cseq_nr );

	/* run the cycle from the end ... we are interested in the
	   most-right digits ... and just take the %10 value of it
	*/
	third=(ci_len-1)/3;
	for ( i=ci_len-1, j=2*third, k=third;
		k>0 ; i--, j--, k-- ) {
		hash_code+=crc_16_tab[*(ci+i) /*+7*/ ]+
			ccitt_tab[*(ci+k)+63]+	
			ccitt_tab[*(ci+j)+13];
	}
	for( i=0 ; i<cs_len ; i++ )
		//hash_code+=crc_32_tab[(cseq_nr.s[i]+hash_code)%243];
		hash_code+=ccitt_tab[*(cs+i)+123];

	hash_code &= (TABLE_ENTRIES-1); /* TABLE_ENTRIES = 2^k */
   	return hash_code;
}

void hashtest_cycle( int hits[TABLE_ENTRIES], char *ip )
{
	long int i,j,k, l;
	int len1, len2, hashv;
	static char buf1[1024];
	static char buf2[1024];
	str call_id; 
	str cseq;

	call_id.s=buf1;
	cseq.s=buf2;

	for (i=987654328;i<987654328+10;i++)
		for (j=85296341;j<85296341+10;j++)
			for (k=987654;k<=987654+10;k++)
				for (l=101;l<201;l++) {
					call_id.len=sprintf( buf1, "%d-%d-%d@%s", i,j,k, ip );
					cseq.len=sprintf( buf2, "%d", l );
					printf("%s\t%s\n", buf1, buf2 );
					hashv=hash( call_id, cseq );
					hits[ hashv ]++;
				}
}

void hashtest()
{
	int hits[TABLE_ENTRIES];
	int i;
	
	memset( hits, 0, sizeof hits );
	hashtest_cycle( hits, "192.168.99.100" );
	hashtest_cycle( hits, "172.168.99.100" );
	hashtest_cycle( hits, "142.168.99.100" );
	for (i=0; i<TABLE_ENTRIES; i++)
		printf("[%d. %d]\n", i, hits[i] );
	exit(0);
}

