#ifndef __orasel_h__
#define __orasel_h__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <oci.h>

typedef struct {
    unsigned  len;
    char      s[];
}Str;

typedef struct {
    const Str*  username;
    const Str*  password;
    const Str*  uri;
    OCIError*   errhp;
    OCISvcCtx*  svchp;
    OCIEnv*     envhp;
    OCISession* authp;
    OCIServer*  srvhp;
    OCIStmt*    stmthp;
}con_t;

typedef struct {
    Str**          names;
    Str***         rows;
    unsigned char* types;
    unsigned       col_n;
    unsigned       row_n;
}res_t;

void __attribute__((noreturn)) donegood(const char *msg);
void __attribute__((noreturn)) errxit(const char *msg);
void __attribute__((noreturn)) oraxit(sword status, const con_t* con);
void* safe_malloc(size_t sz);
Str* str_alloc(const char *s, size_t len);

void open_sess(con_t* con);
void send_req(con_t* con, const Str* req);
void get_res(const con_t* con, res_t* _r);
void out_res(const res_t* _r);

typedef struct {
    unsigned raw : 1,
	     hdr : 1,
	     emp : 1;
}outmode_t;
extern outmode_t outmode;

#endif
