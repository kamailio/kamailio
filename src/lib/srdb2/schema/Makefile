#
# $Id$
#

# File containing SER database description
DOC_ROOT = ser.xml

ROOT=../../..
STYLESHEETS=$(ROOT)/doc/stylesheets/dbschema/xsl

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

# XML Catalog used to resolve entities
CATALOG = $(ROOT)/doc/catalog.xml

XSLTPROC = /usr/bin/xsltproc
XSLTPROC_FLAGS = --xinclude

ifeq ($(VALIDATE), 0)
	override XSLTPROC := $(XSLTPROC) --novalid
endif

all: dbtext mysql postgres oracle docbook

.PHONY: mysql mysql_clean
mysql:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/mysql" \
	    --stringparam prefix "my_" \
	    --stringparam db "mysql" \
	    $(MYSQL_XSL) $(DOC_ROOT)

mysql_clean:
	rm -f $(ROOT)/scripts/mysql/my_*

.PHONY: postgres postgres_clean
postgres:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/postgres" \
	    --stringparam prefix "pg_" \
	    --stringparam db "postgres" \
	    $(POSTGRES_XSL) $(DOC_ROOT)

postgres_clean:
	rm -f $(ROOT)/scripts/postgres/pg_*

.PHONY: oracle oracle_clean
oracle:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/oracle" \
	    --stringparam prefix "or_" \
	    --stringparam db "oracle" \
	    $(ORACLE_XSL) $(DOC_ROOT)

oracle_clean:
	rm -f $ROOT/scripts/oracles/or_*

.PHONY: dbtext dbtext_clean
dbtext:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	--stringparam dir "$(ROOT)/scripts/dbtext/ser_db" \
	--stringparam prefix "" \
	--stringparam db "dbtext" \
	$(DBTEXT_XSL) $(DOC_ROOT)

dbtext_clean:
	rm -f $ROOT/scripts/dbtext/ser_db/*

.PHONY: docbook docbook_clean
docbook:
#	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
#           --stringparam dir "($ROOT)/doc/database" \
#	    --stringparam prefix "" \
#	    $(DOCBOOK_XSL) $(DOC_ROOT)

docbook_clean:


.PHONY: clean
clean: mysql_clean postgres_clean oracle_clean dbtext_clean dbtext_clean docbook_clean

