#ifndef SCA_CALL_INFO_H
#define SCA_CALL_INFO_H

#include "sca.h"
#include "sca_subscribe.h"

/* pass to sca_notify_subscriber to include all appearances in Call-Info hdr */
#define SCA_CALL_INFO_APPEARANCE_INDEX_ALL	0

struct _sca_call_info {
    int		index;
    int		state;
    str		uri;
};
typedef struct _sca_call_info		sca_call_info;

extern const str	SCA_CALL_INFO_HEADER_STR;

int sca_call_info_build_header( sca_mod *, sca_subscription *, char *, int );
int sca_call_info_append_header_for_appearance_index( sca_subscription *, int,
						      char *, int );

int sca_call_info_header_parse( str *, sca_call_info * );
int sca_call_info_free( sca_call_info * );

#endif /* SCA_CALL_INFO_H */
