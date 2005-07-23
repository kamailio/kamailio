<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns:xi="http://www.w3.org/2001/XInclude">
    <xsl:output method="text" indent="no" omit-xml-declaration="yes"/>

    <!-- Path prefix to add at the beginning of all dependencies -->
    <xsl:variable name="prefix"/>

    <!--
       This template gets pathname of one of the dependency files and
       generates path prefix for other files included from that dependency.
       This is used for .xml files included using xi:include, the function
       returns all components of the pathname but the filename. For example
       when called with "../modules/auth/doc/auth.xml" the template will return
       "../modules/auth/doc/". This will be then used as the prefix for files
       included from auth.xml, so if auth.xml includes params.xml then with the
       prefix it will be "../modules/auth/doc/params.xml"
    -->
    <xsl:template name="get-prefix">
        <xsl:if test="contains($prefix, '/')">
	    <xsl:value-of select="concat(substring-before($prefix, '/'), '/')"/>
	    <xsl:call-template name="get-prefix">
	        <xsl:with-param name="prefix" select="substring-after($prefix, '/')"/>
	    </xsl:call-template>
	</xsl:if>
    </xsl:template>

    <!--
        
    -->
    <xsl:template match='xi:include'>
	<xsl:value-of select="concat($prefix, concat(@href, ' '))"/>
	<xsl:apply-templates select="document(@href)">
	    <xsl:with-param name="prefix">
	    	<xsl:call-template name="get-prefix">
			<xsl:with-param name="prefix" select="concat($prefix, @href)"/>
		</xsl:call-template>
	    </xsl:with-param>
	</xsl:apply-templates>
    </xsl:template>

    <xsl:template match='xi:include[@parse="text"]'>
	<xsl:value-of select="concat($prefix, concat(@href, ' '))"/>
    </xsl:template>

    <xsl:template match="graphic|imagedata|inlinemediaobject|textdata">
        <xsl:value-of select="concat($prefix, concat(@fileref, ' '))"/>
    </xsl:template>
    
    <xsl:template match="text()|@*"/>
</xsl:stylesheet>
