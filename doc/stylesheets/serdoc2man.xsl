<?xml version='1.0'?>
<!-- vim: sw=2 sta et
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
                xmlns:serdoc="http://sip-router.org/xml/serdoc"
                xmlns="http://docbook2x.sourceforge.net/xmlns/Man-XML"
                exclude-result-prefixes="doc"
                version='1.0'
                xml:lang="en">
                
<!-- Don't include stuff with role="admin-guide" -->
<xsl:template match="* [@role='admin-guide']" />

<!-- Get the docbook2man XSLT -->
<xsl:include href="http://docbook2x.sourceforge.net/latest/xslt/man/docbook.xsl" />


<xsl:template match="serdoc:todo">
  <para>
    <xsl:text>TODO</xsl:text>
  </para>
</xsl:template>

<xsl:template match="serdoc:link">
  <xsl:call-template name="inline-bold" />
</xsl:template>

<xsl:template match="serdoc:func">
  <xsl:call-template name="inline-bold-monospace"/>
  <xsl:text>()</xsl:text>
</xsl:template>

<xsl:template match="serdoc:module">
  <xsl:call-template name="inline-bold-monospace"/>
  <xsl:text>(7)</xsl:text>
</xsl:template>

<xsl:template match="serdoc:modparam">
  <xsl:call-template name="inline-bold-monospace"/>
</xsl:template>

<xsl:template match="serdoc:coreparam">
  <xsl:call-template name="inline-bold-monospace"/>
</xsl:template>

<xsl:template match="serdoc:field">
  <xsl:call-template name="inline-bold-monospace"/>
</xsl:template>

<xsl:template match="serdoc:bin">
  <xsl:call-template name="inline-bold-monospace"/>
  <xsl:text>(1)</xsl:text>
</xsl:template>

<xsl:template match="serdoc:sbin">
  <xsl:call-template name="inline-bold-monospace"/>
  <xsl:text>(8)</xsl:text>
</xsl:template>

<xsl:template match="serdoc:file">
  <xsl:call-template name="inline-bold-monospace"/>
  <xsl:text>(5)</xsl:text>
</xsl:template>

<xsl:template match="serdoc:prototype">
  <xsl:call-template name="inline-bold-monospace"/>
</xsl:template>

<xsl:template match="serdoc:paraminfo">
  <para>
    <xsl:apply-templates />
  </para>
</xsl:template>
  
<xsl:template match="serdoc:paramtype">
  <br />
  <xsl:text>Type: </xsl:text>
  <xsl:apply-templates />
</xsl:template>

<xsl:template match="serdoc:paramdefault">
  <br />
  <xsl:text>Default: </xsl:text>
  <xsl:apply-templates />
</xsl:template>

<xsl:template match="serdoc:fieldinfo">
  <para>
    <xsl:apply-templates />
  </para>
</xsl:template>

<xsl:template match="serdoc:fieldsql">
  <br />
  <xsl:text>Type: </xsl:text>
  <xsl:call-template name="inline-monospace" />
</xsl:template>



<xsl:template match="optional">
  <xsl:text>[</xsl:text>
  <xsl:apply-templates />
  <xsl:text>]</xsl:text>
</xsl:template>



</xsl:stylesheet>
