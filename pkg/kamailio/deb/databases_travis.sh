#!/bin/bash
# create MySQL and PostgreSQL kamailio databases

cd utils/kamctl
PWSKIP=yes CHARSET=latin1 DBENGINE=MYSQL DBNAME=kamailio INSTALL_EXTRA_TABLES=yes \
  INSTALL_PRESENCE_TABLES=yes INSTALL_DBUID_TABLES=yes \
  ./kamdbctl create
touch ~/.pgpass; chmod 600 ~/.pgpass
PWSKIP=yes DBENGINE=PGSQL DBNAME=kamailio INSTALL_EXTRA_TABLES=yes \
  INSTALL_PRESENCE_TABLES=yes INSTALL_DBUID_TABLES=yes \
  ./kamdbctl create
