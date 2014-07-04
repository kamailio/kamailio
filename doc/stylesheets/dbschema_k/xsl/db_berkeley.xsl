<?xml version='1.0'?>
<!--
 * $Id: db_berkeley.xsl 4518 2008-07-28 15:39:28Z henningw $
 *
 * XSL converter script for db_berkeley
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
                xmlns:xi="http://www.w3.org/2001/XInclude">

    <xsl:import href="common.xsl"/>
    <xsl:output method="text" indent="no" omit-xml-declaration="yes"/>

    <!-- Create the file for the table in db_berkeley subdirectory -->
    <xsl:template match="table">
	<xsl:variable name="name">
		<xsl:call-template name="get-name"/>
	</xsl:variable>
	
	<xsl:variable name="path" select="concat($dir, concat('/', concat($prefix, $name)))"/>
	<xsl:document href="{$path}" method="text" indent="no" omit-xml-declaration="yes">
		<xsl:text>METADATA_COLUMNS&#x0A;</xsl:text>
		<xsl:apply-imports/>

		<!-- Process all indexes -->
		<xsl:text>METADATA_KEY&#x0A;</xsl:text>
		
		<!-- natural indexes -->
		<xsl:for-each select="column">
			<xsl:if test="natural">
				<xsl:value-of select="position() - 1"/>
				<xsl:if test="not(position()=last())">
			   	<xsl:text> </xsl:text>
				</xsl:if>
			</xsl:if>
		</xsl:for-each>
		<xsl:text>&#x0A;</xsl:text>

	<!--
		<xsl:for-each select="index">
	        <xsl:call-template name="create_index"/>
		</xsl:for-each>
		<xsl:text>&#x0A;</xsl:text>
	-->

		<xsl:text>METADATA_READONLY&#x0A;</xsl:text>
		<xsl:text>0&#x0A;</xsl:text>

		<xsl:text>METADATA_LOGFLAGS&#x0A;</xsl:text>
		<xsl:text>0&#x0A;</xsl:text>

		<!-- default values -->
		<xsl:text>METADATA_DEFAULTS&#x0A;</xsl:text>
		<xsl:for-each select="column">
		<xsl:choose>
		    <xsl:when test="default[@db=$db]">
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
			<xsl:choose>
			    <xsl:when test="default/null">
				<xsl:text>NULL</xsl:text>
		    	</xsl:when>
		    	<xsl:when test= "string(number(default))='NaN'"><!-- test for string value -->
				<xsl:text>'</xsl:text>
				<xsl:value-of select="default"/>
				<xsl:text>'</xsl:text>
		    	</xsl:when>
		    	<xsl:otherwise>
				<xsl:value-of select="default"/><!-- ommit the quotes for numbers -->
		    	</xsl:otherwise>
			</xsl:choose>
	    	</xsl:when>
			<xsl:otherwise>
				<xsl:text>NIL</xsl:text><!-- no value specified -->
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if test="not(position()=last())">
			<xsl:text>|</xsl:text>
		</xsl:if>
		</xsl:for-each>
		<xsl:text>&#x0A;</xsl:text>

		<!-- Insert version data -->
		 <xsl:apply-templates select="version"/> 
		<!-- this is not exactly what we want for db_berkeley, as the version data gets
		     appended to the actual table file, and no to the 'version' table.
		     But its not possible (at least with XSL 1.0, AFAIK) to append data to a
		     file. So it's much more easier to do this in the Makefile -->
	</xsl:document>
    </xsl:template>

	<!-- version data template -->
	<xsl:template match="version">
	<xsl:call-template name="get-name">
	    <xsl:with-param name="select" select="parent::table"/>
	</xsl:call-template>
	<xsl:text>|&#x0A;</xsl:text>
	<xsl:call-template name="get-name">
	    <xsl:with-param name="select" select="parent::table"/>
	</xsl:call-template>
	<xsl:text>|</xsl:text>
	<xsl:value-of select="text()"/>
	<xsl:text>&#x0A;</xsl:text>
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
						$type='unsigned int' or
						$type='long'">
		<xsl:text>int</xsl:text>
		</xsl:when>
		<xsl:when test="$type='datetime'">
			<xsl:text>datetime</xsl:text>
	    </xsl:when>
	    <xsl:when test="$type='float' or 
						$type='double'">
		<xsl:text>double</xsl:text>
	    </xsl:when>
	    <xsl:when test="$type='string' or
						$type='text' or
						$type='binary' or 
                        $type='largetext' or
						$type='largebinary'">
		<xsl:text>str</xsl:text>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:call-template name="type-error"/>
	    </xsl:otherwise>
	</xsl:choose>

<!-- Support berkeley db NULL values? -->
<!--	<xsl:if test="$null=1">
	    <xsl:text>,null</xsl:text>
	</xsl:if>-->
	<xsl:text>)</xsl:text>
	<xsl:if test="not(position()=last())">
	    <xsl:text> </xsl:text>
	</xsl:if>
	<xsl:if test="position()=last()">
	    <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
    </xsl:template>

    <!-- Escape all | occurrences -->
    <xsl:template name="escape">
	<xsl:param name="value"/>
	<xsl:choose>
	    <xsl:when test="contains($value, '|')">
		<xsl:value-of select="concat(substring-before($value, '|'), '\|')"/>
		<xsl:call-template name="escape">
		    <xsl:with-param name="value" select="substring-after($value, '|')"/>
		</xsl:call-template>
	    </xsl:when>
	    <xsl:otherwise>
		<xsl:value-of select="$value"/>
	    </xsl:otherwise>
	</xsl:choose>
    </xsl:template>

  <!-- create the non primary indexes -->
	<xsl:template name="create_index">
	<!-- save the parent table name -->
	<xsl:variable name="table.name">
		<xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::table"/>
		</xsl:call-template>
	</xsl:variable>
	<!-- loop over all colrefs -->
	<xsl:for-each select="colref">
		<xsl:variable name="link">
			<xsl:call-template name="get-column-name">
			<xsl:with-param name="select" select="@linkend"/>
			</xsl:call-template>
		</xsl:variable>
		<!-- and search the right position of the link -->
		<xsl:for-each select="//table[name = $table.name]/column">
			<xsl:if test="@id = $link">
			<!-- xsl count from 1 -->
				<xsl:value-of select="position() - 1"/>
				<xsl:text> </xsl:text>
			</xsl:if>
		</xsl:for-each>
	</xsl:for-each>

	</xsl:template>

</xsl:stylesheet>
