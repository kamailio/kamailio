/*
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
 * $Id$
 */

#include "parse_from.h"
#include "parse_to.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "msg_parser.h"
#include "../ut.h"
#include "../mem/mem.h"

/*
 * This method is used to parse the from header. It was decided not to parse
 * anything in core that is not *needed* so this method gets called by 
 * rad_acc module and any other modules that needs the FROM header.
 *
 * params: hdr : Hook to the from header
 * returns 0 on success,
 *		   -1 on failure.
 */
int parse_from_header(struct hdr_field* hdr) 
{
	struct to_body* from_b;
	
	from_b = pkg_malloc(sizeof(struct to_body));
	if (from_b == 0) {
		LOG(L_ERR, "parse_from_header: out of memory\n");
		goto error;
	}
			
	memset(from_b, 0, sizeof(struct to_body));
	parse_to(hdr->body.s, hdr->body.s + hdr->body.len + 1, from_b);
	if (from_b->error == PARSE_ERROR) {
		LOG(L_ERR, "ERROR: parse_from_header: bad from header\n");
		pkg_free(from_b);
		goto error;
	}
	hdr->parsed = from_b;	
	DBG("DEBUG: parse_from_header: <%s> [%d]; uri=[%.*s] \n",
		hdr->name.s, hdr->body.len, from_b->uri.len, from_b->uri.s);
	DBG("DEBUG: from body [%.*s]\n",from_b->body.len, from_b->body.s);	

	return 0;

	error:
	/* more debugging, msg->orig is/should be null terminated*/
	LOG(L_ERR, "ERROR: parse_from_header: \n");
	return -1;

}


