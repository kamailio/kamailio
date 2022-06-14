# TODO: tls_wolfssl module

* update `docs/`
* use native wolfSSL API instead of OpenSSL compatibility layer
* remove OpenSSL multi-process hacks:
    * clean up per proc `SSL_CTX`
	* clean up pthread hacks
