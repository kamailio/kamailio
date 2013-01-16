#!/bin/bash

INSTPREFIX=/opt/kamailio
FLAVOUR=FLAVOUR=kamailio
VERBOSE=Q=verbose

rm -f /opt/kamailio/lib/kamailio/modules/app_java.so

make clean && rm -f makecfg.lst *.d && make ${FLAVOUR} ${VERBOSE} && make ${FLAVOUR} install prefix=${INSTPREFIX} ${VERBOSE}
