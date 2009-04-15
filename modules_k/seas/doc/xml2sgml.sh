#!/bin/bash
xmlstarlet sel -E ISO-8859-1 -t -c "//chapter[@id='user']" seas.xml > seas_user.sgml
xmlstarlet sel -E ISO-8859-1 -t -c "//chapter[@id='devel']" seas.xml > seas_devel.sgml
xmlstarlet sel -E ISO-8859-1 -t -c "//chapter[@id='faq']" seas.xml > seas_faq.sgml
