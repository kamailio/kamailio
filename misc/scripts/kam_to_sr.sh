#!/usr/bin/env sh
#
# This is a simple script which attempts to convert kamailio modules so that
# they can be used with the sip-router core. Most of the changes done by the
# script deal with the changes in the database abstraction layer in the
# sip-router source tree. 
#
# Run this script in module directory to convert it from kamailio core to
# sip-router core. The root of the tree should be two levels up, otherwise
# relative paths to headers (../..) would not work and the module will not
# compile.
# 
# Some of the changes done by the script:
#
#  * Extra defines in the Makefile to make the module link with libsrdb1
#  * Path to database headers updated to point to lib/srdb1
#  * db_con_t and db_res_t renamed to db1_con_t and db1_res_t in *.[ch]
#  * Value type names such as DB_INT changed to DB1_INT in *.[ch]
#
# NOTE: There is no guarantee that the update module would compile or even
#       work. Make a backup before running the script. You have been warned!
# 
# Written by Jan Janak <jan@iptel.org>
#

if [ ! -f Makefile ] ; then
	echo "ERROR: Could not find module Makefile"
	echo "       Run this file in the module directory"
	exit 1
fi

if ! egrep "Makefile\.modules" Makefile >/dev/null ; then
	echo "ERROR: Doesn't look like a module..."
	exit 1
fi

if ! egrep '^#[ \t]*include[ \t]*".*\/db\/db(_(cap|con|id|key|op|pool|query|res|row|ut|val))?\.h[ \t]*"' *.[ch] >/dev/null ; then
	echo "The module does not seem to include old database headers..."
	exit 0
fi

echo -n "Updating Makefile..."
cp Makefile Makefile.backup
cat Makefile.backup | gawk '
BEGIN {
    serlibpath_seen = 0
    libs_seen = 0
    defs_seen = 0
}

# If the define already exists then skip it, this ensures that
# we do not add the same line more than once.
/^[ \t]*DEFS[ \t]*\+?=.*OPENSER_MOD_INTERFACE/ {
    defs_seen = 1
}

/^[ \t]*DEFS[ \t]*\+?=.*KAMAILIO_MOD_INTERFACE/ {
    defs_seen = 1
}

/^[ \t]*SER_LIBS[ \t]*\+?=.*srdb1\/srdb1/ {
    libs_seen = 1
}

/^[ \t]*SERLIBPATH[ \t]*=/ {
    serlibpath_seen = 1
}

# Write everything just before the line including Makefile.modules,
# this is most likely the last line in the Makefile
/^[ \t]*include[ \t]+.*\/Makefile\.modules[ \t]*$/ {
    if (serlibpath_seen == 0) print "SERLIBPATH=../../lib"
    if (defs_seen == 0) print "DEFS+=-DKAMAILIO_MOD_INTERFACE"
    if (libs_seen == 0) print "SER_LIBS+=$(SERLIBPATH)/srdb1/srdb1"
}

{ print $0 }

' > Makefile
echo "done."

for file in *.[ch] ; do
	echo -n "Updating file $file..."
	cp $file $file.backup
	cat $file.backup | gawk '

/^#[ \t]*include[ \t]*".*\/db\/db(_(cap|con|id|key|op|pool|query|res|row|ut|val))?\.h[ \t]*"/ {
    sub("/db/", "/lib/srdb1/", $0);
}

/(^|[^a-zA-Z0-9_])(db_(con|res)_t|struct[ \t]+db_(con|res))([^a-zA-Z0-9_]|$)/ {
    gsub("struct[ \t]+db_con", "struct db1_con", $0);
    gsub("struct[ \t]+db_res", "struct db1_res", $0);
    gsub("db_con_t", "db1_con_t", $0);
    gsub("db_res_t", "db1_res_t", $0);
}

/(^|[^a-zA-Z0-9_])DB_((BIG)?INT|DOUBLE|STR(ING)?|DATETIME|BLOB|BITMAP)([^a-zA-Z0-9_]|$)/ {
    gsub("DB_INT", "DB1_INT", $0);
    gsub("DB_BIGINT", "DB1_BIGINT", $0);
    gsub("DB_DOUBLE", "DB1_DOUBLE", $0);
    gsub("DB_STR", "DB1_STR", $0);
    gsub("DB_DATETIME", "DB1_DATETIME", $0);
    gsub("DB_BLOB", "DB1_BLOB", $0);
    gsub("DB_BITMAP", "DB1_BITMAP", $0);
}

{ print $0 }
' >$file
	echo "done."
done
