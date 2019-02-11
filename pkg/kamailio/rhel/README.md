Now supported dists:

1. RHEL 6
1. RHEL 7

SPEC file and other stuff located at `../obs` folder

IF you wnat build optional packages please use rpmbuild with options "--with". Example

```
rpmbbuild -bb --with cnxcc --with geoip --with http_async_client --with jansson \
    --with json --with kazoo --with memcached --with sctp --with websocket kamailio.spec
```

or

```
mock --rpmbuild-opts '--with cnxcc --with geoip --with http_async_client' --rebuild kamailio.src.rpm
```
