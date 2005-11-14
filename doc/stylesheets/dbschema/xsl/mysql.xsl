<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
                xmlns:db="http://iptel.org/dbschema/mysql"
>

    <xsl:import href="sql.xsl"/>

    <xsl:template match="database">
	<xsl:variable name="database.name">
	    <xsl:call-template name="get-name"/>
	</xsl:variable>

	<xsl:text>CREATE DATABASE </xsl:text>
	<xsl:value-of select="$database.name"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:text>USE </xsl:text>
	<xsl:value-of select="$database.name"/>
	<xsl:text>;&#x0A;&#x0A;</xsl:text>
	<xsl:apply-imports/>
    </xsl:template>

    <xsl:template match="database" mode="drop">
	<xsl:variable name="database.name">
	    <xsl:call-template name="get-name"/>
	</xsl:variable>

	<xsl:text>DROP DATABASE </xsl:text>
	<xsl:value-of select="$database.name"/>
	<xsl:text>;&#x0A;</xsl:text>

	<xsl:apply-templates mode="drop"/>
    </xsl:template>
    
    <xsl:template name="table.close">
	<xsl:text>)</xsl:text>
	<xsl:if test="db:type">
	    <xsl:text> Type=</xsl:text>
	    <xsl:value-of select="normalize-space(db:type)"/>
	</xsl:if>
	<xsl:text>;&#x0A;&#x0A;</xsl:text>	
    </xsl:template>

    <xsl:template name="column.type">
	<xsl:variable name="type">
	    <xsl:call-template name="get-type"/>
	</xsl:variable>

	<xsl:choose>
	    <xsl:when test="db:type">
		<xsl:value-of select="normalize-space(db:type)"/>
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

    <xsl:template match="user">
	<xsl:text>GRANT ALL PRIVILEGES ON </xsl:text>	
	<xsl:text>FLUSH PRIVILEGES;</xsl:text>
    </xsl:template>


    <xsl:template name="get-userid">
	<xsl:param name="select" select="."/>
	<xsl:param name="host" select="/.."/>
	<xsl:text>'</xsl:text>
	<xsl:choose>
	    <!-- override test -->
	    <xsl:when test="count($select/db:username)='1'">
		<xsl:value-of select="normalize-space($select/db:username)"/>
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
	    <xsl:when test="count(db:host)='1'">
		<xsl:value-of select="normalize-space(db:host)"/>
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

    <xsl:template match="user" mode="drop">
	<xsl:variable name="database.name">
	    <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::database"/>
	    </xsl:call-template>
	</xsl:variable>
	
	<xsl:text>REVOKE ALL PRIVILEGES ON </xsl:text>
	<xsl:value-of select="$database.name"/>
	<xsl:text>.* FROM </xsl:text>

	<xsl:call-template name="get-userid"/>

	<xsl:text>;&#x0A;</xsl:text>

	<xsl:text>DROP USER </xsl:text>
	<xsl:call-template name="get-userid"/>
	<xsl:text>;&#x0A;</xsl:text>

	<!-- No host tag present means connect from anywhere so we need to
	delete @localhost account as well -->
	
	<xsl:choose>
	    <xsl:when test="count(db:host)='1'"/>
	    <xsl:when test="count(host)='1'"/>
	    <xsl:otherwise>
		<xsl:text>REVOKE ALL PRIVILEGES ON </xsl:text>
		<xsl:value-of select="$database.name"/>
		<xsl:text>.* FROM </xsl:text>
		<xsl:call-template name="get-userid">
		    <xsl:with-param name="host">localhost</xsl:with-param>
		</xsl:call-template>
		<xsl:text>;&#x0A;</xsl:text>
		<xsl:text>DROP USER </xsl:text>
		<xsl:call-template name="get-userid">
		    <xsl:with-param name="host">localhost</xsl:with-param>
		</xsl:call-template>
		<xsl:text>;&#x0A;</xsl:text>
	    </xsl:otherwise>
	</xsl:choose>

	<xsl:text>FLUSH PRIVILEGES;</xsl:text>
	<xsl:text>&#x0A;</xsl:text>
    </xsl:template>


    <xsl:template match="user">
	<xsl:variable name="database.name">
	    <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::database"/>
	    </xsl:call-template>
	</xsl:variable>
	
	<xsl:text>GRANT </xsl:text>
	<xsl:choose>
	    <xsl:when test="count(db:privileges)='1'">
		<xsl:value-of select="normalize-space(db:privileges)"/>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="normalize-space(privileges)"/>
	    </xsl:otherwise>
	</xsl:choose>
	<xsl:text> ON </xsl:text>
	<xsl:value-of select="$database.name"/>
	<xsl:text>.* TO </xsl:text>
	<xsl:call-template name="get-userid"/>
	<xsl:choose>
	    <xsl:when test="count(db:password)='1'">
		<xsl:text> IDENTIFIED BY '</xsl:text>
		<xsl:value-of select="normalize-space(db:password)"/>
		<xsl:text>'</xsl:text>
	    </xsl:when>
	    <xsl:when test="count(password)='1'">
		<xsl:text> IDENTIFIED BY '</xsl:text>
		<xsl:value-of select="normalize-space(password)"/>
		<xsl:text>'</xsl:text>
	    </xsl:when>
	</xsl:choose>
	<xsl:text>;&#x0A;</xsl:text>

	<!-- No host tag present means connect from anywhere so we need to
	delete @localhost account as well -->
	
	<xsl:choose>
	    <xsl:when test="count(db:host)='1'"/>
	    <xsl:when test="count(host)='1'"/>
	    <xsl:otherwise>
		<xsl:text>GRANT </xsl:text>
		<xsl:choose>
		    <xsl:when test="count(db:privileges)='1'">
			<xsl:value-of select="normalize-space(db:privileges)"/>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:value-of select="normalize-space(privileges)"/>
		    </xsl:otherwise>
		</xsl:choose>
		<xsl:text> ON </xsl:text>
		<xsl:value-of select="$database.name"/>
		<xsl:text>.* TO </xsl:text>
		<xsl:call-template name="get-userid">
		    <xsl:with-param name="host">localhost</xsl:with-param>
		</xsl:call-template>
		<xsl:choose>
		    <xsl:when test="count(db:password)='1'">
			<xsl:text> IDENTIFIED BY '</xsl:text>
			<xsl:value-of select="normalize-space(db:password)"/>
			<xsl:text>'</xsl:text>
		    </xsl:when>
		    <xsl:when test="count(password)='1'">
			<xsl:text> IDENTIFIED BY '</xsl:text>
			<xsl:value-of select="normalize-space(password)"/>
			<xsl:text>'</xsl:text>
		    </xsl:when>
		</xsl:choose>
		<xsl:text>;&#x0A;</xsl:text>
	    </xsl:otherwise>
	</xsl:choose>

	<xsl:text>FLUSH PRIVILEGES;</xsl:text>
	<xsl:text>&#x0A;</xsl:text>
    </xsl:template>


</xsl:stylesheet>
