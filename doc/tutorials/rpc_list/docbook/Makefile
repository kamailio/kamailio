
docs = rpc_list.xml
docbook_dir = ../../../docbook
# no subsection listed in the main TOC
xsltproc_flags+= --stringparam toc.section.depth 0
xsltproc_flags+= --stringparam section.autolabel 1
xsltproc_flags+= --stringparam section.label.includes.component.label 1

include $(docbook_dir)/Makefile

# updates to the html generated file
.PHONY: html-update
html-update:
	# strip first to lines (xml and doctype constraints)
	sed -i '1,2d' $(basename $(docs)).html
