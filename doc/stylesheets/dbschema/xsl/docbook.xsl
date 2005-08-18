<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns:db="http://iptel.org/dbschema/docbook"
>
    <xsl:import href="common.xsl"/>

    <xsl:template match="/">
	<xsl:variable name="filename" select="concat($prefix, concat('/', 'dbschema.xml'))"/>
	<xsl:document href="{$filename}" method="xml" indent="yes"
	    omit-xml-declaration="no">
	    <xsl:element name="section">
		<xsl:element name="title">
		    <xsl:call-template name="get-name">
			<xsl:with-param name="select" select="database[1]"/>
		    </xsl:call-template>
		</xsl:element>
		<xsl:apply-templates select="/database[1]"/>
	    </xsl:element>
	</xsl:document>
    </xsl:template>

</xsl:stylesheet>
