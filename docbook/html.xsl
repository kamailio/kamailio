<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

    <!-- Produce a single file when building parts of documentation -->
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/onechunk.xsl"/>

    <!-- Common XHTML customization -->
    <xsl:import href="html.common.xsl"/>

	<!-- Display all subsections in in the toc of READMEs -->

	<!-- Enable TOC for sections -->
  	<xsl:param name="toc.section.depth">4</xsl:param>
  	<xsl:param name="generate.section.toc.level">4</xsl:param>

	<!-- This is the default value of generate.toc modified so that we get TOC
	     in documents whose root element is root, but only for the root
	     section element. Non-root section or sectX elements have been removed
		 so that they never generate a TOC. -->
	<xsl:param name="generate.toc">
		appendix  toc,title
		article/appendix  nop
		article   toc,title
		book      toc,title,figure,table,example,equation
		chapter   toc,title
		part      toc,title
		preface   toc,title
		qandadiv  toc
		qandaset  toc
		reference toc,title
		/section   toc
		set       toc,title
	</xsl:param>

</xsl:stylesheet>
