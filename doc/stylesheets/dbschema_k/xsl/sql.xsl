<?xml version='1.0'?>
<!--
 * $Id: sql.xsl 4518 2008-07-28 15:39:28Z henningw $
 *
 * XSL converter script for SQL
 *
 * Copyright (C) 2001-2007 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version='1.0'
>

    <xsl:import href="common.xsl"/>

    <xsl:template match="/">
	<xsl:variable name="createfile" select="concat($dir, concat('/', concat($prefix, 'create.sql')))"/>
	<xsl:document href="{$createfile}" method="text" indent="no" omit-xml-declaration="yes">
	    <xsl:apply-templates select="/database[1]"/>
	</xsl:document>

    </xsl:template>

<!-- ################ DATABASE ################# -->

<!-- ################ /DATABASE ################# -->


<!-- ################ TABLE  ################# -->

    <xsl:template match="table">
	<xsl:variable name="table.name">
	    <xsl:call-template name="get-name"/>
	</xsl:variable>

	<!-- Create row in version table -->
	<xsl:apply-templates select="version"/>

	<xsl:text>CREATE TABLE </xsl:text>
	<xsl:call-template name="quotechar"/>
	<xsl:value-of select="$table.name"/>
	<xsl:call-template name="quotechar"/>
	<xsl:text> (&#x0A;</xsl:text>

	<!-- Process all columns -->
	<xsl:apply-templates select="column"/>

	<!-- Process all unique indexes -->
	<xsl:apply-templates select="index[child::unique]"/>

	<!-- Process all primary indexes -->
	<xsl:apply-templates select="index[child::primary]"/>

	<xsl:text>&#x0A;</xsl:text>

	<xsl:call-template name="table.close"/>

	<xsl:for-each select="index[count(child::unique)=0]">
	    <xsl:if test="not(child::primary)">
	        <xsl:call-template name="create_index"/>
	    </xsl:if>
	</xsl:for-each>
    </xsl:template>

<!-- ################ /TABLE ################  -->


<!-- ################ /VERSION ################  -->

	<xsl:template match="version">
	<xsl:text>INSERT INTO version (table_name, table_version) values ('</xsl:text>
	<xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::table"/>
	</xsl:call-template>
	<xsl:text>','</xsl:text>
	<xsl:value-of select="text()"/>
	<xsl:text>');&#x0A;</xsl:text>
	</xsl:template>

<!-- ################ /VERSION ################  -->


<!-- ################ INDEX (constraint) ################  -->

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
		<xsl:call-template name="get-index-name"/>
	</xsl:if>
	<xsl:if test="unique">
		<xsl:text> UNIQUE (</xsl:text>
		<xsl:apply-templates select="colref"/>
		<xsl:text>)</xsl:text>
	    <xsl:if test="not(position()=last())">
		<xsl:text>,</xsl:text>
		<xsl:text>&#x0A;</xsl:text>
	    </xsl:if>
	</xsl:if>
	<!-- PRIMARY KEY standalone definition -->
	<xsl:if test="primary">
	    <xsl:text> PRIMARY KEY </xsl:text>
	    <xsl:text> (</xsl:text>
	    <xsl:apply-templates select="colref"/>
	    <xsl:text>)</xsl:text>
	    <xsl:if test="not(position()=last())">
		<xsl:text>,</xsl:text>
		<xsl:text>&#x0A;</xsl:text>
	    </xsl:if>
	</xsl:if>
    </xsl:template>

<!-- ################ /INDEX (constraint) ################  -->

<!-- ################ INDEX (create) ################  -->

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
	<xsl:if test="not($index.name='')">
		<xsl:call-template name="get-index-name"/>
	</xsl:if>
	<xsl:text> ON </xsl:text>
	<xsl:value-of select="$table.name"/>
	<xsl:text> (</xsl:text>
	<xsl:apply-templates select="colref"/>
	<xsl:text>);&#x0A;</xsl:text>

	<xsl:if test="position()=last()">
	    <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
    </xsl:template>

<!-- ################ /INDEX (create) ################  -->


<!-- ################ COLUMN ################  -->

    <xsl:template match="column">
	<xsl:text>    </xsl:text>
	<xsl:call-template name="quotechar"/>
	<xsl:call-template name="get-name"/>
	<xsl:call-template name="quotechar"/>
	<xsl:text> </xsl:text>

	<xsl:call-template name="column.type"/>

	<xsl:choose>
	    <xsl:when test="default[@db=$db]">
		<xsl:text> DEFAULT </xsl:text>
		<xsl:choose>
		    <xsl:when test="default[@db=$db]/null">
			<xsl:text>NULL</xsl:text>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:value-of select="default[@db=$db]"/>
		    </xsl:otherwise>
		</xsl:choose>
	    </xsl:when>
	    <xsl:when test="default">
		<xsl:text> DEFAULT </xsl:text>
		<xsl:choose>
		    <xsl:when test="default/null">
			<xsl:text>NULL</xsl:text>
		    </xsl:when>
		    <xsl:when test="string(number(default))='NaN'"><!-- test for string value -->
			<xsl:text>'</xsl:text>
			<xsl:value-of select="default"/>
			<xsl:text>'</xsl:text>
		    </xsl:when>
		    <xsl:otherwise>
			<xsl:value-of select="default"/><!-- ommit the quotes for numbers -->
		    </xsl:otherwise>
		</xsl:choose>
	    </xsl:when>
	</xsl:choose>

	<xsl:variable name="null">
	    <xsl:call-template name="get-null"/>
	</xsl:variable>
	<xsl:if test="$null=0">
	    <xsl:text> NOT NULL</xsl:text>
	</xsl:if>

	<xsl:if test="not(position()=last())">
	    <xsl:text>,</xsl:text>
	    <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
    </xsl:template>

    <xsl:template name="column.type">
	<!-- FIXME -->
	<xsl:call-template name="get-type"/>
	<xsl:call-template name="column.size"/>
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

<!-- ################ /COLUMN ################  -->


<!-- ################ COLREF ################  -->

    <xsl:template match="colref">
	<xsl:call-template name="quotechar"/>
	<xsl:call-template name="get-column-name">
	    <xsl:with-param name="select" select="@linkend"/>
	</xsl:call-template>
	<xsl:call-template name="quotechar"/>
	<xsl:if test="not(position()=last())">
	    <xsl:text>, </xsl:text>
	</xsl:if>
    </xsl:template>

<!-- ################ /COLREF ################  -->

</xsl:stylesheet>
