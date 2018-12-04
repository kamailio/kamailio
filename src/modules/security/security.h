#ifndef _SECURITY_H
#define _SECURITY_H

#include "../../core/sr_module.h"


/* Blacklist variables */
char *bl_ua_list[1024];
char *bl_country_list[1024];
char *bl_domain_list[1024];
char *bl_user_list[1024];
char *bl_ip_list[1024];
int *nblUa;
int *nblCountry;
int *nblDomain;
int *nblUser;
int *nblIp;

/* Whitelist variables */
char *wl_ua_list[1024];
char *wl_country_list[1024];
char *wl_domain_list[1024];
char *wl_user_list[1024];
char *wl_ip_list[1024];
int *nwlUa;
int *nwlCountry;
int *nwlDomain;
int *nwlUser;
int *nwlIp;

/* Destination blacklist variables */
char *dst_list[1024];
int *nDst;

/* Shared functions */
int init_db(void);
void uppercase(char *sPtr);
int insert_db(int action, char *type, char *value);

/* Database functions */
int check_version(void);
void load_data_from_db(void);

/* SQLi check functions */
int check_sqli_ua(struct sip_msg *msg);
int check_sqli_to(struct sip_msg *msg);
int check_sqli_from(struct sip_msg *msg);
int check_sqli_contact(struct sip_msg *msg);

/* Extern variables */
extern str db_url;
extern str table_name;
extern int dst_exact_match;

/* RPC commands */
void rpc_reload(rpc_t *rpc, void *ctx);
void rpc_print(rpc_t *rpc, void *ctx);
void rpc_add_bl(rpc_t *rpc, void *ctx);
void rpc_add_wl(rpc_t *rpc, void *ctx);

#endif
