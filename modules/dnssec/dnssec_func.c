
#include "validator/validator-config.h"
#include <validator/validator.h>
#include <validator/resolver.h>

#include "../../dprint.h"

static struct libval_context  *libval_ctx = NULL;

static inline int
dnssec_init_context(void) {
  if (libval_ctx == NULL) {
      if (val_create_context(NULL, &libval_ctx) != VAL_NO_ERROR)
	return -1;
  }
  return 0;
}



struct hostent *
dnssec_gethostbyname(const char *name) {
  val_status_t          val_status;
  struct hostent *      res;

  if (dnssec_init_context())
    return NULL;

  LOG(L_ERR, " gethostbyname(%s) called: wrapper\n", name);
  
  res = val_gethostbyname(libval_ctx, name, &val_status);

  if (val_istrusted(val_status) && !val_does_not_exist(val_status)) {
   return res;
  }

  return (NULL); 
}


struct hostent *
dnssec_gethostbyname2(const char *name, int family) {
  val_status_t          val_status;
  struct hostent *      res;

  if (dnssec_init_context())
    return NULL;

  LOG(L_ERR, " gethostbyname2(%s) called: wrapper\n", name);
  
  res = val_gethostbyname2(libval_ctx, name, family,  &val_status);

  if (val_istrusted(val_status) && !val_does_not_exist(val_status)) {
      return res;
  }
  return NULL; 
}

int
dnssec_res_init(void) {

  LOG(L_ERR, "res_init called: wrapper\n");

  return dnssec_init_context();
}



int
dnssec_res_search(const char *dname, int class_h, int type_h, 
	  unsigned char *answer, int anslen) {
  val_status_t          val_status;
  int ret;

  if (dnssec_init_context())
    return -1;

  LOG(L_ERR, "res_query(%s,%d,%d) called: wrapper\n",
	  dname, class_h, type_h);

  ret = val_res_search(libval_ctx, dname, class_h, type_h, answer, anslen,
			&val_status);

  if (val_istrusted(val_status) && !val_does_not_exist(val_status)) {
    return ret;
  }

  return -1;
}
