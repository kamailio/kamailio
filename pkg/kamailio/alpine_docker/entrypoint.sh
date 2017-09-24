#!/bin/sh
#
#  Created by Sergey Safarov <s.safarov@gmail.com>
#

SHM_MEMORY=${SHM_MEMORY:-64}
PKG_MEMORY=${PKG_MEMORY:-8}
trap 'kill -SIGTERM "$pid"' SIGTERM

/usr/sbin/kamailio -DD -E -m $SHM_MEMORY -M $PKG_MEMORY &
pid="$!"

wait $pid
exit 0
