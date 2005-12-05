<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
     version='1.0' xmlns:xi="http://www.w3.org/2001/XInclude">

    <xsl:param name="prefix"/>
    <xsl:param name="output"/>

    <xsl:template match="/">
	<xsl:document href="{$output}" method="text" indent="no" omit-xml-declaration="yes">
            <xsl:value-of select="concat($output, ': ')"/>
	    <xsl:apply-templates mode="subroot"/>
	</xsl:document>
    </xsl:template>

    <xsl:template name="get-prefix">
	<xsl:if test="contains($prefix, '/')">
	    <xsl:value-of select="concat(substring-before($prefix, '/'), '/')"/>
	    <xsl:call-template name="get-prefix">
		<xsl:with-param name="prefix" select="substring-after($prefix, '/')"/>
	    </xsl:call-template>
	</xsl:if>
    </xsl:template>
    
    <xsl:template match='xi:include' mode="subroot">
	<xsl:value-of select="concat($prefix, concat(@href, ' '))"/>
	<xsl:apply-templates select="document(@href)" mode="subroot">
	    <xsl:with-param name="prefix">
		<xsl:call-template name="get-prefix">
		    <xsl:with-param name="prefix" select="concat($prefix, @href)"/>
		</xsl:call-template>
	    </xsl:with-param>
	</xsl:apply-templates>
    </xsl:template>
    
    <xsl:template match='xi:include[@parse="text"]' mode="subroot">
	<xsl:value-of select="concat($prefix, concat(@href, ' '))"/>
    </xsl:template>
    
    <xsl:template match="graphic|imagedata|inlinemediaobject|textdata" mode="subroot">
	<xsl:value-of select="concat($prefix, concat(@fileref, ' '))"/>
    </xsl:template>
    
    <xsl:template match="text()|@*" mode="subroot"/>
</xsl:stylesheet>
