#! /bin/bash

USER=root


/usr/bin/mysql -u $USER < mysql-drop.sql
/usr/bin/mysql -u $USER < mysql-create.sql
