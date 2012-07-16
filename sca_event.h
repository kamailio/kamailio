enum {
    SCA_EVENT_TYPE_UNKNOWN = -1,
    SCA_EVENT_TYPE_CALL_INFO = 1,
    SCA_EVENT_TYPE_LINE_SEIZE = 2,
};

extern str	SCA_EVENT_NAME_CALL_INFO;
extern str	SCA_EVENT_NAME_LINE_SEIZE;

#define sca_ok_status_for_event(e1) \
	(e1) == SCA_EVENT_TYPE_CALL_INFO ? 202 : 200
#define sca_ok_text_for_event(e1) \
	(e1) == SCA_EVENT_TYPE_CALL_INFO ? "Accepted" : "OK"

int		sca_event_from_str( str * );
char		*sca_event_name_from_type( int );
int		sca_event_append_header_for_type( int, char *, int );
