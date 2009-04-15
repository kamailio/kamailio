#
# The list of documents to build (without extensions)
#
DOCUMENTS = pa

#
# The root directory containing Makefile.doc
#
ROOT_DIR=../../..

#
# Validate docbook documents before generating output
# (may be slow)
#
#VALIDATE=1

#
# You can override the stylesheet used to generate
# xhtml documents here
#
#XHTML_XSL=$(ROOT_DIR)/doc/stylesheets/xhtml.xsl

#
# You can override the stylesheet used to generate
# plain text documents here
#
#TXT_XSL=$(XHTML_XSL)

DB_XSL = $(ROOT_DIR)/doc/stylesheets/dbschema2docbook.xsl
DB_DIR = $(ROOT_DIR)/db/schema

DB_FILES = $(DB_DIR)/presentity.xml 

EXTRA_DEPS = pa_base.xml pa_db_src.xml $(DB_XSL)

include $(ROOT_DIR)/Makefile.doc

pa_db.xml:	pa_db_src.xml pa_db_src.d
			XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ --xinclude $(DB_XSL) $<

pa_db_src.d: pa_db_src.xml $(alldep) 
	XML_CATALOG_FILES=$(CATALOG) $(XSLTPROC) $(XSLTPROC_FLAGS) \
        --nonet \
        --novalid \
        --stringparam output "$@" \
        $(DEP_XSL) $< 

clean:	db_clean

db_clean:	
			rm -f pa_db.xml

ifeq (,$(MAKECMDGOALS))
include pa_db_src.d
endif
ifneq (,$(filter-out clean proper realclean distclean check, $(MAKECMDGOALS)))
include pa_db_src.d
endif
