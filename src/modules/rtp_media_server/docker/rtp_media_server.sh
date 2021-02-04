#!/bin/sh
DIR_PREFIX=`pwd`
CONTAINER=rtp_media_server
IMAGE=rtp_media_server:latest
docker stop ${CONTAINER}
docker rm ${CONTAINER}
docker run -d --net=host --privileged --name=${CONTAINER} ${IMAGE} /bin/sh -c "tail -f /dev/null"

echo ""
echo "manual steps required to start Kamailio in the container:"
echo " cp git/kamailio/src/modules/rtp_media_server/config_example/kamailio.cfg /etc"
echo " set the listen IP in  /etc/kamailio.cfg"
echo " docker exec -it rtp_media_server bash"
echo " kamailio -m 64 -D -dd -f /etc/kamailio.cfg"
echo ""

# ulimit -c unlimited
# echo "/tmp/core.%e.sig%s.%p" > /proc/sys/kernel/core_pattern
# sysctl -w kernel.core_pattern=core
# apt-get update && apt-get install -i gdb
