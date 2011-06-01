#include "ht_var.h"
#include "../../str.h"
#include "ht_serialize.h"
#include "../../basex.h"

int serialize_ht_pair(pv_value_t* val, str* htname, str* s, int len) {
	str encoded_val = {0, 0};
	str encoded_htname = {0, 0};
	if (!s) {
		LM_ERR("no destination string given\n");
		goto error;
	}
	if(!htname || !htname->s || !htname->len) {
		LM_ERR("no hashtable name given\n");
		goto error;
	}
	if(val->rs.len) {
		encoded_val.len = base64_enc_len(val->rs.len);
		encoded_val.s = pkg_malloc(encoded_val.len);
		if(base64_enc((unsigned char*)val->rs.s, val->rs.len, (unsigned char*)encoded_val.s, encoded_val.len) < 0) {
			LM_ERR("cannot base64 value\n");
			goto error;
		}
	}
	encoded_htname.len = base64_enc_len(htname->len);
	encoded_htname.s = pkg_malloc(encoded_htname.len);
	if(base64_enc((unsigned char*)htname->s, htname->len, (unsigned char*)encoded_htname.s, encoded_htname.len) < 0) {
		LM_ERR("cannot base64 value\n");
		goto error;
	}
	s->len = snprintf(s->s, len, "%d %d %.*s %.*s", val->flags, val->ri, STR_FMT(&encoded_htname), STR_FMT(&encoded_val));
	if(s->len < 0) {
		LM_ERR("cannot serialize data - probably an small buffer\n");
		goto error;
	}
	return 0;
error:
	if(encoded_val.s) {
		pkg_free(encoded_val.s);
	}
	if(encoded_htname.s) {
		pkg_free(encoded_htname.s);
	}
	return -1;
}