/*
 * $Id$
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 *  Gabber
 *  Copyright (C) 1999-2000 Dave Smith & Julian Missig
 *
 */



/* 
   Implements the Secure Hash Algorithm (SHA1)

   Copyright (C) 1999 Scott G. Miller

   Released under the terms of the GNU General Public License v2
   see file COPYING for details

   Credits: 
      Robert Klep <robert@ilse.nl>  -- Expansion function fix 
	  Thomas "temas" Muldowney <temas@box5.net>:
	  		-- shahash() for string fun
			-- Will add the int32 stuff in a few
	  		
   ---
   FIXME: This source takes int to be a 32 bit integer.  This
   may vary from system to system.  I'd use autoconf if I was familiar
   with it.  Anyone want to help me out?
*/

//#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#ifndef MACOS
#  include <sys/stat.h>
#  include <sys/types.h>
#endif
#ifndef WIN32
#  include <unistd.h>
#  define INT64 long long
#else
#  define snprintf _snprintf
#  define INT64 __int64
#endif

#define switch_endianness(x) (x<<24 & 0xff000000) | \
                             (x<<8  & 0x00ff0000) | \
                             (x>>8  & 0x0000ff00) | \
                             (x>>24 & 0x000000ff)

/* Initial hash values */
#define Ai 0x67452301 
#define Bi 0xefcdab89
#define Ci 0x98badcfe
#define Di 0x10325476
#define Ei 0xc3d2e1f0

/* SHA1 round constants */
#define K1 0x5a827999
#define K2 0x6ed9eba1
#define K3 0x8f1bbcdc 
#define K4 0xca62c1d6

/* Round functions.  Note that f2() is used in both rounds 2 and 4 */
#define f1(B,C,D) ((B & C) | ((~B) & D))
#define f2(B,C,D) (B ^ C ^ D)
#define f3(B,C,D) ((B & C) | (B & D) | (C & D))

/* left circular shift functions (rotate left) */
#define rol1(x) ((x<<1) | ((x>>31) & 1))
#define rol5(A) ((A<<5) | ((A>>27) & 0x1f))
#define rol30(B) ((B<<30) | ((B>>2) & 0x3fffffff))

/*
  Hashes 'data', which should be a pointer to 512 bits of data (sixteen
  32 bit ints), into the ongoing 160 bit hash value (five 32 bit ints)
  'hash'
*/
int 
sha_hash(int *data, int *hash)  
{
  int W[80];
  unsigned int A=hash[0], B=hash[1], C=hash[2], D=hash[3], E=hash[4];
  unsigned int t, x, TEMP;

  for (t=0; t<16; t++) 
    {
#ifndef WORDS_BIGENDIAN
      W[t]=switch_endianness(data[t]);
#else 
      W[t]=data[t];
#endif
    }


  /* SHA1 Data expansion */
  for (t=16; t<80; t++) 
    {
      x=W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16];
      W[t]=rol1(x);
    }

  /* SHA1 main loop (t=0 to 79) 
   This is broken down into four subloops in order to use
   the correct round function and constant */
  for (t=0; t<20; t++) 
    {
      TEMP=rol5(A) + f1(B,C,D) + E + W[t] + K1;
      E=D;
      D=C;
      C=rol30(B);
      B=A;
      A=TEMP;
    }
  for (; t<40; t++) 
    {
      TEMP=rol5(A) + f2(B,C,D) + E + W[t] + K2;
      E=D;
      D=C;
      C=rol30(B);
      B=A;
      A=TEMP;
    }
  for (; t<60; t++) 
    {
      TEMP=rol5(A) + f3(B,C,D) + E + W[t] + K3;
      E=D;
      D=C;
      C=rol30(B);
      B=A;
      A=TEMP;
    }
  for (; t<80; t++) 
    {
      TEMP=rol5(A) + f2(B,C,D) + E + W[t] + K4;
      E=D;
      D=C;
      C=rol30(B);
      B=A;
      A=TEMP;
    }
  hash[0]+=A; 
  hash[1]+=B;
  hash[2]+=C;
  hash[3]+=D;
  hash[4]+=E;
  return 0;
}

/*
  Takes a pointer to a 160 bit block of data (five 32 bit ints) and
  initializes it to the start constants of the SHA1 algorithm.  This
  must be called before using hash in the call to sha_hash
*/
int 
sha_init(int *hash) 
{
  hash[0]=Ai;
  hash[1]=Bi;
  hash[2]=Ci;
  hash[3]=Di;
  hash[4]=Ei;
  return 0;
}

int strprintsha(char *dest, int *hashval) 
{
	int x;
	char *hashstr = dest;
	for (x=0; x<5; x++) 
	{
		snprintf(hashstr, 9, "%08x", hashval[x]);
		hashstr+=8;
	}
	/*old way */
	//snprintf(hashstr++, 1, "\0");
	/*new way - by bogdan*/
	*hashstr = 0;

	return 0;
}

char *shahash(const char *str) 
{
	char read_buffer[65];
	//int read_buffer[64];
	int c=1, i;
       
	INT64 length=0;

	int strsz;
	static char final[40];
	int *hashval;

	hashval = (int *)malloc(20);

	sha_init(hashval);

	strsz = strlen(str);

	if(strsz == 0) 
	{
	     memset(read_buffer, 0, 65);
	     read_buffer[0] = 0x80;
	     sha_hash((int *)read_buffer, hashval);
	}

	while (strsz>0) 
	{
		memset(read_buffer, 0, 65);
		strncpy((char*)read_buffer, str, 64);
		c = strlen((char *)read_buffer);
		length+=c;
		strsz-=c;
		if (strsz<=0) 
		{
			length<<=3;	
			read_buffer[c]=(char)0x80;
			for (i=c+1; i<64; i++) 
				read_buffer[i]=0;
			if (c>55) 
			{
				/* we need to do an entire new block */
				sha_hash((int *)read_buffer, hashval);
				for (i=0; i<14; i++) 
					((int*)read_buffer)[i]=0;
			}      
#ifndef WORDS_BIGENDIAN
			for (i=0; i<8; i++) 
			{
				read_buffer[56+i]=(char)(length>>(56-(i*8))) & 0xff;
			}
#else	
			memcpy(read_buffer+56, &length, 8);
#endif
		}
		
		sha_hash((int *)read_buffer, hashval);
		str+=64;
	}

	strprintsha((char *)final, hashval);
	free(hashval);
	return (char *)final;
}
