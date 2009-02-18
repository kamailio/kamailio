#!/usr/bin/env sh
#
# This is a simple script which attempts to convert ser modules so that they
# can be used with the sip-router core. Most of the changes done by the script
# deal with the changes in the database abstraction layer in the sip-router
# source tree.
#
# Run this script in module directory to convert it from ser core to
# sip-router core. The root of the tree should be two levels up, otherwise
# relative paths to headers (../..) would not work and the module will not
# compile.
# 
# Some of the changes done by the script:
#
#  * Extra defines in the Makefile to make the module link with libsrdb2
#  * Path to database headers updated to point to lib/srdb2
#  * Database flag names renamed from DB_* to SRDB_* 
#    (DB_DISABLED -> SRDB_DISABLED)
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

if ! egrep '^#[ \t]*include[ \t]*".*\/db\/db(_(cmd|con|ctx|drv|fld|gen|pool|rec|res|uri))?\.h[ \t]*"' *.[ch] >/dev/null ; then
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
/^[ \t]*DEFS[ \t]*\+?=.*SER_MOD_INTERFACE/ {
    defs_seen = 1
}

/^[ \t]*SER_LIBS[ \t]*\+?=.*srdb2\/srdb2/ {
    libs_seen = 1
}

/^[ \t]*SERLIBPATH[ \t]*=/ {
    serlibpath_seen = 1
}

# Write everything just before the line including Makefile.modules,
# this is most likely the last line in the Makefile
/^[ \t]*include[ \t]+.*\/Makefile\.modules[ \t]*$/ {
    if (serlibpath_seen == 0) print "SERLIBPATH=../../lib"
    if (defs_seen == 0) print "DEFS+=-DSER_MOD_INTERFACE"
    if (libs_seen == 0) print "SER_LIBS+=$(SERLIBPATH)/srdb2/srdb2"
}

{ print $0 }

' > Makefile
echo "done."

for file in *.[ch] ; do
	echo -n "Updating file $file..."
	cp $file $file.backup
	cat $file.backup | gawk '

/^#[ \t]*include[ \t]*".*\/db\/db(_(cmd|con|ctx|drv|fld|gen|pool|rec|res|uri))?\.h[ \t]*"/ {
    sub("/db/", "/lib/srdb2/", $0);
}

/(^|[^a-zA-Z0-9_])DB_(LOAD_SER|DISABLED|CANON|IS_(TO|FROM)|FOR_SERWEB|PENDING|((CALLER|CALLEE)_)?DELETED|MULTIVALUE|FILL_ON_REG|REQUIRED|DIR)([^a-zA-Z0-9_]|$)/ {
    gsub("DB_LOAD_SER", "SRDB_LOAD_SER", $0);
    gsub("DB_DISABLED", "SRDB_DISABLED", $0);
    gsub("DB_CANON", "SRDB_CANON", $0);
    gsub("DB_IS_TO", "SRDB_IS_TO", $0);
    gsub("DB_IS_FROM", "SRDB_IS_FROM", $0);
    gsub("DB_FOR_SERWEB", "SRDB_FOR_SERWEB", $0);
    gsub("DB_PENDING", "SRDB_PENDING", $0);
    gsub("DB_DELETED", "SRDB_DELETED", $0);
    gsub("DB_CALLER_DELETED", "SRDB_CALLER_DELETED", $0);
    gsub("DB_CALLEE_DELETED", "SRDB_CALLEE_DELETED", $0);
    gsub("DB_MULTIVALUE", "SRDB_MULTIVALUE", $0);
    gsub("DB_FILL_ON_REG", "SRDB_FILL_ON_REG", $0);
    gsub("DB_REQUIRED", "SRDB_REQUIRED", $0);
    gsub("DB_DIR", "SRDB_DIR", $0);
}

{ print $0 }
' >$file
	echo "done."
done
