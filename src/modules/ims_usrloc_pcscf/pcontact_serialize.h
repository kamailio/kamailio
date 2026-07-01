#ifndef PCONTACT_SERIALIZE_H
#define PCONTACT_SERIALIZE_H
#include "../../core/str.h"

int pcscf_serialize_impus(str *impus, int n, str *out, int out_size);
int pcscf_serialize_impus_barred(str *barred, int n, str *out, int out_size);
int pcscf_parse_impus(str *in, str *parsed, int max);
int pcscf_apply_barred_flags(
		str *impus, int n, str *barred, int n_barred, char *flags);

#endif
