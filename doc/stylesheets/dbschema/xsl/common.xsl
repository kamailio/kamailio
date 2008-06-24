<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
				version='1.0'>
  
  <xsl:key name="column_id" match="column" use="@id|xml:id"/>

  <xsl:param name="prefix" select="_"/>
  <xsl:param name="dir" select="mm"/>
  <xsl:param name="db" select="_"/>

  <xsl:variable name="sign-prefix">unsigned </xsl:variable>
  
  <!-- Do not output text -->
  <xsl:template match="text()|@*"/>
  <xsl:template match="text()|@*" mode="drop"/>

  <!-- Return the name of the context element, first look for a database
       specific name, use the common name if no database-specific name
       is found.
    -->
  <xsl:template name="get-name">
	<xsl:param name="select" select="."/>
	<xsl:choose>
	  <!-- override test -->
	  <xsl:when test="count($select/name[@db=$db])='1'">
		<xsl:value-of select="normalize-space($select/name[@db=$db])"/>
	  </xsl:when>
	  <!-- No override, use the standard name -->
	  <xsl:otherwise>
		<xsl:value-of select="normalize-space($select/name)"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <xsl:template name="type-error">
	<xsl:message terminate="yes">
	  <xsl:text>ERROR: Table: </xsl:text>
	  <xsl:value-of select="normalize-space(parent::table/name)"/>
	  <xsl:text>, column: </xsl:text>
	  <xsl:value-of select="normalize-space(name)"/>
	  <xsl:text> - unsupported column type: </xsl:text>
	  <xsl:value-of select="normalize-space(type)"/>
	  <xsl:text>.</xsl:text>
	</xsl:message>
  </xsl:template>

  <!-- Process the root database element -->
  <xsl:template match="/">
	<!-- Process only the first database element, this is supposed to be
	     the root database element and having multiple database elements is
	     a bug
	  -->
	<xsl:apply-templates select="database[1]"/>
  </xsl:template>

  <!-- ################ DATABASE ################# -->

  <xsl:template match="database">

	<!-- Create all tables -->
	<xsl:apply-templates select="table"/>
	<xsl:apply-templates select="user"/>
  </xsl:template>

  <xsl:template match="database" mode="data">

	<!-- Insert initial data -->
	<xsl:apply-templates select="table" mode="data"/>
  </xsl:template>

  <!-- ################ /DATABASE ################# -->

  <!-- ################ TABLE ################# -->

  <xsl:template match="table">
	<!-- Process all columns -->
	<xsl:apply-templates select="column"/>

	<!-- Process all indexes -->
	<xsl:apply-templates select="index"/>

	<!-- Process initial data --> 
	<xsl:apply-templates select="row"/>
  </xsl:template>

  <!-- ################ /TABLE ################# -->

  <!-- ################ COLUMN ################# -->
  
  <xsl:template match="column"/>

  <xsl:template name="get-type-string">
	<xsl:param name="select" select="."/>
	<xsl:choose>
	  <xsl:when test="count($select/type[@db=$db])='1'">
		<xsl:value-of select="translate(normalize-space($select/type[@db=$db]),
							  'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')"/>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:value-of select="translate(normalize-space($select/type),
							  'ABCDEFGHIJKLMNOPQRSTUVWXYZ', 'abcdefghijklmnopqrstuvwxyz')"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <xsl:template name="get-type">
	<xsl:param name="select" select="."/>
	<xsl:variable name="type">
	  <xsl:call-template name="get-type-string">
		<xsl:with-param name="select" select="$select"/>
	  </xsl:call-template>
	</xsl:variable>
	<xsl:choose>
	  <xsl:when test="starts-with($type, $sign-prefix)">
		<xsl:value-of select="substring-after($type, $sign-prefix)"/>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:value-of select="$type"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>
  
  <xsl:template name="get-sign">
	<xsl:param name="select" select="."/>
	<xsl:variable name="type">
	  <xsl:call-template name="get-type-string">
		<xsl:with-param name="select" select="$select"/>
	  </xsl:call-template>
	</xsl:variable>
	<xsl:choose>
	  <xsl:when test="starts-with($type, $sign-prefix)">0</xsl:when>
	  <xsl:otherwise>1</xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <xsl:template name="get-null">
	<xsl:param name="select" select="."/>
	<xsl:choose>
	  <xsl:when test="count($select/null[@db=$db])='1'">1</xsl:when>
	  <xsl:when test="count($select/null)='1'">1</xsl:when>
	  <xsl:otherwise>0</xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <xsl:template name="get-size">
	<xsl:param name="select" select="."/>
	<xsl:choose>
	  <xsl:when test="count($select/size[@db=$db])='1'">
		<xsl:value-of select="normalize-space($select/size[@db=$db])"/>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:value-of select="normalize-space($select/size)"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>


  <!-- column ID to column name -->
  <xsl:template name="get-column-name">
	<xsl:param name="select" select="."/>

	<xsl:variable name="columns" select="key('column_id', $select)"/>
	<xsl:variable name="column" select="$columns[1]"/>
	<xsl:choose>
	  <xsl:when test="count($column) = 0">
		<xsl:message terminate="yes">
		  <xsl:text>ERROR: Column with id '</xsl:text>
		  <xsl:value-of select="$select"/>
		  <xsl:text>' does not exist.</xsl:text>
		</xsl:message>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:call-template name="get-name">
		  <xsl:with-param name="select" select="$column"/>
		</xsl:call-template>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>
  <!-- ################ /COLUMN ################# -->

  <!-- ################ INDEX ################# -->

  <xsl:template match="index">
	<xsl:apply-templates match="colref"/>
  </xsl:template>
  
  <!-- ################ /INDEX ################# -->

  <!-- ################ COLREF ################# -->

  <xsl:template match="colref"/>

  <xsl:template name="get-column">
	<xsl:param name="id" select="/.."/>	
	<xsl:variable name="columns" select="key('column_id', $id)"/>
	<xsl:variable name="column" select="$columns[1]"/>

	<xsl:choose>
	  <xsl:when test="count($column) = 0">
		<xsl:message terminate="yes">
		  <xsl:text>ERROR: Column with id '</xsl:text>
		  <xsl:value-of select="$id"/>
		  <xsl:text>' does not exist.</xsl:text>
		</xsl:message>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:copy-of select="$column"/>
	  </xsl:otherwise>
	</xsl:choose>
  </xsl:template>

  <!-- ################ /COLREF ################# -->

  <!-- ################ ROW ################# -->

  <xsl:template match="row">
	<xsl:apply-templates select="value"/>
  </xsl:template>

  <!-- ################ /ROW ################# -->

  <!-- ################ VALUE ################# -->

  <xsl:template match="value"/>

  <!-- ################ /VALUE ################# -->

  <!-- ################ USER ################# -->

  <xsl:template match="user"/>

  <!-- ################ /USER ################# -->
</xsl:stylesheet>
