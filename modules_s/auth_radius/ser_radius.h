/* Service types */
#define PW_CALL_CHECK                   10
#define PW_GROUP_CHECK                  12
#define PW_EMERGENCY_CALL               13

/* Attributes*/
#define PW_DIGEST_RESPONSE	        206	/* string */
#define PW_DIGEST_ATTRIBUTES	        207	/* string */

#define PW_SIP_URI_USER                 208     /* string */
#define PW_SIP_METHOD                   209     /* int */
#define PW_SIP_REQ_URI                  210     /* string */
#define PW_SIP_GROUP                    211     /* string */
#define PW_SIP_CC                       212     /* string */

#define PW_DIGEST_REALM		        1063	/* string */
#define	PW_DIGEST_NONCE		        1064	/* string */
#define	PW_DIGEST_METHOD	        1065	/* string */
#define	PW_DIGEST_URI		        1066	/* string */
#define	PW_DIGEST_QOP		        1067	/* string */
#define	PW_DIGEST_ALGORITHM	        1068	/* string */
#define	PW_DIGEST_BODY_DIGEST	        1069	/* string */
#define	PW_DIGEST_CNONCE	        1070	/* string */
#define	PW_DIGEST_NONCE_COUNT	        1071	/* string */
#define	PW_DIGEST_USER_NAME	        1072	/* string */
