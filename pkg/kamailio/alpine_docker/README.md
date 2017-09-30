About
-----

Container designed to run on host, bridge and swarm network.
Size of container decreased to 50MB (23MB compressed)
Significantly increased security - removed all libs except libc, busybox, tcpdump, dumpcap, kamailio and dependent libs.
Docker container is created useing Alpine linux packaging

Usage container
---------------

```sh
docker run --net=host --name kamailio \
           -v /etc/kamailio/:/etc/kamailio \
           kamailio/kamailio -m 64 -M 8
```

systemd unit file
-----------------

You can use this systemd unit files on your docker host.
Unit file can be placed to ```/etc/systemd/system/kamailio-docker.service``` and enabled by commands
```sh
systemd start kamailio-docker.service
systemd enable kamailio-docker.service
```

host network
============

```sh
$ cat /etc/systemd/system/kamailio-docker.service
[Unit]
Description=kamailio Container
After=docker.service network-online.target
Requires=docker.service


[Service]
Restart=always
TimeoutStartSec=0
#One ExecStart/ExecStop line to prevent hitting bugs in certain systemd versions
ExecStart=/bin/sh -c 'docker rm -f kamailio; \
          docker run -t --net=host --name kamailio \
                 -v /etc/kamailio/:/etc/kamailio \
                 kamailio/kamailio'
ExecStop=-/bin/sh -c '/usr/bin/docker stop kamailio; \
          /usr/bin/docker rm -f kamailio;'

[Install]
WantedBy=multi-user.target
```

default bridge network
======================
```sh
[Unit]
Description=kamailio Container
After=docker.service network-online.target
Requires=docker.service


[Service]
Restart=always
TimeoutStartSec=0
#One ExecStart/ExecStop line to prevent hitting bugs in certain systemd versions
ExecStart=/bin/sh -c 'docker rm -f kamailio; \
          docker run -t --network bridge --name kamailio \
                 -p 5060:5060/udp -p 5060:5060 \
                 -v /etc/kamailio/:/etc/kamailio \
                 kamailio/kamailio'

ExecStop=-/bin/sh -c '/usr/bin/docker stop kamailio; \
          /usr/bin/docker rm -f kamailio;'

[Install]
WantedBy=multi-user.target
```

.bashrc file
------------
To simplify kamailio managment you can add alias for ```kamctl``` to ```.bashrc``` file as example bellow.
```sh
alias kamctl='docker exec -i -t kamailio /usr/sbin/kamctl'
```
