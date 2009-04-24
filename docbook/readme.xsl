<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

    <!-- Produce a single file when building parts of documentation -->
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>

    <!-- Import plain-text customization -->
    <xsl:import href="txt.xsl"/>

	<xsl:param name="section.autolabel">1</xsl:param>
	<xsl:param name="section.label.includes.component.label">1</xsl:param>
	<xsl:param name="generate.toc">book toc,title,figure,table,example</xsl:param>
</xsl:stylesheet>