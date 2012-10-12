#! /bin/sh

set -x
sudo /sbin/service sip-router stop
sudo cp sca.so /usr/local/lib64/ser/modules_s/sca.so
sudo /sbin/service sip-router start
set +x
