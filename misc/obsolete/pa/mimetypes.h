#ifndef __MIMETYPES_H
#define __MIMETYPES_H

#include "../../parser/parse_content.h"

typedef struct {
	int event_type;
	int mimes[MAX_MIMES_NR];
} event_mimetypes_t;

int get_preferred_event_mimetype(struct sip_msg *_m, int et);
event_mimetypes_t *find_event_mimetypes(int et);
int check_mime_types(int *accepts_mimes, event_mimetypes_t *em);
	
#endif
