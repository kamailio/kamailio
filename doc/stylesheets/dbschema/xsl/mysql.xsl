<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
				>

  <xsl:import href="sql.xsl"/>

  <xsl:template match="database" mode="drop">
	<xsl:apply-templates mode="drop"/>
  </xsl:template>
  
  <xsl:template name="table.close">
	<xsl:text>)</xsl:text>
	<xsl:if test="type[@db=$db]">
	  <xsl:text> Type=</xsl:text>
	  <xsl:value-of select="normalize-space(type[@db=$db])"/>
	</xsl:if>
	<xsl:text>;&#x0A;&#x0A;</xsl:text>	
  </xsl:template>

  <xsl:template name="column.type">
	<xsl:variable name="type">
	  <xsl:call-template name="get-type"/>
	</xsl:variable>

	<xsl:choose>
	  <xsl:when test="type[@db=$db]">
		<xsl:value-of select="normalize-space(type[@db=$db])"/>
	  </xsl:when>
	  <xsl:when test="$type='char'">
		<xsl:text>TINYINT</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='short'">
		<xsl:text>SMALLINT</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='int'">
		<xsl:text>INT</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='long'">
		<xsl:text>BIGINT</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='datetime'">
		<xsl:text>DATETIME</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='double'">
		<xsl:text>DOUBLE</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='float'">
		<xsl:text>FLOAT</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='string'">
		<xsl:text>VARCHAR</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='binary'">
		<xsl:text>BLOB</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:call-template name="type-error"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <xsl:template name="column.trailing">
	<xsl:variable name="signed">
	  <xsl:call-template name="get-sign"/>
	</xsl:variable>
	
	<xsl:if test="$signed = 0">
	  <xsl:text> UNSIGNED</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template match="index">
	<xsl:variable name="index.name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>

	<xsl:if test="position()=1">
	  <xsl:text>,&#x0A;</xsl:text>
	</xsl:if>
	<xsl:text>    </xsl:text>
	<xsl:if test="unique">
	  <xsl:text>UNIQUE </xsl:text>
	</xsl:if>
	<xsl:text>KEY </xsl:text>
	<xsl:if test="not($index.name='')">
	  <xsl:value-of select="concat($index.name, ' ')"/>
	</xsl:if>
	<xsl:text>(</xsl:text>
	<xsl:apply-templates select="colref"/>
	<xsl:text>)</xsl:text>
	<xsl:if test="not(position()=last())">
	  <xsl:text>,</xsl:text>
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template name="get-userid">
	<xsl:param name="select" select="."/>
	<xsl:param name="host" select="/.."/>
	<xsl:text>'</xsl:text>
	<xsl:choose>
	  <!-- override test -->
	  <xsl:when test="count($select/username[@db=$db])='1'">
		<xsl:value-of select="normalize-space($select/username[@db=$db])"/>
	  </xsl:when>
	  <!-- No override, use the standard name -->
	  <xsl:otherwise>
		<xsl:value-of select="normalize-space($select/username)"/>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>'</xsl:text>

	<xsl:text>@'</xsl:text>
	<xsl:choose>
	  <xsl:when test="count($host)='1'">
		<xsl:value-of select="normalize-space($host)"/>
	  </xsl:when>
	  <xsl:when test="count(host[@db=$db])='1'">
		<xsl:value-of select="normalize-space(host[@db=$db])"/>
	  </xsl:when>
	  <xsl:when test="count(host)='1'">
		<xsl:value-of select="normalize-space(host)"/>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:text>%</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:text>'</xsl:text>
  </xsl:template>

  <!-- ################ ROW ################  -->

  <!-- override common template for ROW. Create INSERT statements 
       with IGNORE keyword
    -->
  <xsl:template match="row">
	<xsl:if test="@vendor-controlled[1]">
	  <xsl:text>DELETE FROM </xsl:text>	    
	  <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::table"/>
	  </xsl:call-template>
	  <xsl:text> WHERE </xsl:text>	    
	  <xsl:call-template name="row-identification"/>
	  <xsl:text>;&#x0A;</xsl:text>	    
	</xsl:if>

	<xsl:text>INSERT IGNORE INTO </xsl:text>
	<xsl:call-template name="get-name">
	  <xsl:with-param name="select" select="parent::table"/>
	</xsl:call-template>
	<xsl:text> (</xsl:text>
	<xsl:apply-templates select="value" mode="colname"/>
	<xsl:text>) VALUES (</xsl:text>
	<xsl:apply-templates select="value"/>
	<xsl:text>);&#x0A;</xsl:text>
	<xsl:if test="position()=last()">
	  <xsl:text>&#x0A;</xsl:text>	    
	</xsl:if>
  </xsl:template>

  <!-- ################ /ROW ################  -->

</xsl:stylesheet>
