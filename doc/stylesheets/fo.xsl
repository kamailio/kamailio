<?xml version='1.0'?>

<!-- Common settings for generating FO output -->
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl"/>
    <xsl:import href="common.xsl"/>

    <xsl:param name="paper.type">A4</xsl:param>
    <xsl:param name="double.sided">1</xsl:param>
    <xsl:param name="xep.extensions">1</xsl:param>
    <xsl:param name="insert.xref.page.number">yes</xsl:param>
    <xsl:param name="admon.graphics">1</xsl:param>
    <xsl:param name="ulink.footnotes">0</xsl:param>
</xsl:stylesheet>
