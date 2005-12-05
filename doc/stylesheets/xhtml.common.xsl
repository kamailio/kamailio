<?xml version='1.0'?>

<!-- Common settings for generating XHTML output -->
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

   <!-- Stylesheets that override this one must import either onechunk.xsl
        or chunk.xsl
     -->
    
<!-- Stylesheets common for all transformations (xhtml, txt, fo) -->
    <xsl:import href="common.xsl"/>

    <xsl:output method="xml"/>
    
<!-- HTML Customization -->
    <xsl:param name="html.cleanup" select="1"/>
    <xsl:param name="html.stylesheet">ser.css</xsl:param>

<!-- Needed for strict XHTML -->
    <xsl:param name="css.decoration">0</xsl:param>
    <xsl:param name="html.longdesc">0</xsl:param>
    <xsl:param name="ulink.target"/>
    <xsl:param name="use.viewport">0</xsl:param>

<!-- Output format -->
    <xsl:param name="chunker.output.encoding">UTF-8</xsl:param>
    <xsl:param name="chunker.output.indent">yes</xsl:param>
    <xsl:param name="chunker.output.doctype-public">-//W3C//DTD XHTML 1.0 Strict//EN</xsl:param>
    <xsl:param name="chunker.output.doctype-system">http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd</xsl:param>
</xsl:stylesheet>
