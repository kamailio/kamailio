#ifndef SCA_UTIL_H
#define SCA_UTIL_H

#include "sca_common.h"

/* get method, regardless of whether message is a request or response */
int	sca_get_msg_method( sip_msg_t * );

/* populate a str pointer with contact uri from a SIP request or response */
int	sca_get_msg_contact_uri( sip_msg_t *, str * );

/* convenient extraction of cseq number from Cseq header */
int	sca_get_msg_cseq_number( sip_msg_t * );

/* convenient extraction of cseq method from Cseq header */
int	sca_get_msg_cseq_method( sip_msg_t * );

/* convenient From header parsing and extraction */
int	sca_get_msg_from_header( sip_msg_t *, struct to_body ** );

/* convenient To header parsing and extraction */
int	sca_get_msg_to_header( sip_msg_t *, struct to_body ** );

/* convenient AoR extraction from sip: URIs */
int	sca_uri_extract_aor( str *, str * );

/* convenient AoR creation from a Contact URI and another AoR */
int	sca_uri_build_aor( str *, int, str *, str * );

/* convenient call hold detection */
int	sca_call_is_held( sip_msg_t * );

#endif /* SCA_UTIL_H */
