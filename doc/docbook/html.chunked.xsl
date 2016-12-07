<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

    <!-- Generate chunked output when building all the documentation  -->
    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/>

    <!-- Commmon XHTML customization -->
    <xsl:import href="html.common.xsl"/>

<!-- Chunking -->
    <xsl:param name="use.id.as.filename">yes</xsl:param>
</xsl:stylesheet>
