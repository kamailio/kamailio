# This variable contains a list of docbook sources separated by white space to
# be processed by the docbook build system if the user did not specify any
# document on the command line. Documents configure here are processed one
# after another, each of them will be treated as a standalone documentation
# file.
docs ?= index.xml

# Here you can specify the name of the file that will be used to generate a
# module README file. You can override it in a module Makefile. This is
# useful, for example, if you want to use a different docbook file to generate
# README and a different docbook file to generate documentation in other
# formats (HTML, TXT). If you omit this variable in your Makefile then the
# first document specified in DOCS is used to generate module README file.
readme_docs ?= $(docs)

# This is the list of docbook files to be used to generate HTML
# documentation. You can use this variable in your Makefile if you have
# docbook documents that should be processed only when HTML documentation is
# being generated (i.e. you want them to be ignored when plain-text
# documentation is being generated). Multiple documents separated by white
# space can be configured here and they will be processed one after
# another. If you omit this variable then the documents specified in DOCS
# variable will be converted to HTML when HTML documentation is being built.
html_docs ?= $(docs)

# This is the list of docbook files to be used to generate plain-text
# documentation. You can use this variable in your Makefile if you have
# docbook documents that should be processed only when plain-textdocumentation
# is being generated (i.e. you want them to be ignored when HTML documentation
# is being generated). Multiple documents separated by white space can be
# configured here and they will be processed one after another. If you omit
# this variable then the documents specified in DOCS variable will be
# converted to plain-text when plain-text documentation is being built.
txt_docs ?= $(docs)

# Output directory where files produced by XSL stylesheets should be stored,
# by default we output to the same directory, documents that are chunked may
# choose to override this to write all chunks in a subdirectory instead.
output_dir ?= .

# Here you can configure the path to the directory which contains all
# auxiliary files that are needed to convert docbook documentation to other
# formats. You should not need to use this variable under normal
# circumstances.
docbook_dir ?= ../../../docbook
COREPATH ?= $(docbook_dir)/..

# This is the stylesheet used to generate a list of dependencies for a docbook
# source file. The stylesheet walks through the document and prints all files
# that are included in the document. The list of files is then written in a
# standalone file by this Makefile and the file can be included by the
# Makefile.
dep_xsl ?= $(docbook_dir)/dep.xsl

# This is the stylesheet that is used to produce a single HTML file for every
# docbook document being processed. This stylesheet contains configuration for
# docbook XSL scripts that only applies when the build system generates a
# single HTML file.
single_html_xsl ?= $(docbook_dir)/html.xsl

# This stylesheet is used to produce chunked HTML documention, the output will
# be a set of HTML files. This stylesheet contains configuration for docbook
# XSL scripts that only applies when multiple-file HTML documentation is being
# generated.
chunked_html_xsl ?= $(chunked_html_xls)/html.chunked.xsl

# This stylesheet is used to produce plain-text documentation. Internally this
# stylesheet includes the docbook XSL stylesheet used to produce single-file
# HTML documentation and the output is then converted by lynx to a plain-text
# file.
txt_xsl ?= $(docbook_dir)/txt.xsl

# This stylesheet is used when the docbook build system generates a module
# README. The process is similar to the process of generating plain-text file,
# but we use a separate stylesheet here because the configuration of docbook
# XSL stylesheets can be a bit different for READMEs (i.e. we do not want to
# have list of examples in READMES, etc.)
readme_xsl ?= $(docbook_dir)/readme.xsl

# Use this configuration variable to enable or disable docbook document
# validation before the build system converts the document into another
# format.
validate ?= 0

# A list of CSS stylesheets separated by whitespace to be included in HTML
# files.
html_css ?= /css/sr-doc.css

# This is the catalog file used by xsltproc to locate docbook schema files
# needed to validate docbook documents and docbook XSL stylesheets needed
# convert docbook documents to other formats. All docbook documents in the
# sip-router projects refer to schema files using public
# identifiers. Similarly customization stylesheets refer to docbook XSL
# stylesheets using public URIs. The catalog file is then used to convert
# public identifiers and public URIs to files on local filesystem. The default
# version of the catalog file only refers to file /etc/xml/catalog, this is
# the site-wide catalog file present in Debian and possibly other systems
# too. This file is updated by the package manager whenever docbook schema
# file or docbook xsl stylesheets are being installed.
catalog  ?= $(docbook_dir)/catalog.xml

# Use standard catalog from OS, set it to no to use the above path to a
# custom catalog file.
nocatalog ?= yes

lynx     ?= lynx
dia      ?= dia
xsltproc ?= xsltproc
xmllint  ?= xmllint

lynx_flags     ?= -nolist
dia_args       ?=
xsltproc_flags ?=
xmllint_flags  ?= --xinclude --postvalid --noout

ifeq ($(validate), 0)
	override xsltproc_flags := $(xsltproc_flags) --novalid
endif

ifeq ($(nocatalog),yes)
XMLCATALOGX=
else
XMLCATALOGX=XML_CATALOG_FILES=$(catalog)
endif

all_deps = Makefile $(docbook_dir)/Makefile $(docbook_dir)/entities.xml \
		   $(dep_xsl) $(catalog) $(extra_deps)

all: txt

html_files = $(addprefix $(output_dir)/, $(addsuffix .html,\
			 $(basename $(html_docs))))
html_deps = $(addsuffix .d, $(basename $(html_docs)))

txt_files = $(addprefix $(output_dir)/, $(addsuffix .txt,\
			$(basename $(txt_docs))))
txt_deps = $(addsuffix .d, $(basename $(txt_docs)))

readme_deps = $(addsuffix .d, $(basename $(readme_docs)))

html xhtml single_html single_xhtml: $(html_files)
txt text plaintext: $(txt_files)
readme README: ../README

../README: $(readme_docs) $(readme_deps) $(readme_xsl) $(all_deps)
	$(XMLCATALOGX) $(xsltproc) $(xsltproc_flags) \
		--xinclude \
        $(readme_xsl) $< | $(lynx) $(lynx_flags) -stdin -dump > $@

$(output_dir)/%.html: %.xml %.d $(single_html_xsl) $(all_deps)
	$(XMLCATALOGX) $(xsltproc) $(xsltproc_flags) \
        --xinclude \
        --stringparam base.dir "$(output_dir)/" \
        --stringparam root.filename "$(basename $<)" \
        --stringparam html.stylesheet "$(html_css)" \
        --stringparam html.ext ".html" \
        $(single_html_xsl) $<


$(output_dir)/%.txt: %.xml %.d $(txt_xsl) $(all_deps)
	$(XMLCATALOGX) $(xsltproc) $(xsltproc_flags) \
		--xinclude \
        $(txt_xsl) $< | $(lynx) $(lynx_flags) -stdin -dump > $@

%.png: %.dia $(all_dep)
	$(dia) $(dia_flags) -t png -e $@ $<

%.d: %.xml $(all_dep) 
	$(xsltproc) $(xsltproc_flags) \
        --nonet \
        --novalid \
        --stringparam output "$@" \
        $(dep_xsl) $< 

.PHONY: check
check: $(docs) $(html_docs) $(txt_docs) $(readme_docs)
	$(XMLCATALOGX) $(xmllint) $(xmllint_flags) $<

.PHONY: clean
clean:
	-@rm -f $(html_files) $(txt_files)


.PHONY: proper realclean distclean maintainer-clean
proper realclean distclean maintainer-clean: clean
	-rm -f $(html_deps) $(txt_deps) $(readme_deps) *~

maintainer-clean: clean-tmp

.PHONY: clean-tmp
clean-tmp:
	-rm -f TAGS tags *.dbg .*.swp


ifeq (,$(MAKECMDGOALS))
include $(html_deps) $(txt_deps) $(readme_deps)
else
ifeq (,$(strip $(nodep_targets)))
include $(COREPATH)/Makefile.targets
endif
ifneq (,$(filter-out $(nodep_targets) check, $(MAKECMDGOALS)))
include $(html_deps) $(txt_deps) $(readme_deps)
endif
endif
