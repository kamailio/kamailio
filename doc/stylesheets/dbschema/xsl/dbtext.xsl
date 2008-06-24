<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns:xi="http://www.w3.org/2001/XInclude">

  <xsl:import href="common.xsl"/>
  <xsl:output method="text" indent="no" omit-xml-declaration="yes"/>

  <!-- Create the file for the table in dbtext subdirectory -->
  <xsl:template match="table">
	<xsl:variable name="name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>
	
	<xsl:variable name="path" select="concat($dir, concat('/', concat($prefix, $name)))"/>
	<xsl:document 
	   href="{$path}"
	   method="text"
	   indent="no"
	   omit-xml-declaration="yes">
	  <xsl:apply-imports/>
	</xsl:document>
  </xsl:template>

  <!-- Create column definitions -->
  <xsl:template match="column">
	<xsl:variable name="type">
	  <xsl:call-template name="get-type"/>
	</xsl:variable>

	<xsl:variable name="null">
	  <xsl:call-template name="get-null"/>
	</xsl:variable>

	<xsl:call-template name="get-name"/>
	<xsl:text>(</xsl:text>
	<xsl:choose>
	  <xsl:when test="type[@db=$db]">
		<xsl:value-of select="normalize-space(type[@db=$db])"/>
	  </xsl:when>
	  <xsl:when test="$type='char' or 
		              $type='short' or 
                      $type='int' or
	                  $type='long' or 
                      $type='datetime'">
		<xsl:text>int</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='float' or 
		              $type='double'">
		<xsl:text>double</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='string' or 
                      $type='binary'">
		<xsl:text>str</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:call-template name="type-error"/>
	  </xsl:otherwise>
	</xsl:choose>

	<xsl:if test="$null=1">
	  <xsl:text>,null</xsl:text>
	</xsl:if>
	<xsl:text>) </xsl:text>
	<xsl:if test="position()=last()">
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
  </xsl:template>

  <!-- Escape all : occurrences -->
  <xsl:template name="escape">
	<xsl:param name="value"/>
	<xsl:choose>
	  <xsl:when test="contains($value, ':')">
		<xsl:value-of select="concat(substring-before($value, ':'), '\:')"/>
		<xsl:call-template name="escape">
		  <xsl:with-param name="value" select="substring-after($value, ':')"/>
		</xsl:call-template>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:value-of select="$value"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <!-- Process initial data -->
  <xsl:template match="row">
	<!-- store the actual row so that it can be used later 
	     with another context node 
	  -->
	<xsl:variable name="row" select="."/>

	<!-- Walk through all the columns of the table and lookup
	     corresponding values based on id-col match from the
         current row, use the default value of the column if
	     there is no value with matching col attribute in the
         current row
	  -->
	<xsl:for-each select="parent::table/column">
	  <xsl:variable name="id" select="@id"/>
	  <xsl:call-template name="escape">
		<xsl:with-param name="value">
		  <xsl:choose>
			<!-- If we have db-specific value, use it -->
			<xsl:when test="$row/value[@col=$id and @db=$db]">
			  <xsl:value-of select="normalize-space($row/value[@col=$id and @db=$db])"/>
			</xsl:when>
			<!-- No db-specific value, try generic -->
			<xsl:when test="$row/value[@col=$id]">
			  <xsl:value-of select="normalize-space($row/value[@col=$id])"/>
			</xsl:when>
			<!-- No value at all, try db-specific default value for the column -->
			<xsl:when test="default[@db=$db]">
			  <xsl:value-of select="normalize-space(default[@db=$db])"/>
			</xsl:when>
			<!-- Try generic default value for the column -->
			<xsl:when test="default">
			  <xsl:value-of select="normalize-space(default)"/>
			</xsl:when>
			<!-- No value and no default value for the column - ouch -->
			<xsl:otherwise>
			  <xsl:message terminate="yes">
				<xsl:text>ERROR: Value for column </xsl:text>
				<xsl:value-of select="normalize-space(name)"/>
				<xsl:text> in table </xsl:text>
				<xsl:value-of select="normalize-space(parent::table/name)"/>
				<xsl:text> was not provided and the column has no default value.</xsl:text>
			  </xsl:message>
			</xsl:otherwise>
		  </xsl:choose>
		</xsl:with-param>
	  </xsl:call-template>
	  
	  <xsl:if test="not(position()=last())">
		<xsl:text>:</xsl:text>
	  </xsl:if>
	</xsl:for-each>

	<xsl:apply-imports/>
	<xsl:text>&#x0A;</xsl:text>
  </xsl:template>

  <!-- Make sure all values reference existing columns -->
  <xsl:template match="value">
	<xsl:variable name="column">
	  <xsl:call-template name="get-column">
		<xsl:with-param name="id" select="@col"/>
	  </xsl:call-template>
	</xsl:variable>
  </xsl:template>
</xsl:stylesheet>
