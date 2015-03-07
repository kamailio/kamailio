/*
 * Tests for basex.h
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


/*#define NO_BASE64_LOOKUP_TABLE
 #define SINGLE_REG */

#include "../basex.h"
#include "profile.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <ctype.h>


#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define BASE64 64
#define Q_BASE64 640
#define BASE16 16

#ifndef BASEX
#define BASEX BASE16
#endif

#if BASEX == Q_BASE64

#warning Q_BASE64
#define B_ENC	q_base64_enc
#define B_DEC	q_base64_dec
#define B_ENC_LEN(l)	(((l)+2)/3*4)

#elif BASEX == BASE16

#warning BASE16
#define B_ENC	base16_enc
#define B_DEC	base16_dec
#define B_ENC_LEN(l)	((l)*2)

#else

#warning BASE64
#define B_ENC	base64_enc
#define B_DEC	base64_dec
#define B_ENC_LEN(l)	(((l)+2)/3*4)


#endif


#define QUOTE_MACRO(x) QUOTEME(x)
#define QUOTEME(x) #x

static char* id="$Id$";
static char* version="basex test 0.1 " 
"BASE" QUOTE_MACRO(BASEX)  ": " QUOTE_MACRO(B_ENC) ", " QUOTE_MACRO(B_DEC) ""
#if defined BASE64_LOOKUP_TABLE 
#ifdef BASE64_LOOKUP_LARGE
" (large b64 lookup table)"
#else
" (lookup b64 table)"
#endif
#else
" (no b64 lookup table)"
#endif
#if defined BASE16_LOOKUP_TABLE
#ifdef BASE16_LOOKUP_LARGE
" (large b16 lookup table)"
#else
" (lookup b16 table)"
#endif
#else
" (no b16 lookup table)"
#endif
#if defined BASE64_READ_WHOLE_INTS || defined BASE16_READ_WHOLE_INTS
" (read 4 bytes at a time)"
#else
" (read 1 byte at a time)"
#endif
;

static char* help_msg="\
Usage: basex  [-hv] ... [options]\n\
Options:\n\
    -m min        minimum length\n\
    -M max        maximum length\n\
    -o offset     offset from the start of the buffer (alignment tests)\n\
    -e offset     offset from the start of the dst. buf. (alignment tests)\n\
    -n no.        number of test loops\n\
    -v            increase verbosity\n\
    -V            version number\n\
    -h            this help message\n\
";


/* profiling */
struct profile_data pf1, pf2, pf3, pf4, pf5, pf6;


void dump_profile_info(struct profile_data* pd)
{
	printf("profiling for %s (%ld/%ld):  %lld/%lld/%lld (max/avg/last),"
			" total %lld\n",
			pd->name, pd->entries, pd->exits, pd->max_cycles, 
			pd->entries?pd->total_cycles/pd->entries:0, pd->cycles,
			pd->total_cycles);
}



int seed_prng()
{
	int seed, rfd;
	
	if ((rfd=open("/dev/urandom", O_RDONLY))!=-1){
try_again:
		if (read(rfd, (void*)&seed, sizeof(seed))==-1){
			if (errno==EINTR) goto try_again; /* interrupted by signal */
				fprintf(stderr, "WARNING: could not read from /dev/urandom: "
								" %s (%d)\n", strerror(errno), errno);
		}
		close(rfd);
	}else{
		fprintf(stderr, "WARNING: could not open /dev/urandom: %s (%d)\n",
						strerror(errno), errno);
	}
	seed+=getpid()+time(0);
	srand(seed);
	return 0;
}


/* fill buf with random data*/
void fill_rand(unsigned char* buf, int len)
{
	unsigned char* end;
	int v;

/* find out how many random bytes we can get from rand() */
#if RAND_MAX >= 0xffffffff
#define RAND_BYTES 4
#warning RAND_BYTES is 4
#elif RAND_MAX >= 0xffffff
#define RAND_BYTES 3
#warning RAND_BYTES is 3
#elif RAND_MAX >= 0xffff
#define RAND_BYTES 2
#warning RAND_BYTES is 2
#else
#define RAND_BYTES 1
#endif

	end=buf+len/RAND_BYTES*RAND_BYTES;
	for(;buf<end;buf+=RAND_BYTES){
		v=rand();
		buf[0]=v;
#if RAND_BYTES > 1
		buf[1]=v>>8;
#endif
#if RAND_BYTES > 2
		buf[2]=v>>16;
#endif
#if RAND_BYTES > 4
		buf[3]=v>>24;
#endif
	}
	v=rand();
	switch(end-buf){
		case 3:
#if RAND_BYTES > 2
			buf[2]=v>>16;
#else
			buf[2]=rand();
#endif
		case 2:
#if RAND_BYTES > 1
			buf[1]=v>>8;
#else
			buf[1]=rand();
#endif
		case 1:
			buf[0]=v;
		case 0:
			break;
	}
}



int main(int argc, char** argv)
{

	int loops, min_len, max_len, offset, e_offset;
	unsigned char* ibuf;
	unsigned char* enc_buf;
	unsigned char* dec_buf;
	int ibuf_len, enc_buf_len, dec_buf_len;
	int offs, c_len, e_len, l;
	int r;
	int verbose;
	int c;
	char* tmp;

	verbose=0;
	min_len=max_len=offset=-1;
	e_offset=0;
	loops=1024;
	opterr=0;
	while ((c=getopt(argc, argv, "n:m:M:o:e:vhV"))!=-1){
		switch(c){
			case 'n':
				loops=strtol(optarg, &tmp, 0);
				if ((tmp==0)||(*tmp)||(loops<0)){
					fprintf(stderr, "bad number: -%c %s\n", c, optarg);
					goto error;
				}
				break;
			case 'm':
				min_len=strtol(optarg, &tmp, 0);
				if ((tmp==0)||(*tmp)||(min_len<0)){
					fprintf(stderr, "bad number: -%c %s\n", c, optarg);
					goto error;
				}
				break;
			case 'M':
				max_len=strtol(optarg, &tmp, 0);
				if ((tmp==0)||(*tmp)||(max_len<0)){
					fprintf(stderr, "bad number: -%c %s\n", c, optarg);
					goto error;
				}
				break;
			case 'o':
				offset=strtol(optarg, &tmp, 0);
				if ((tmp==0)||(*tmp)||(offset<0)){
					fprintf(stderr, "bad number: -%c %s\n", c, optarg);
					goto error;
				}
				break;
			case 'e':
				e_offset=strtol(optarg, &tmp, 0);
				if ((tmp==0)||(*tmp)||(e_offset<0)){
					fprintf(stderr, "bad number: -%c %s\n", c, optarg);
					goto error;
				}
				break;
			case 'v':
				verbose++;
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n", id);
				exit(0);
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c\n", optopt);
				else
					fprintf(stderr, "Unknown character `\\x%x\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option `-%c requires an argument.\n",
						optopt);
				goto error;
				break;
			default:
				abort();
		}
	}
	if (min_len==-1 && max_len==-1){
		min_len=0;
		max_len=4*1024*1024;
	}else if (min_len==-1)
		min_len=0;
	else if (max_len==-1)
		max_len=min_len;
	/* init */
	ibuf_len=max_len;
	ibuf=malloc(ibuf_len);
	if (ibuf==0){
		fprintf(stderr, "ERROR: 1. memory allocation error (%d bytes)\n",
						ibuf_len);
		exit(-1);
	}
	enc_buf_len=B_ENC_LEN(ibuf_len);
	enc_buf=malloc(enc_buf_len+e_offset);
	if (enc_buf==0){
		fprintf(stderr, "ERROR: 2. memory allocation error (%d bytes)\n",
						enc_buf_len);
		exit(-1);
	}
	enc_buf+=e_offset; /* make sure it's off by e_offset bytes from the
						 aligned stuff malloc returns */
	dec_buf_len=ibuf_len;
	dec_buf=malloc(dec_buf_len+e_offset);
	if (dec_buf==0){
		fprintf(stderr, "ERROR: 3. memory allocation error (%d bytes)\n",
						dec_buf_len+e_offset);
		exit(-1);
	}
	dec_buf+=e_offset; /* make sure it's off by e_offset bytes from the
						  aligned stuff malloc returns */
	
	
	seed_prng();
	/* profile */
	profile_init(&pf1, "encode");
	profile_init(&pf2, "decode");
	
	init_basex();
	if (verbose)
		printf("starting (loops %d, min size %d, max size %d, offset %d,"
				", e_offset %d, buffer sizes %d %d %d)\n",
				loops, min_len, max_len, offset, e_offset, ibuf_len,
				enc_buf_len, dec_buf_len);
		
		for (r=0; r<loops; r++){
			if (min_len!=max_len)
				/* test encode/decode random data w/ random length*/
				c_len= min_len+(int)((float)(max_len-min_len+1)*
											(rand()/(RAND_MAX+1.0)));
			else 
				/* test encode /decode random data w/ fixed lenght*/
				c_len=max_len;
			if (offset==-1)
				/* offset between 0 & MIN(clen,3) */
				offs= (int)((float)(MIN(c_len,3)+1)*(rand()/(RAND_MAX+1.0)));
			else if (offset>c_len)
				offs=0;
			else
				offs=offset;
			if (verbose>2)
				printf("loop %d, current len %d, offset %d, start %p\n",
							r, c_len-offs, offs, &ibuf[offs]);
			else if ((verbose >1) && (r %10==0)) putchar('.');
			
			fill_rand(ibuf, c_len);
			
			c_len-=offs;
			e_len=B_ENC_LEN(c_len);
			profile_start(&pf1);
			l=B_ENC(&ibuf[offs], c_len, enc_buf, e_len);
			profile_end(&pf1);
			if (l != e_len){
				fprintf(stderr, "ERROR: invalid length for encoding: %d "
								"instead of %d (loops=%d)\n", l, e_len, r);
				exit(-1);
			}
			profile_start(&pf2);
			l=B_DEC(enc_buf, e_len, dec_buf, c_len);
			profile_end(&pf2);
			if (l != c_len){
				fprintf(stderr, "ERROR: invalid length for decoding: %d "
								"instead of %d (loops=%d)\n", l, c_len, r);
				exit(-1);
			}
			if (memcmp(&ibuf[offs], dec_buf, c_len)!=0){
				fprintf(stderr, "ERROR: decoding mismatch "
								"(loops=%d, c_len=%d)\n", r, c_len);
				abort();
				exit(-1);
			}
		}
	 if (verbose >1) putchar('\n');
	/* encode len data and decode it, print profiling info*/
	 dump_profile_info(&pf1);
	 dump_profile_info(&pf2);
	 return 0;
error:
		 exit(-1);
}
