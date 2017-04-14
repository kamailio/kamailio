#ifndef _DBK_PUA_
#define _DBK_PUA_


int kz_initialize_pua();
int kz_pua_publish(struct sip_msg* msg, char *json);
int kz_pua_publish_mwi(struct sip_msg* msg, char *json);
int kz_pua_publish_presence(struct sip_msg* msg, char *json);
int kz_pua_publish_dialoginfo(struct sip_msg* msg, char *json);


#endif
