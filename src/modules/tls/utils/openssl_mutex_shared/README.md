# OpenSSL Shared Mutex #

**IMPORTANT: the workaround of using this preloaded shared library is no longer
needed starting with Kamailio v5.3.0-pre1 (git master branch after September 14, 2019).
The code of this shared library has been included in the core of Kamailio and the
same behaviour is now achieved by default.**

This is a shared library required as a short term workaround for using Kamailio
with OpenSSL (libssl) v1.1. It has to be pre-loaded before starting Kamailio.

In v1.1, libssl does not allow setting custom locking functions, using internally
pthread mutexes and rwlocks, but they are not initialized with process shared
option (PTHREAD_PROCESS_SHARED), which can result in blocking Kamailio worker
processes.

## Installation ##

By default, it is installed when the tls module is installed.

It can be installed manually, in this folder execute:

```
make
make install
```

It is installed at the same place where Kamailio deploys the directory with
modules.

For example, when installing from sources on a 64b system, the location is:

```
/usr/local/lib64/kamailio/openssl_mutex_shared/openssl_mutex_shared.so
```

For Debian packing, the location is like:

```
/usr/lib/x86_64-linux-gnu/kamailio/openssl_mutex_shared/openssl_mutex_shared.so
```

Note: there is no dependency on Kamailio source code, this shared object can
be compiled and used ouside of Kamailio source tree. It uses only Kamailio's
Makefile system to install in the same directory like the other shared objects
installed by Kamailio.

## Usage ##

Use LD_PRELOAD to tell the linker to preload this shared object before starting
Kamailio.

Example, when Kamailio was installed from sources:

```
LD_PRELOAD=/usr/local/lib64/kamailio/openssl_mutex_shared/openssl_mutex_shared.so \
  /usr/local/sbin/kamailio -f /usr/local/etc/kamailio/kamailio.cfg
```

If using systemd, add to service file:

```
Environment='LD_PRELOAD=/usr/local/lib64/kamailio/openssl_mutex_shared/openssl_mutex_shared.so'
```
