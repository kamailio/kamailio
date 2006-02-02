#include <cds/hash_functions.h>
#include <cds/GeneralHashFunctions.h>
#include <cds/md5.h>

/* hashval is 5 integers ! */
void shahash(const char *str, int strsz, int *hashval);

unsigned int hash_md5(const char *s, unsigned int len)
{
	MD5_CTX context;
	unsigned char digest[16];
	int i = 0;
	unsigned int x = 0;
	unsigned int result = 0;

	MD5Init(&context);
	MD5Update(&context, s, len);
	MD5Final(digest, &context);

	for (i = 0; i < sizeof(digest); i++) {
		result = result ^ (digest[i] << x);
		if (x == 0) x = 8;
		else x = x << 8;
	}
	
	return result;
}

unsigned int hash_sha(const char *s, unsigned int len)
{
	unsigned int u;
	int hashval[5];

	shahash(s, len, hashval);

	u = hashval[0] ^ hashval[1] ^ hashval[2] ^ hashval[3];
	return u;
}

unsigned int hash_domain(const char* s, unsigned int len)
{
	#define h_inc h+=v^(v>>3);
		
	const char* p;
	register unsigned v;
	register unsigned h = 0;/* _h from parameter? */

	for(p=s; p<=(s+len-4); p+=4)
	{
		v=(*p<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
		h_inc;
	}
	
	v=0;
	for(;p<(s+len); p++)
	{
		v<<=8;
		v+=*p;
	}
	h_inc;

	return h;
}

