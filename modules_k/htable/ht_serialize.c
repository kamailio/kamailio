#include "ht_var.h"
#include "../../str.h"
#include "ht_serialize.h"
#include "../../basex.h"

/* snprintf - pretty ugly, but cds/serialize is unusable for the moment */
int serialize_ht_pair(pv_value_t* val, str* htname, str* s) {
	str encoded_val = {0, 0};
	str encoded_htname = {0, 0};
	int len;
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
		len = base16_enc((unsigned char*)val->rs.s, val->rs.len, (unsigned char*)encoded_val.s, encoded_val.len);
		if(len < 0) {
			LM_ERR("cannot encode value\n");
			goto error;
		}
		encoded_val.len = len;
	}
	encoded_htname.len = base64_enc_len(htname->len);
	encoded_htname.s = pkg_malloc(encoded_htname.len);
	len = base16_enc((unsigned char*)htname->s, htname->len, (unsigned char*)encoded_htname.s, encoded_htname.len);
	if(len < 0) {
		LM_ERR("cannot encode htname\n");
		goto error;
	}
	encoded_htname.len = len;
	s->len = snprintf(s->s, s->len, "%d %d %.*s %.*s", val->flags, val->ri, STR_FMT(&encoded_htname), STR_FMT(&encoded_val));
	if(s->len < 0) {
		LM_ERR("cannot serialize data - probably an small buffer\n");
		goto error;
	}
	
	if(encoded_val.s) {
		pkg_free(encoded_val.s);
	}
	if(encoded_htname.s) {
		pkg_free(encoded_htname.s);
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

int deserialize_ht_pair(pv_value_t* val, str* htname, str* src) {
	str encoded_htname = {0, 0};
	str encoded_val = {0, 0};
	encoded_htname.s = pkg_malloc(src->len);
	memset(encoded_htname.s, 0, src->len);
	encoded_val.s = pkg_malloc(src->len);
	memset(encoded_val.s, 0, src->len);
	
	sscanf(src->s, "%d %d %s %s", &val->flags, &val->ri, encoded_htname.s, encoded_val.s);
	encoded_htname.len = strlen(encoded_htname.s);
	encoded_val.len = strlen(encoded_val.s);
	
	if(base64_dec((unsigned char*)encoded_htname.s, encoded_htname.len, (unsigned char*)htname->s, htname->len) < 0) {
		LM_ERR("cannot decode htname\n");
		goto error;
	}
	if(base64_dec((unsigned char*)encoded_val.s, encoded_val.len, (unsigned char*)val->rs.s, val->rs.len) < 0) {
		LM_ERR("cannot decode val\n");
		goto error;
	}
	
	pkg_free(encoded_htname.s);
	pkg_free(encoded_val.s);
	return 0;
error:
	pkg_free(encoded_htname.s);
	pkg_free(encoded_val.s);
	return -1;
}