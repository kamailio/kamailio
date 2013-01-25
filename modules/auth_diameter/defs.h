#ifndef DIAMETER_DEFS 
#define  DIAMETER_DEFS

#define vendorID	0

#define AA_REQUEST 265
#define AA_ANSWER  265

#define SERVICE_LEN  1
#define SIP_AUTHENTICATION	"6"
#define SIP_GROUP_CHECK		"8"

#define SIP_AUTH_SERVICE 	'6'
#define SIP_GROUP_SERVICE	'8'
#define SIP_ACC_SERVICE		'9'

#define AAA_CHALENGE 		 1
#define AAA_AUTHORIZED 		 0
#define AAA_NOT_AUTHORIZED	 2
#define AAA_SRVERR			 3

#define AAA_ERROR			-1
#define AAA_CONN_CLOSED		-2
#define AAA_TIMEOUT			-3
#define AAA_USER_IN_GROUP	 0	

#define AAA_NO_CONNECTION	-1

#define WWW_AUTH_CHALLENGE_LEN 		18
#define PROXY_AUTH_CHALLENGE_LEN 	20
		
#define WWW_AUTH_CHALLENGE		"WWW-Authenticate: "
#define PROXY_AUTH_CHALLENGE 	"Proxy-Authenticate: "

#define MESSAGE_401 "Unauthorized"
#define MESSAGE_407 "Proxy Authentication Required"
#define MESSAGE_400 "Bad Request"
#define MESSAGE_500 "Server Internal Error"

#define separator ","

/* information needed for reading messages from tcp connection */
typedef struct rd_buf
{
	/* used to return a parsed response */
	int ret_code;
	unsigned int chall_len; 
	unsigned char *chall;

	/* used to read the message*/
	unsigned int first_4bytes;
	unsigned int buf_len;
	unsigned char *buf;
} rd_buf_t;

#endif
