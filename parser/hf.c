/* 
 * $Id$ 
 */

#include "hf.h"
#include "parse_via.h"
#include "parse_to.h"
#include "parse_cseq.h"
#include "../dprint.h"
#include "../mem/mem.h"
#include "parse_def.h"
#include "digest/digest.h" /* free_credentials */


/* 
 * Frees a hdr_field structure,
 * WARNING: it frees only parsed (and not name.s, body.s)
 */
void clean_hdr_field(struct hdr_field* hf)
{
	if (hf->parsed){
		switch(hf->type){
		case HDR_VIA:
			free_via_list(hf->parsed);
			break;

		case HDR_TO:
			free_to(hf->parsed);
			break;

		case HDR_CSEQ:
			free_cseq(hf->parsed);
			break;

                case HDR_AUTHORIZATION:
		case HDR_PROXYAUTH:
			free_credentials((auth_body_t**)(&(hf->parsed));
			break;

		case HDR_FROM:
			free_to(hf->parsed);
			break;

		default:
			LOG(L_CRIT, "BUG: clean_hdr_field: unknown header type %d\n",
			    hf->type);
			break;
		}
	}
}


/* 
 * Frees a hdr_field list,
 * WARNING: frees only ->parsed and ->next*/
void free_hdr_field_lst(struct hdr_field* hf)
{
	struct hdr_field* foo;
	
	while(hf) {
		foo=hf;
		hf=hf->next;
		clean_hdr_field(foo);
		pkg_free(foo);
	}
}
