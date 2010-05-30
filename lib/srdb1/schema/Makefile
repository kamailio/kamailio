#
# $Id$
#

# Kamailio database descriptions for modules
TABLES := $(patsubst kamailio-%.xml,%,$(wildcard kamailio-*.xml))

ROOT=../../../
STYLESHEETS=$(ROOT)/doc/stylesheets/dbschema_k/xsl
SCHEME=$(ROOT)/utils/kamctl/

# Stylesheet used to generate MySQL database schema
MYSQL_XSL = $(STYLESHEETS)/mysql.xsl

# Stylesheet used to generate Postgres database schema
POSTGRES_XSL = $(STYLESHEETS)/postgres.xsl

# Stylesheet used to generate dbtext database schema
DBTEXT_XSL = $(STYLESHEETS)/dbtext.xsl

# Stylesheet used to generate berkeley database schema
DB_BERKELEY_XSL = $(STYLESHEETS)/db_berkeley.xsl

# Stylesheet used to generate oracle database schema
ORACLE_XSL = $(STYLESHEETS)/oracle.xsl

# Stylesheet used to generate docbook documentation
DOCBOOK_XSL = $(STYLESHEETS)/docbook.xsl

# Stylesheet used to generate modules templates header
MODULES_H_XSL = $(STYLESHEETS)/modules_h.xsl

# Stylesheet used to generate modules templates implementation
MODULES_C_XSL = $(STYLESHEETS)/modules_c.xsl

# Stylesheet used to generate modules documentation
DBDOC_XSL = $(STYLESHEETS)/dbdoc.xsl

# Enable/disable DTD validation
VALIDATE = 0

# Enable/disable verbose output (and DTD validation)
VERBOSE = 0

# XML Catalog used to resolve entities
CATALOG = $(ROOT)/doc/stylesheets/dbschema_k/catalog.xml

XSLTPROC = /usr/bin/xsltproc
XSLTPROC_FLAGS = --xinclude

ifeq ($(VALIDATE), 0)
	override XSLTPROC := $(XSLTPROC) --novalid
endif

ifeq ($(VERBOSE), 1)
	override XSLTPROC := $(XSLTPROC) --verbose
endif

all: mysql postgres dbtext db_berkeley docbook oracle #modules dbdoc

.PHONY: mysql mysql_clean
mysql:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/mysql" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "mysql" \
		$(MYSQL_XSL) kamailio-"$$FILE".xml ; \
	done

mysql_clean:
	-@rm -f $(SCHEME)/mysql/*

.PHONY: postgres postgres_clean
postgres:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/postgres" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "postgres" \
		$(POSTGRES_XSL) kamailio-"$$FILE".xml ; \
	done

postgres_clean:
	-@rm -f $(SCHEME)/postgres/*

.PHONY: oracle oracle_clean
oracle:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/oracle" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "oracle" \
		$(ORACLE_XSL) kamailio-"$$FILE".xml ; \
	done

oracle_clean:
	-@rm -f $(SCHEME)/oracle/*.sql

.PHONY: dbtext dbtext_clean
dbtext:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/dbtext/kamailio" \
		--stringparam prefix "" \
		--stringparam db "dbtext" \
		$(DBTEXT_XSL) kamailio-"$$FILE".xml ; \
	done
	TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`
	# small hack to create the version table entries, this is here easier as with XSL
	for FILE in  $(wildcard $(SCHEME)/dbtext/kamailio/*) ; do \
		if [ -f "$$FILE" ]; then \
			if [  "$$FILE" != "$(SCHEME)/dbtext/kamailio/version" ]; then \
				tail -n 1 "$$FILE" >> "$(SCHEME)/dbtext/kamailio/version" ; \
 				head -n 1 "$$FILE" > $TMPFILE ; \
				cp $TMPFILE "$$FILE" ; \
			fi ; \
		fi ; \
	done ; \
	rm -f $TMPFILE

dbtext_clean:
	-@rm -f $(SCHEME)/dbtext/kamailio/*

.PHONY: db_berkeley db_berkeley_clean
db_berkeley:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/db_berkeley/kamailio" \
		--stringparam prefix "" \
		--stringparam db "db_berkeley" \
		$(DB_BERKELEY_XSL) kamailio-"$$FILE".xml ; \
	done
	TMPFILE=`mktemp -t kamailio-test.XXXXXXXXXX`
	# small hack to create the version table entries, this is here easier as with XSL
	for FILE in  $(wildcard $(SCHEME)/db_berkeley/kamailio/*) ; do \
		if [ -f "$$FILE" ]; then \
			if [  "$$FILE" != "$(SCHEME)/db_berkeley/kamailio/version" ]; then \
				tail -n 2 "$$FILE" >> "$(SCHEME)/db_berkeley/kamailio/version" ; \
 				head -n 10 "$$FILE" > $TMPFILE ; \
				cp $TMPFILE "$$FILE" ; \
			fi ; \
		fi ; \
	done ; \
	rm -f $TMPFILE

db_berkeley_clean:
	-@rm -f $(SCHEME)/db_berkeley/kamailio/*


.PHONY: docbook docbook_clean
docbook:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/doc/databases/kamailio" \
		--stringparam prefix "$$FILE-" \
		$(DOCBOOK_XSL) kamailio-"$$FILE".xml ; \
	done ; \

	# link all documents to one file, to get nicer output
	echo "<!DOCTYPE book PUBLIC \"-//OASIS//DTD DocBook V4.4//EN\" \"http://www.oasis-open.org/docbook/sgml/4.4/docbook.dtd\"[" > "$(ROOT)/doc/databases/kamailio/tables.sgml"
	
	# create entities, as xi:include is not available in sgml docbook
	# substitute '-' for '_', docbook smgl don't like this
	for FILE in $(wildcard $(ROOT)/doc/databases/kamailio/*.xml); do \
		echo "  <!ENTITY `basename "$$FILE" | sed -e 's#_#-#g'` SYSTEM \""$$FILE"\">" >> "$(ROOT)/doc/databases/kamailio/tables.sgml" ; \
	done ; \
	#Include general documentation entities
	echo "  <!ENTITY % docentities SYSTEM \"../../../docbook/entities.xml\"> %docentities;" >> "$(ROOT)/doc/databases/kamailio/tables.sgml" ; \
	echo "]>" >> "$(ROOT)/doc/databases/kamailio/tables.sgml"
	# add bookinfo
	cat "$(ROOT)/doc/stylesheets/dbschema_k/bookinfo.xml" >> "$(ROOT)/doc/databases/kamailio/tables.sgml"
	# actually include the entities
	for FILE in $(wildcard $(ROOT)/doc/databases/kamailio/*.xml); do \
		echo "    &`basename "$$FILE" | sed -e 's#_#-#g'`" >> "$(ROOT)/doc/databases/kamailio/tables.sgml" ; \
	done ; \
	echo "</book>" >> "$(ROOT)/doc/databases/kamailio/tables.sgml"

docbook_clean:
	-@rm -f $(ROOT)/doc/databases/kamailio/*.xml
	-@rm -f $(ROOT)/doc/databases/kamailio/tables.sgml

.PHONY: modules modules_clean
modules:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/modules_k/$$FILE/" \
		--stringparam prefix "$$FILE" \
		--stringparam db "modules" \
		$(MODULES_H_XSL) kamailio-"$$FILE".xml ; \
	done
	
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/modules_k/$$FILE/" \
		--stringparam prefix "$$FILE" \
		--stringparam db "modules" \
		$(MODULES_C_XSL) kamailio-"$$FILE".xml ; \
	done

modules_clean:
	for FILE in $(TABLES); do \
		rm -f $(ROOT)/modules/$$FILE/db_*.c; \
		rm -f $(ROOT)/modules/$$FILE/db_*.h; \
	done

dbdoc:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/modules_k/$$FILE/doc/" \
		--stringparam prefix "$$FILE" \
		--stringparam db "modules" \
		$(DBDOC_XSL) kamailio-"$$FILE".xml ; \
	done

dbdoc_clean:
	for FILE in $(TABLES); do \
		rm -f $(ROOT)/modules/$$FILE/doc/*_db.xml; \
	done

.PHONY: clean
clean: mysql_clean postgres_clean oracle_clean dbtext_clean db_berkeley_clean docbook_clean # modules_clean dbdoc_clean
