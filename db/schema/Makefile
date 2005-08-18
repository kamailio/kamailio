#
# $Id$
#

# File containing SER database description
DOC_ROOT = ser.xml

# Stylesheet used to generate MySQL database schema
MYSQL_XSL = ../../doc/stylesheets/dbschema/xsl/mysql.xsl

# Stylesheet used to generate Postgres database schema
POSTGRES_XSL = ../../doc/stylesheets/dbschema/xsl/postgres.xsl

# Stylesheet used to generate dbtext database schema
DBTEXT_XSL = ../../doc/stylesheets/dbschema/xsl/dbtext.xsl

# Stylesheet used to generate oracle database schema
ORACLE_XSL = ../../doc/stylesheets/dbschema/xsl/oracle.xsl

# Stylesheet used to generate docbook documentation
DOCBOOK_XSL = ../../doc/stylesheets/dbschema/xsl/docbook.xsl

# Enable/disable DTD validation
VALIDATE = 1

# XML Catalog used to resolve entities
CATALOG = ../../doc/catalog.xml

XSLTPROC = /usr/bin/xsltproc
XSLTPROC_FLAGS = --xinclude

ifeq ($(VALIDATE), 0)
	override XSLTPROC := $(XSLTPROC) --novalid
endif

all: dbtext mysql postgres oracle docbook

.PHONY: mysql
mysql:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam prefix mysql $(MYSQL_XSL) $(DOC_ROOT)

.PHONY: postgres
postgres:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam prefix postgres $(POSTGRES_XSL) $(DOC_ROOT)

.PHONY: oracle
oracle:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam prefix oracle $(ORACLE_XSL) $(DOC_ROOT)

.PHONY: dbtext
dbtext:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam prefix dbtext $(DBTEXT_XSL) $(DOC_ROOT)

.PHONY: docbook
docbook:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam prefix docbook $(DOCBOOK_XSL) $(DOC_ROOT)

.PHONY: clean
clean:
	rm -rf mysql postgres oracle dbtext docbook

