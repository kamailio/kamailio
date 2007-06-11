#
# $Id$
#

# OpenSER database descriptions for modules
TABLES = standard acc imc lcr siptrace domainpolicy cpl domain group \
	permissions registrar usrloc msilo alias_db uri_db speeddial \
	avpops auth_db pdt presence serweb extensions

ROOT=../..
STYLESHEETS=$(ROOT)/doc/dbschema/xsl

# Stylesheet used to generate MySQL database schema
MYSQL_XSL = $(STYLESHEETS)/mysql.xsl

# Stylesheet used to generate Postgres database schema
POSTGRES_XSL = $(STYLESHEETS)/postgres.xsl

# Stylesheet used to generate dbtext database schema
DBTEXT_XSL = $(STYLESHEETS)/dbtext.xsl

# Stylesheet used to generate oracle database schema
ORACLE_XSL = $(STYLESHEETS)/oracle.xsl

# Stylesheet used to generate docbook documentation
DOCBOOK_XSL = $(STYLESHEETS)/docbook.xsl

# Enable/disable DTD validation
VALIDATE = 0

# Enable/disable verbose output (and DTD validation)
VERBOSE = 0

# XML Catalog used to resolve entities
CATALOG = $(ROOT)/doc/dbschema/catalog.xml

XSLTPROC = /usr/bin/xsltproc
XSLTPROC_FLAGS = --xinclude

ifeq ($(VALIDATE), 0)
	override XSLTPROC := $(XSLTPROC) --novalid
endif

ifeq ($(VERBOSE), 1)
	override XSLTPROC := $(XSLTPROC) --verbose
endif

all: mysql postgres dbtext #docbook oracle

.PHONY: mysql mysql_clean
mysql:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/scripts/mysql" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "mysql" \
		$(MYSQL_XSL) openser-"$$FILE".xml ; \
	done

mysql_clean:
	rm -f $(ROOT)/scripts/mysql/*

.PHONY: postgres postgres_clean
postgres:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/scripts/postgres" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "postgres" \
		$(POSTGRES_XSL) openser-"$$FILE".xml ; \
	done

postgres_clean:
	rm -f $(ROOT)/scripts/postgres/*

.PHONY: oracle oracle_clean
oracle:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/scripts/oracle" \
		--stringparam prefix "$$FILE-" \
		--stringparam db "oracle" \
		$(ORACLE_XSL) openser-"$$FILE".xml ; \
	done

oracle_clean:
	rm -f $(ROOT)/scripts/oracle/*

.PHONY: dbtext dbtext_clean
dbtext:
	for FILE in $(TABLES); do \
		XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
		--stringparam dir "$(ROOT)/scripts/dbtext/openser_db" \
		--stringparam prefix "" \
		--stringparam db "dbtext" \
		$(DBTEXT_XSL) openser-"$$FILE".xml ; \
	done

dbtext_clean:
	rm -f $(ROOT)/scripts/dbtext/openser_db/*

.PHONY: docbook docbook_clean
docbook:
#	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
#           --stringparam dir "$(ROOT)/doc/database" \
#	    --stringparam prefix "" \
#	    $(DOCBOOK_XSL) $(DOC_ROOT)

docbook_clean:


.PHONY: clean
clean: mysql_clean postgres_clean oracle_clean dbtext_clean docbook_clean
