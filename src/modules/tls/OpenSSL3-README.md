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


