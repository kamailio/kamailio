#ifndef _PEERSTATE_H
#define _PEERSTATE_H

#include "../../core/parser/parse_uri.h"
struct str_list;

#define PEERSTATE_EVENT_ROUTE "dialog:state_changed"

typedef enum pstate
{
	NOT_INUSE,	 // according to dialog state
	INUSE,		 // according to dialog and register state
	RINGING,	 // according to dialog state
	UNAVAILABLE, // according to register state
	NA
} pstate_t;

static const char *pstate_strs[] __attribute__((unused)) = {
		"NOT_INUSE", "INUSE", "RINGING", "UNAVAILABLE", "N/A"};

#define PSTATE_TO_STR(s)                                                   \
	((s) >= 0 && (s) < (int)(sizeof(pstate_strs) / sizeof(pstate_strs[0])) \
					? pstate_strs[s]                                       \
					: "UNKNOWN")

struct dlginfo_cell
{
	struct sip_uri *from;
	struct sip_uri *to;
	str callid;
	struct str_list *caller_peers;
	struct str_list *callee_peers;
	int disable_caller_notify;
	int disable_callee_notify;
};

#endif /* _PEERSTATE_H */