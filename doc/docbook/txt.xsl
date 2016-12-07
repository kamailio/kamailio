<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

    <!-- Produce a single file when building parts of documentation -->
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>

    <!-- Common XHTML customization -->
    <xsl:import href="html.common.xsl"/>

	<!-- Display all subsections in in the toc of READMEs -->
  	<xsl:param name="toc.section.depth">4</xsl:param>
  	<!-- Only the first section in the output should contain toc -->
  	<xsl:param name="generate.section.toc.level">1</xsl:param>
</xsl:stylesheet>
