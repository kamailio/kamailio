# OpenSSL 3 Developer Notes

## Background

OpenSSL since 1.1.1 uses thread-local storage. The OpenSSL internal API
is `CRYPTO_THREAD_set_local()` and it is implemented on Linux using `pthread_setspecific()`.

In a new thread, the value of a thread-local variable is 0x0; the first access of this
variable by OpenSSL will allocate new dynamic memory. The implication for Kamailio is that
if such variables are initialized in rank 0, then all workers will reuse the same memory
location as Kamailio uses shared memory for OpenSSL.

## OpenSSL 1.1.1 shmmem Corruption

In OpenSSL 1.1.1 there are three variables that impact Kamailio: `private_drbg`, `public_drbg`, and
`err_thread_local`. The first two variables are circumvented by an early call to `RAND_set_rand_method()`.

The third variable, i.e., `err_thread_local` is reused by all workers and leads to shmmem corruption
particularly with other users of OpenSSL such as libcurl, and db modules with TLS.

Historically, since 2019, this was a low-impact bug due to use of static variables in the OpenSSL 1.1.1
implementation.


## OpenSSL 3 shmmem Corruption

In OpenSSL 3 there is one variable that impacts Kamailio: `err_thread_local`.

OpenSSL 3 uses more dynamic memory to handle the error stack and shmmem corruption is easily
reproducible, even without libcurl or db modules.

## Resolution

This resolution uses non-portable internal knowledge of pthreads on Linux: that `pthread_key_t`
is a small integer, and that it is incremented when a new thread-local key is requested

OpenSSL 3 uses 6 thread-locals, and OpenSSL 1.1.1 uses 4 thread-locals.

The first attempt (5.8.0/5.8.1) to resolve this issue uses the following technique:
* `tls_threads_mode = 1`: for each function that might initialize OpenSSL, run it in
  a temporary thread; this leaves the thread-local variables in rank 0, main thread at their
  default value of 0x0
* `tls_threads_mode = 2`: add an at-fork handler to set thread-local variables to 0x0.
   The implementation will set thread-local keys from 0-15 to have value 0x0.<br>

Limitation: the limitation of this method is some libraries like libpython cannot be initialized
other than in the primary thread and they will initialize thread-locals.

The revised method makes a few OpenSSL function calls so
that OpenSSL will initialize all required thread-locals, and the tls.so sets a high-water mark.
It is assumed that all `pthread_key_t` values at the high-water mark or greater are set
by non-OpenSSL libraries. During fork, tls.so will clear all thread-locals up to the high-water
mark.

## Update
@meengu(github) has an alternate solution from this [issue](https://github.com/OpenSIPS/opensips/issues/3388)
The diff is included here for future reference. It may prove useful if the current
solution fails in later versions of OpenSSL.

    From 84b4df66853506ce8d4853ec0fbcb25545a67a54 Mon Sep 17 00:00:00 2001
    From: Ondrej Jirman <megi@xff.cz>
    Date: Mon, 13 May 2024 17:34:52 +0200
    Subject: [PATCH] Fix openssl TLS data corruption in shared memory by workers

    The problem is that somet TLS state is shared among workers but should
    not be. We solve this by clearing the relevant TLS data after fork in the
    child process.

    We identify the data to clear by asking OPENSSL itself for the pointers,
    and then searching through the first 32 TLS items.

    Signed-off-by: Ondrej Jirman <megi@xff.cz>
    ---
     modules/tls_openssl/openssl.c | 47 +++++++++++++++++++++++++++++++++++
     1 file changed, 47 insertions(+)

    diff --git a/modules/tls_openssl/openssl.c b/modules/tls_openssl/openssl.c
    index 522b68258527..067865eef20f 100644
    --- a/modules/tls_openssl/openssl.c
    +++ b/modules/tls_openssl/openssl.c
    @@ -29,6 +29,9 @@
     #include <openssl/opensslv.h>
     #include <openssl/err.h>
     #include <openssl/rand.h>
    +#if OPENSSL_VERSION_NUMBER < 0x30000000L
    +#include <openssl/rand_drbg.h>
    +#endif

     #include "../../dprint.h"
     #include "../../mem/shm_mem.h"
    @@ -188,6 +191,48 @@ static int check_for_krb(void)
     }
     #endif

    +static void clean_openssl_locals(void)
    +{
    +#if OPENSSL_VERSION_NUMBER < 0x30000000L
    +	ERR_STATE *es = ERR_get_state();
    +	RAND_DRBG *r0 = RAND_DRBG_get0_public();
    +	RAND_DRBG *r1 = RAND_DRBG_get0_private();
    +
    +	for(int k = 0; k < 32; k++) {
    +		void* p = pthread_getspecific(k);
    +		if (p && p == es) {
    +			pthread_setspecific(k, NULL);
    +			ERR_clear_error();
    +		} else if (p && p == r0) {
    +			pthread_setspecific(k, NULL);
    +			RAND_DRBG_get0_public();
    +		} else if (p && p == r1) {
    +			pthread_setspecific(k, NULL);
    +			RAND_DRBG_get0_private();
    +		}
    +	}
    +#else
    +	OSSL_LIB_CTX *ctx = OSSL_LIB_CTX_get0_global_default();
    +	ERR_STATE *es = ERR_get_state();
    +	EVP_RAND_CTX *r0 = RAND_get0_public(ctx);
    +	EVP_RAND_CTX *r1 = RAND_get0_private(ctx);
    +
    +	for(int k = 0; k < 32; k++) {
    +		void* p = pthread_getspecific(k);
    +		if (p && p == es) {
    +			pthread_setspecific(k, NULL);
    +			ERR_clear_error();
    +		} else if (p && p == r0) {
    +			pthread_setspecific(k, NULL);
    +			RAND_get0_public(ctx);
    +		} else if (p && p == r1) {
    +			pthread_setspecific(k, NULL);
    +			RAND_get0_private(ctx);
    +		}
    +	}
    +#endif
    +}
    +
     /*
      * initialize ssl methods
      */
    @@ -297,6 +342,8 @@ static int mod_init(void)
        on_exit(openssl_on_exit, NULL);
     #endif

    +	pthread_atfork(NULL, NULL, clean_openssl_locals);
    +
        return 0;
     }

    --
    2.45.0
