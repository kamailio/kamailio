#ifndef DNSSEC_FUNC_H
#define DNSSEC_FUNC_H

struct hostent;

int dnssec_res_init(void);
struct hostent* dnssec_gethostbyname(const char *);
struct hostent* dnssec_gethostbyname2(const char *, int);
int dnssec_res_search(const char*, int, int, unsigned char*, int);


#endif
