#ifndef _SECURITY_H
#define _SECURITY_H

#include "../../core/sr_module.h"


/* Blacklist variables */
extern char *sec_bl_ua_list[1024];
extern char *sec_bl_country_list[1024];
extern char *sec_bl_domain_list[1024];
extern char *sec_bl_user_list[1024];
extern char *sec_bl_ip_list[1024];
extern int *sec_nblUa;
extern int *sec_nblCountry;
extern int *sec_nblDomain;
extern int *sec_nblUser;
extern int *sec_nblIp;

/* Whitelist variables */
extern char *sec_wl_ua_list[1024];
extern char *sec_wl_country_list[1024];
extern char *sec_wl_domain_list[1024];
extern char *sec_wl_user_list[1024];
extern char *sec_wl_ip_list[1024];
extern int *sec_nwlUa;
extern int *sec_nwlCountry;
extern int *sec_nwlDomain;
extern int *sec_nwlUser;
extern int *sec_nwlIp;

/* Destination blacklist variables */
extern char *sec_dst_list[1024];
extern int *sec_nDst;

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
extern str sec_db_url;
extern str sec_table_name;
extern str sec_action_col;
extern str sec_type_col;
extern str sec_data_col;
extern int sec_dst_exact_match;

/* RPC commands */
void rpc_reload(rpc_t *rpc, void *ctx);
void rpc_print(rpc_t *rpc, void *ctx);
void rpc_add_bl(rpc_t *rpc, void *ctx);
void rpc_add_wl(rpc_t *rpc, void *ctx);

#endif
