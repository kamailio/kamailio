#
# $Id$
#

# File containing OpenSER core database descriptions
DOC_CORE = openser-core.xml
# File containing OpenSER extra database descriptions
DOC_EXTRA = openser-extra.xml
# File containing OpenSER serweb database descriptions
DOC_SERWEB = openser-serweb.xml
# File containing OpenSER presence database descriptions
DOC_PRESENCE = openser-presence.xml

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

# XML Catalog used to resolve entities
CATALOG = $(ROOT)/doc/catalog.xml

XSLTPROC = /usr/bin/xsltproc
XSLTPROC_FLAGS = --xinclude

ifeq ($(VALIDATE), 0)
	override XSLTPROC := $(XSLTPROC) --novalid
endif

all: mysql dbtext mysql postgres #oracle docbook

.PHONY: mysql mysql_clean
mysql:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/mysql" \
	    --stringparam prefix "core-" \
	    --stringparam db "mysql" \
	    $(MYSQL_XSL) $(DOC_CORE)
	    
	    
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/mysql" \
	    --stringparam prefix "extra-" \
	    --stringparam db "mysql" \
	    $(MYSQL_XSL) $(DOC_EXTRA)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/mysql" \
	    --stringparam prefix "serweb-" \
	    --stringparam db "mysql" \
	    $(MYSQL_XSL) $(DOC_SERWEB)
    
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/mysql" \
	    --stringparam prefix "presence-" \
	    --stringparam db "mysql" \
	    $(MYSQL_XSL) $(DOC_PRESENCE)

mysql_clean:
	rm -f $(ROOT)/scripts/mysql/my_*

.PHONY: postgres postgres_clean
postgres:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/postgres" \
	    --stringparam prefix "core-" \
	    --stringparam db "postgres" \
	    $(POSTGRES_XSL) $(DOC_CORE)
	    
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/postgres" \
	    --stringparam prefix "extra-" \
	    --stringparam db "postgres" \
	    $(POSTGRES_XSL) $(DOC_EXTRA)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/postgres" \
	    --stringparam prefix "serweb-" \
	    --stringparam db "postgres" \
	    $(POSTGRES_XSL) $(DOC_SERWEB)
	    
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/postgres" \
	    --stringparam prefix "presence-" \
	    --stringparam db "postgres" \
	    $(POSTGRES_XSL) $(DOC_PRESENCE)


postgres_clean:
	rm -f $(ROOT)/scripts/postgres/pg_*

.PHONY: oracle oracle_clean
oracle:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/oracle" \
	    --stringparam prefix "core-" \
	    --stringparam db "oracle" \
	    $(ORACLE_XSL) $(DOC_CORE)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/oracle" \
	    --stringparam prefix "extra-" \
	    --stringparam db "oracle" \
	    $(ORACLE_XSL) $(DOC_EXTRA)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/oracle" \
	    --stringparam prefix "serweb-" \
	    --stringparam db "oracle" \
	    $(ORACLE_XSL) $(DOC_SERWEB)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	    --stringparam dir "$(ROOT)/scripts/oracle" \
	    --stringparam prefix "presence-" \
	    --stringparam db "oracle" \
	    $(ORACLE_XSL) $(DOC_PRESENCE)

oracle_clean:
	rm -f $ROOT/scripts/oracles/or_*

.PHONY: dbtext dbtext_clean
dbtext:
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	--stringparam dir "$(ROOT)/scripts/dbtext/openser_db" \
	--stringparam prefix "" \
	--stringparam db "dbtext" \
	$(DBTEXT_XSL) $(DOC_CORE)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	--stringparam dir "$(ROOT)/scripts/dbtext/openser_db" \
	--stringparam prefix "" \
	--stringparam db "dbtext" \
	$(DBTEXT_XSL) $(DOC_EXTRA)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	--stringparam dir "$(ROOT)/scripts/dbtext/openser_db" \
	--stringparam prefix "" \
	--stringparam db "dbtext" \
	$(DBTEXT_XSL) $(DOC_SERWEB)

	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
	--stringparam dir "$(ROOT)/scripts/dbtext/openser_db" \
	--stringparam prefix "" \
	--stringparam db "dbtext" \
	$(DBTEXT_XSL) $(DOC_PRESENCE)

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
clean: mysql_clean postgres_clean oracle_clean dbtext_clean docbook_clean

