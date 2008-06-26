<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
				>

  <xsl:import href="common.xsl"/>

  <xsl:template match="/">
	<xsl:variable name="createfile" select="concat($dir, concat('/', concat($prefix, 'create.sql')))"/>
	<xsl:document href="{$createfile}" method="text" indent="no" omit-xml-declaration="yes">
	  <xsl:apply-templates select="/database[1]"/>
	  <!-- This is a hack to ensure that the file gets created when
	       nothing is written
	    -->
	  <xsl:text> </xsl:text>
	</xsl:document>

	<xsl:variable name="datafile" select="concat($dir, concat('/', concat($prefix, 'data.sql')))"/>
	<xsl:document href="{$datafile}" method="text" indent="no" omit-xml-declaration="yes">
	  <xsl:apply-templates mode="data" select="/database[1]"/>
	  <!-- This is a hack to ensure that the file gets created when
	       nothing is written
	    -->
	  <xsl:text> </xsl:text>
	</xsl:document>

	<xsl:variable name="dropfile" select="concat($dir, concat('/', concat($prefix, 'drop.sql')))"/>
	<xsl:document href="{$dropfile}" method="text" indent="no" omit-xml-declaration="yes">
	  <xsl:apply-templates mode="drop" select="/database[1]"/>
	  <!-- This is a hack to ensure that the file gets created when
	       nothing is written
	    -->
	  <xsl:text> </xsl:text>
	</xsl:document>
  </xsl:template>
  
  <!-- ################ DATABASE ################# -->

  <!-- ################ /DATABASE ################# -->

  
  <!-- ################ TABLE  ################# -->
  
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
	<xsl:apply-templates select="index"/>

	<xsl:text>&#x0A;</xsl:text>

	<xsl:call-template name="table.close"/>
  </xsl:template>

  <xsl:template match="table" mode="data">
	<!-- Process initial data --> 
	<xsl:apply-templates select="row"/>
  </xsl:template>

  <xsl:template name="table.close">
	<xsl:text>);&#x0A;&#x0A;</xsl:text>
  </xsl:template>

  <!-- ################ /TABLE ################  -->


  <!-- ################ COLUMN ################  -->

  <xsl:template match="column">
	<xsl:text>    </xsl:text>
	<xsl:call-template name="get-name"/>
	<xsl:text> </xsl:text>

	<xsl:call-template name="column.type"/>

	<xsl:variable name="null">
	  <xsl:call-template name="get-null"/>
	</xsl:variable>
	<xsl:if test="$null=0">
	  <xsl:text> NOT NULL</xsl:text>
	</xsl:if>

	<xsl:choose>
	  <xsl:when test="default[@db=$db]">
		<xsl:text> DEFAULT </xsl:text>
		<xsl:choose>
		  <xsl:when test="default[@db=$db]/null">
			<xsl:text>NULL</xsl:text>
		  </xsl:when>
		  <xsl:otherwise>
			<xsl:text>'</xsl:text>
			<xsl:value-of select="default[@db=$db]"/>
			<xsl:text>'</xsl:text>
		  </xsl:otherwise>
		</xsl:choose>
	  </xsl:when>
	  <xsl:when test="default">
		<xsl:text> DEFAULT </xsl:text>
		<xsl:choose>
		  <xsl:when test="default/null">
			<xsl:text>NULL</xsl:text>
		  </xsl:when>
		  <xsl:otherwise>
			<xsl:text>'</xsl:text>
			<xsl:value-of select="default"/>
			<xsl:text>'</xsl:text>
		  </xsl:otherwise>
		</xsl:choose>
	  </xsl:when>
	</xsl:choose>

	<xsl:if test="not(position()=last())">
	  <xsl:text>,</xsl:text>
	  <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template name="column.type">
	<!-- FIXME -->
	<xsl:call-template name="get-type"/>
	<xsl:call-template name="column.size"/>
	<xsl:call-template name="column.trailing"/>
  </xsl:template>

  <xsl:template name="column.size">
	<xsl:variable name="size">
	  <xsl:call-template name="get-size"/>
	</xsl:variable>

	<xsl:if test="not($size='')">
	  <xsl:text>(</xsl:text>
	  <xsl:value-of select="$size"/>
	  <xsl:text>)</xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template name="column.trailing"/>

  <!-- ################ /COLUMN ################  -->

  <!-- ################ INDEX ################  -->

  <xsl:template match="index">
	<!-- Translate unique indexes into SQL92 unique constraints -->
	<xsl:if test="unique">
	  <xsl:if test="position()=1">
		<xsl:text>,&#x0A;</xsl:text>
	  </xsl:if>
	  <xsl:text>    </xsl:text>

	  <xsl:call-template name="get-name"/>
	  <xsl:text> UNIQUE (</xsl:text>

	  <xsl:apply-templates match="colref"/>

	  <xsl:text>)</xsl:text>

	  <xsl:if test="not(position()=last())">
		<xsl:text>,</xsl:text>
		<xsl:text>&#x0A;</xsl:text>
	  </xsl:if>
	</xsl:if>
  </xsl:template>

  <!-- ################ /INDEX ################  -->

  <!-- ################ COLREF ################  -->

  <xsl:template match="colref">
	<xsl:call-template name="get-column-name">
	  <xsl:with-param name="select" select="@linkend"/>
	</xsl:call-template>
	<xsl:if test="not(position()=last())">
	  <xsl:text>, </xsl:text>
	</xsl:if>
  </xsl:template>

  <!-- ################ /COLREF ################  -->

  <!-- ################ ROW ################  -->

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

	<xsl:text>INSERT INTO </xsl:text>
	<xsl:call-template name="get-name">
	  <xsl:with-param name="select" select="parent::table"/>
	</xsl:call-template>
	<xsl:text> (</xsl:text>
	<xsl:apply-templates select="value" mode="colname"/>
	<xsl:text>) VALUES (</xsl:text>
	<xsl:apply-imports/>
	<xsl:text>);&#x0A;</xsl:text>
	<xsl:if test="position()=last()">
	  <xsl:text>&#x0A;</xsl:text>	    
	</xsl:if>
  </xsl:template>

  <xsl:template name="row-identification">
	<xsl:variable name="row-ident" select="parent::table/row-identificator"/>
	<xsl:variable name="row" select="."/>
	<xsl:variable name="columns" select="$row-ident/colref"/>


	<xsl:choose>
	  <xsl:when test="count($row-ident) = 0">
		<xsl:message terminate="yes">
		  <xsl:text>ERROR: row-identificator does not exists.</xsl:text>
		</xsl:message>
	  </xsl:when>
	  <xsl:when test="count($columns) = 0">
		<xsl:message terminate="yes">
		  <xsl:text>ERROR: row-identificator does not have any column.</xsl:text>
		</xsl:message>
	  </xsl:when>
	  <xsl:otherwise>


		<xsl:for-each select="$columns">
	      <xsl:variable name="col-id" select="@linkend"/>
	      
	      <!-- column name -->
	      <xsl:call-template name="get-column-name">
			<xsl:with-param name="select" select="$col-id"/>
	      </xsl:call-template>

	      <xsl:text>=</xsl:text>	    

	      <!-- value of column -->
	      <xsl:variable name="value" select="$row/value[@col=$col-id]"/>
	      <xsl:choose>
			<xsl:when test="count($value) = 0">
		      <xsl:message terminate="yes">
				<xsl:text>ERROR: Value of column with id '</xsl:text>
				<xsl:value-of select="$col-id"/>
				<xsl:text>' does not exist.</xsl:text>
		      </xsl:message>
			</xsl:when>
			<xsl:otherwise>
		      <xsl:text>'</xsl:text>
		      <xsl:value-of select="$value"/>
		      <xsl:text>'</xsl:text>
			</xsl:otherwise>
	      </xsl:choose>


	      <xsl:if test="not(position()=last())">
			<xsl:text> AND </xsl:text>
	      </xsl:if>
		</xsl:for-each>


	  </xsl:otherwise>
	</xsl:choose>
    
  </xsl:template>

  <!-- ################ /ROW ################  -->

  <!-- ################ VALUE ################  -->

  <xsl:template match="value">
	<xsl:choose>
	  <xsl:when test="null">
		<xsl:text>NULL</xsl:text>
	  </xsl:when>
	  <xsl:otherwise>
		<xsl:text>'</xsl:text>
		<xsl:value-of select="text()"/>
		<xsl:text>'</xsl:text>
	  </xsl:otherwise>
	</xsl:choose>
	<xsl:if test="not(position()=last())">
	  <xsl:text>, </xsl:text>
	</xsl:if>
  </xsl:template>

  <xsl:template match="value" mode="colname">
	<xsl:call-template name="get-column-name">
	  <xsl:with-param name="select" select="@col"/>
	</xsl:call-template>
	<xsl:if test="not(position()=last())">
	  <xsl:text>, </xsl:text>
	</xsl:if>
  </xsl:template>

  <!-- ################ /VALUE ################  -->

</xsl:stylesheet>
