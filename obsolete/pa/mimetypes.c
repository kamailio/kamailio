#include "mimetypes.h"
#include "../../dprint.h"
#include "../../parser/parse_event.h"

static event_mimetypes_t event_package_mimetypes[] = {
	{ EVENT_PRESENCE, {
			MIMETYPE(APPLICATION,PIDFXML), 
/*		    MIMETYPE(APPLICATION,XML_MSRTC_PIDF), */
/*		    MIMETYPE(TEXT,XML_MSRTC_PIDF), */
			MIMETYPE(APPLICATION,CPIM_PIDFXML), 
		    MIMETYPE(APPLICATION,XPIDFXML), 
			MIMETYPE(APPLICATION,LPIDFXML), 
			0 } },
	{ EVENT_PRESENCE_WINFO, { 
			MIMETYPE(APPLICATION,WATCHERINFOXML), 
			0 } },
/*	{ EVENT_SIP_PROFILE, { 
			MIMETYPE(MESSAGE,EXTERNAL_BODY), 
			0 } }, */
/*	{ EVENT_XCAP_CHANGE, { MIMETYPE(APPLICATION,WINFO+XML), 0 } }, */
	{ -1, { 0 }},
};

/* returns -1 if m is NOT found in mimes
 * index to the found element otherwise (non-negative value) */
static int find_mime(int *mimes, int m)
{
	int i;
	for (i = 0; mimes[i]; i++) {
		if (mimes[i] == m) return i;
	}
	return -1;
}

event_mimetypes_t *find_event_mimetypes(int et)
{
	int i;
	event_mimetypes_t *em;
		
	i = 0;
	while (et != event_package_mimetypes[i].event_type) {
		if (event_package_mimetypes[i].event_type == -1) break;
		i++;
	}
	em = &event_package_mimetypes[i]; /* if not found is it the "separator" (-1, 0)*/
	return em;
}

int check_mime_types(int *accepts_mimes, event_mimetypes_t *em)
{
	/* LOG(L_ERR, "check_message -2- accepts_mimes=%p\n", accepts_mimes); */
	if (accepts_mimes) {
		int j = 0, k;
		int mimetype;
		while ((mimetype = em->mimes[j]) != 0) {
			k = find_mime(accepts_mimes, mimetype);
			if (k >= 0) {
				int am0 = accepts_mimes[0];
				/* we have a match */
				/*LOG(L_ERR, "check_message -4b- eventtype=%#x accepts_mime=%#x\n", eventtype, mimetype); */
				/* move it to front for later */
				accepts_mimes[0] = mimetype;
				accepts_mimes[k] = am0;
				return 0; /* ! this may be useful, but it modifies the parsed content !!! */
			}
			j++;
		}
		
		return -1;
	}
	return 0;
}

/* returns index of mimetype, the lowest index = highest priority */
static int get_accepted_mime_type_idx(int *accepts_mimes, event_mimetypes_t *em)
{
	int i, mt;
	if (accepts_mimes) {
		/* try find "preferred" mime type */
		i = 0;
		while ((mt = em->mimes[i]) != 0) {
			/* TRACE_LOG("searching for %x\n", mt); */
			if (find_mime(accepts_mimes, mt) >= 0) return i;
			i++;
		}
	}
	return -1;
}

int get_preferred_event_mimetype(struct sip_msg *_m, int et)
{
	int idx, tmp, acc = 0;
	int *accepts_mimes;
	struct hdr_field *accept;

	event_mimetypes_t *em = find_event_mimetypes(et);
	if (!em) return 0; /* never happens, but ... */

	accept = _m->accept;
	idx = -1;
	while (accept) { /* go through all Accept headers */
		if (accept->type == HDR_ACCEPT_T) {
			/* it MUST be parsed from parse_hdr !!! */
			accepts_mimes = (int *)accept->parsed;
			tmp = get_accepted_mime_type_idx(accepts_mimes, em);
			if (idx == -1) idx = tmp;
			else
				if ((tmp != -1) && (tmp < idx)) idx = tmp;
			/* TRACE_LOG("%s: found mimetype %x (idx %d), %p\n", __FUNCTION__, (idx >= 0) ? em->mimes[idx]: -1, idx, accepts_mimes); */
			if (idx == 0) break; /* the lowest value */
		}
		accept = accept->next;
	}
	if (idx != -1) return em->mimes[idx]; /* found value with highest priority */

	acc = em->mimes[0];
	DBG("defaulting to mimetype %x for event_type=%d\n", acc, et);
	return acc;
}

