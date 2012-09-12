#ifndef SCA_UPDATE_H
#define SCA_UPDATE_H

/* RURI, From URI, To URI, Contact URI, Call-ID, From-tag, To-tag, (body?) */
int		sca_update_endpoint( sca_mod *, str *, str *, str *, str *,
					str *, str *, str * );

int		sca_update_endpoints( sip_msg_t *, char *, char * );

#endif /* SCA_UPDATE_H */
