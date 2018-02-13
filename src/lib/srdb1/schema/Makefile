#
#
#

# Kamailio database descriptions for modules
TABLES := $(patsubst kamailio-%.xml,%,$(wildcard kamailio-*.xml))

ROOTDIR=../../../..
ROOTDOC=../../..
STYLESHEETS=$(ROOTDIR)/doc/stylesheets/dbschema_k/xsl
SCHEME=$(ROOTDIR)/utils/kamctl/

#Stylesheet used to generate db_table nodes for pi_framework XML schema
PI_FRAMEWORK_TABLE_XSL = $(STYLESHEETS)/pi_framework_table.xsl

# Stylesheet used to generate mod nodes for pi_framework XML schema
PI_FRAMEWORK_MOD_XSL = $(STYLESHEETS)/pi_framework_mod.xsl

# Stylesheet used to generate MySQL database schema
MYSQL_XSL = $(STYLESHEETS)/mysql.xsl

# Stylesheet used to generate Postgres database schema
POSTGRES_XSL = $(STYLESHEETS)/postgres.xsl

# Stylesheet used to generate dbtext database schema
DBTEXT_XSL = $(STYLESHEETS)/dbtext.xsl

# Stylesheet used to generate berkeley database schema
DB_BERKELEY_XSL = $(STYLESHEETS)/db_berkeley.xsl

# Stylesheet used to generate sqlite database schema
DB_SQLITE_XSL = $(STYLESHEETS)/db_sqlite.xsl

# Stylesheet used to generate oracle database schema
ORACLE_XSL = $(STYLESHEETS)/oracle.xsl

# Stylesheet used to generate mongodb database schema
MONGODB_XSL = $(STYLESHEETS)/mongodb.xsl

# Stylesheet used to generate Redis database schema
DB_REDIS_XSL = $(STYLESHEETS)/db_redis.xsl

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
CATALOG = $(ROOTDIR)/doc/stylesheets/dbschema_k/catalog.xml

XSLTPROC = /usr/bin/xsltproc
XSLTPROC_FLAGS = --xinclude

ifeq ($(VALIDATE), 0)
	override XSLTPROC := $(XSLTPROC) --novalid
endif

ifeq ($(VERBOSE), 1)
	override XSLTPROC := $(XSLTPROC) --verbose
endif

all: mysql postgres dbtext db_berkeley db_sqlite oracle mongodb db_redis pi_framework

.PHONY: pi_framework pi_framework_clean
pi_framework:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/xhttp_pi" \
		--stringparam prefix "$$FILE-" \
		$(PI_FRAMEWORK_TABLE_XSL) kamailio-"$$FILE".xml ; \
	done
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/xhttp_pi" \
		--stringparam prefix "$$FILE-" \
		$(PI_FRAMEWORK_MOD_XSL) kamailio-"$$FILE".xml ; \
	done
	cat $(SCHEME)/xhttp_pi/pi_framework-00 \
		$(SCHEME)/xhttp_pi/*-table \
		$(SCHEME)/xhttp_pi/pi_framework-01 \
		$(SCHEME)/xhttp_pi/*-mod \
		$(SCHEME)/xhttp_pi/pi_framework-02 > \
		$(SCHEME)/xhttp_pi/pi_framework.xml

pi_framework_clean:
	-@rm -f $(ROOTDIR)/scripts/xhttp_pi/*-table
	-@rm -f $(ROOTDIR)/scripts/xhttp_pi/*-mod

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
	for FILE in $(sort $(wildcard $(SCHEME)/dbtext/kamailio/*)) ; do \
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
	for FILE in $(sort $(wildcard $(SCHEME)/db_berkeley/kamailio/*)) ; do \
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


.PHONY: db_sqlite db_sqlite_clean
db_sqlite:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/db_sqlite" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "db_sqlite" \
		$(DB_SQLITE_XSL) kamailio-"$$FILE".xml ; \
	done

db_sqlite_clean:
	-@rm -f $(SCHEME)/db_sqlite/*

.PHONY: mongodb mongodb_clean
mongodb:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/mongodb/kamailio" \
		--stringparam prefix "" \
		--stringparam db "mongodb" \
		$(MONGODB_XSL) kamailio-"$$FILE".xml ; \
	done
	@# small hack to create the version table entries, this is here easier as with XSL
	@echo "use kamailio;" \
		> "$(SCHEME)/mongodb/kamailio/version-create.mongo" ;
	@echo "db.createCollection(\"version\");" \
		>> "$(SCHEME)/mongodb/kamailio/version-create.mongo" ;
	@for FILE in $(sort $(wildcard $(SCHEME)/mongodb/kamailio/*.json)) ; do \
		if [ -f "$$FILE" ]; then \
			if [  "$$FILE" != "$(SCHEME)/mongodb/kamailio/version.json" ]; then \
				VN=`grep '"version":' "$$FILE" | grep -o -E '[0-9]+'` ; \
				FN=`basename $$FILE .json` ;\
				echo "db.getCollection(\"version\").insert({ table_name: \"$$FN\", table_version: NumberInt($$VN) });" \
						>> "$(SCHEME)/mongodb/kamailio/version-create.mongo" ; \
			fi ; \
		fi ; \
	done ; \

mongodb_clean:
	-@rm -f $(SCHEME)/mongodb/kamailio/*

.PHONY: db_redis db_redis_clean
db_redis:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(SCHEME)/db_redis/kamailio" \
		--stringparam prefix "" \
		--stringparam db "db_redis" \
		$(DB_REDIS_XSL) kamailio-"$$FILE".xml ; \
	done

db_redis_clean:
	-@rm -f $(SCHEME)/db_redis/*

.PHONY: docbook-xml
docbook-xml:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOTDIR)/doc/databases/kamailio" \
		--stringparam prefix "$$FILE-" \
		$(DOCBOOK_XSL) kamailio-"$$FILE".xml ; \
	done
	# link all documents to one file, to get nicer output
	echo "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>" \
		> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	echo "<!DOCTYPE book PUBLIC \"-//OASIS//DTD DocBook XML V4.4//EN\"" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	echo "        \"http://www.oasis-open.org/docbook/xml/4.4/docbookx.dtd\" [" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	# include general documentation entities
	echo "    <!ENTITY % docentities SYSTEM \"$(ROOTDOC)/doc/docbook/entities.xml\">" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	echo "    %docentities;" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	echo "]>" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	echo "<book xmlns:xi=\"http://www.w3.org/2001/XInclude\">" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	# add bookinfo
	cat "$(ROOTDIR)/doc/stylesheets/dbschema_k/bookinfo.xml" | tail -n +2 \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	# actually include the entities
	for FILE in $(sort $(wildcard $(ROOTDIR)/doc/databases/kamailio/*-dbschema.xml)); do \
		echo "    <xi:include href=\"$$(basename $$FILE)\" />" \
			>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml" ; \
	done
	echo "</book>" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.xml"
	# generate makefile
	echo "docs = tables.xml" \
		> "$(ROOTDIR)/doc/databases/kamailio/Makefile"
	echo \
		>> "$(ROOTDIR)/doc/databases/kamailio/Makefile"
	echo "docbook_dir = $(ROOTDOC)/doc/docbook" \
		>> "$(ROOTDIR)/doc/databases/kamailio/Makefile"
	echo "include \$$(docbook_dir)/Makefile" \
		>> "$(ROOTDIR)/doc/databases/kamailio/Makefile"

.PHONY: docbook-sgml
docbook-sgml:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOTDIR)/doc/databases/kamailio" \
		--stringparam prefix "$$FILE-" \
		$(DOCBOOK_XSL) kamailio-"$$FILE".xml ; \
	done ; \
	# link all documents to one file, to get nicer output
	printf "<!DOCTYPE book PUBLIC \"-//OASIS//DTD DocBook V4.4//EN\"" \
		> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml"
	echo " \"http://www.oasis-open.org/docbook/sgml/4.4/docbook.dtd\" [" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml"
	# create entities, as xi:include is not available in sgml docbook
	# substitute '-' for '_', docbook smgl don't like this
	for FILE in $(sort $(wildcard $(ROOTDIR)/doc/databases/kamailio/*.xml)); do \
		echo "  <!ENTITY `basename "$$FILE" | sed -e 's#_#-#g'` SYSTEM \"$$(basename $$FILE)\">" \
			>> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml" ; \
	done ; \
	# include general documentation entities
	echo "  <!ENTITY % docentities SYSTEM \"$(ROOTDOC)/doc/docbook/entities.xml\"> %docentities;" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml" ; \
	echo "]>" >> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml"
	# add bookinfo
	cat "$(ROOTDIR)/doc/stylesheets/dbschema_k/bookinfo.xml" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml"
	# actually include the entities
	for FILE in $(wildcard $(ROOTDIR)/doc/databases/kamailio/*.xml); do \
		echo "    &`basename "$$FILE" | sed -e 's#_#-#g'`;" \
		>> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml" ; \
	done ; \
	echo "</book>" >> "$(ROOTDIR)/doc/databases/kamailio/tables.sgml"

.PHONY: docbook docbook_clean
docbook: docbook-xml

docbook_clean:
	-@rm -f $(ROOTDIR)/doc/databases/kamailio/*.xml
	-@rm -f $(ROOTDIR)/doc/databases/kamailio/tables.sgml

.PHONY: modules modules_clean
modules:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOTDIR)/modules_k/$$FILE/" \
		--stringparam prefix "$$FILE" \
		--stringparam db "modules" \
		$(MODULES_H_XSL) kamailio-"$$FILE".xml ; \
	done
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOTDIR)/modules_k/$$FILE/" \
		--stringparam prefix "$$FILE" \
		--stringparam db "modules" \
		$(MODULES_C_XSL) kamailio-"$$FILE".xml ; \
	done

modules_clean:
	for FILE in $(TABLES); do \
		rm -f $(ROOTDIR)/modules/$$FILE/db_*.c; \
		rm -f $(ROOTDIR)/modules/$$FILE/db_*.h; \
	done

dbdoc:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOTDIR)/modules_k/$$FILE/doc/" \
		--stringparam prefix "$$FILE" \
		--stringparam db "modules" \
		$(DBDOC_XSL) kamailio-"$$FILE".xml ; \
	done

dbdoc_clean:
	for FILE in $(TABLES); do \
		rm -f $(ROOTDIR)/modules/$$FILE/doc/*_db.xml; \
	done

.PHONY: clean
clean: mysql_clean postgres_clean oracle_clean dbtext_clean db_berkeley_clean db_sqlite_clean mongodb_clean db_redis_clean pi_framework_clean docbook_clean # modules_clean dbdoc_clean
