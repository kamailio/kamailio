<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns:xi="http://www.w3.org/2001/XInclude"
				>

  <xsl:import href="sql.xsl"/>

  <xsl:template match="database">
	<xsl:variable name="database.name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>
	<!-- Create all tables -->
	<xsl:apply-templates select="user"/>
	<xsl:text>&#x0A;</xsl:text>
	<xsl:apply-templates select="table"/>
	<xsl:text>&#x0A;</xsl:text>
	<xsl:apply-templates select="user" mode="grant"/>
  </xsl:template>

  <xsl:template match="database" mode="drop">
	<xsl:variable name="database.name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>
	<xsl:apply-templates mode="drop"/>
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
		<xsl:text>SMALLINT</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='short'">
		<xsl:text>SMALLINT</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='int'">
		<xsl:text>INTEGER</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='long'">
		<xsl:text>BIGINT</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='datetime'">
		<xsl:text>TIMESTAMP</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='double'">
		<xsl:text>DOUBLE PRECISION</xsl:text>
	  </xsl:when>
	  <xsl:when test="$type='float'">
		<xsl:text>REAL</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='string'">
		<xsl:text>VARCHAR</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:when test="$type='binary'">
		<xsl:text>BYTEA</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:call-template name="type-error"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <xsl:template name="column.trailing">
	<xsl:variable name="column.type">
	  <xsl:call-template name="get-type"/>
	</xsl:variable>

	<xsl:if test="$column.type='datetime'">
	  <xsl:text> WITHOUT TIME ZONE</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template match="table">
	<xsl:variable name="table.name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>

	<xsl:text>CREATE TABLE </xsl:text>
	<xsl:value-of select="$table.name"/>
	<xsl:text> (&#x0A;</xsl:text>

	<!-- Process all columns -->
	<xsl:apply-templates select="column"/>

	<!-- Process all indexes -->
	<xsl:apply-templates select="index[child::unique]"/>

	<xsl:text>&#x0A;</xsl:text>

	<xsl:call-template name="table.close"/>

	<xsl:for-each select="index[count(child::unique)=0]">
	  <xsl:call-template name="create_index"/>
	</xsl:for-each>
  </xsl:template>

  <xsl:template match="index">
	<xsl:variable name="index.name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>

	<xsl:if test="position()=1">
	  <xsl:text>,&#x0A;</xsl:text>
	</xsl:if>
	<xsl:text>    </xsl:text>
	<xsl:if test="not($index.name='')">
	  <xsl:text>CONSTRAINT </xsl:text>
	  <xsl:value-of select="concat($index.name, ' ')"/>
	</xsl:if>
	<xsl:text>UNIQUE (</xsl:text>
	<xsl:apply-templates select="colref"/>
	<xsl:text>)</xsl:text>
	
	<xsl:if test="not(position()=last())">
	  <xsl:text>,</xsl:text>
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template name="create_index">
	<xsl:variable name="index.name">
	  <xsl:call-template name="get-name"/>
	</xsl:variable>
	<xsl:variable name="table.name">
	  <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::table"/>
	  </xsl:call-template>
	</xsl:variable>

	<xsl:text>CREATE </xsl:text>
	<xsl:if test="unique">
	  <xsl:text>UNIQUE </xsl:text>
	</xsl:if>
	<xsl:text>INDEX </xsl:text>
	<xsl:value-of select="$index.name"/>
	<xsl:text> ON </xsl:text>
	<xsl:value-of select="$table.name"/>
	<xsl:text> (</xsl:text>
	<xsl:apply-templates select="colref"/>
	<xsl:text>);&#x0A;</xsl:text>

	<xsl:if test="position()=last()">
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template name="get-userid">
	<xsl:param name="select" select="."/>
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
  </xsl:template>

  <xsl:template name="get-privileges">
	<xsl:param name="select" select="."/>
	<xsl:choose>
	  <!-- override test -->
	  <xsl:when test="count($select/privileges[@db=$db])='1'">
		<xsl:value-of select="normalize-space($select/privileges[@db=$db])"/>
	  </xsl:when>
	  <!-- No override, use the standard name -->
	  <xsl:otherwise>
		<xsl:value-of select="normalize-space($select/privileges)"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <!-- User management -->

  <xsl:template match="user">
	<xsl:variable name="database.name">
	  <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::database"/>
	  </xsl:call-template>
	</xsl:variable>
	
	<xsl:text>CREATE USER </xsl:text>
	<xsl:call-template name="get-userid"/>
	<xsl:choose>
	  <xsl:when test="count(password[@db=$db])='1'">
		<xsl:text> PASSWORD '</xsl:text>
		<xsl:value-of select="normalize-space(password[@db=$db])"/>
		<xsl:text>'</xsl:text>
	  </xsl:when>
	  <xsl:when test="count(password)='1'">
		<xsl:text> PASSWORD '</xsl:text>
		<xsl:value-of select="normalize-space(password)"/>
		<xsl:text>'</xsl:text>
	  </xsl:when>
	</xsl:choose>
	<xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="user" mode="drop">
	<xsl:variable name="database.name">
	  <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::database"/>
	  </xsl:call-template>
	</xsl:variable>
	
	<xsl:text>DROP USER </xsl:text>
	<xsl:call-template name="get-userid"/>
	<xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

  <xsl:template match="user" mode="grant">
	<xsl:text>GRANT </xsl:text>
	<xsl:call-template name="get-privileges"/>
	<xsl:text> ON </xsl:text>
	<xsl:for-each select="parent::database/table">
	  <xsl:call-template name="get-name"/>
	  <xsl:if test="not(position()=last())">
		<xsl:text>,</xsl:text>
	  </xsl:if>
	</xsl:for-each>
	<xsl:text> TO </xsl:text>
	<xsl:call-template name="get-userid"/>
	<xsl:text>;&#x0A;</xsl:text>
  </xsl:template>

</xsl:stylesheet>
