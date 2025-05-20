<?xml version='1.0'?>
<!--
 * $Id: oracle.xsl 4535 2008-08-01 10:37:23Z henningw $
 *
 * XSL converter script for oracle databases
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
                xmlns:db="http://iptel.org/dbschema/oracle"
>

    <xsl:import href="sql.xsl"/>

    <xsl:template match="database" mode="drop">
	<xsl:apply-templates mode="drop"/>
    </xsl:template>

    <xsl:template name="table.close">
	<xsl:variable name="table.name">
		<xsl:call-template name="get-name"/>
	</xsl:variable>
	<xsl:text>)</xsl:text>
	<xsl:text>;&#x0A;&#x0A;</xsl:text>

	<!-- small hack, as the version table don't have an id field -->
	<xsl:if test="not($table.name='version')">
		<!-- create the autoincrement trigger -->
		<xsl:text>CREATE OR REPLACE TRIGGER </xsl:text>
		<xsl:value-of select="concat($table.name, '_tr&#x0A;')"/>
		<xsl:text>before insert on </xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text> FOR EACH ROW&#x0A;</xsl:text>
		<xsl:text>BEGIN&#x0A;</xsl:text>
		<xsl:text>  auto_id(:NEW.id);&#x0A;</xsl:text>
		<xsl:text>END </xsl:text>
		<xsl:value-of select="concat($table.name, '_tr;&#x0A;')"/>
		<xsl:text>/&#x0A;</xsl:text>
	</xsl:if>
	<xsl:text>BEGIN map2users('</xsl:text>
	<xsl:value-of select="$table.name"/>
	<xsl:text>'); END;&#x0A;</xsl:text>
	<xsl:text>/&#x0A;</xsl:text>
    </xsl:template>

    <xsl:template name="column.type">
	<xsl:variable name="type">
	    <xsl:call-template name="get-type"/>
	</xsl:variable>

	<xsl:choose>
	    <xsl:when test="type[@db=$db]">
		<xsl:value-of select="normalize-space(db:type)"/>
	    </xsl:when>
	    <xsl:when test="$type='char'">
		<xsl:text>NUMBER(5)</xsl:text>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='short'">
		<xsl:text>NUMBER(5)</xsl:text>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='int'">
		<xsl:text>NUMBER(10)</xsl:text>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='bitmap'">
		<xsl:text>NUMBER(11)</xsl:text>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='long'">
		<xsl:text>BIGINT</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='datetime'">
		<xsl:text>DATE</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='double'">
		<xsl:text>NUMBER</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='float'">
		<xsl:text>NUMBER</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='string'">
		<xsl:text>VARCHAR2</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='binary' or 
						$type='largebinary'">
		<xsl:text>BLOB</xsl:text>
		<xsl:call-template name="column.size"/>
		<xsl:call-template name="column.trailing"/>
	    </xsl:when>
	    <xsl:when test="$type='text' or 
                        $type='largetext'">
		<xsl:text>CLOB</xsl:text>
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
	<!-- PRIMARY KEY column definition -->
	<xsl:if test="primary">
		<xsl:variable name="table.name">
	    	<xsl:call-template name="get-name">
				<xsl:with-param name="select" select="parent::table"/>
			</xsl:call-template>
		</xsl:variable>
		<xsl:text> PRIMARY KEY</xsl:text>
	</xsl:if>
    </xsl:template>

    <!-- copied from sql.xsl, for oracle the empty string and NULL are equivalent -->
    <xsl:template match="column">
	<xsl:text>    </xsl:text>
	<xsl:call-template name="get-name"/>
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
		<xsl:if test="string(number(default))!='NaN'"><!-- test for string value -->
	    	<xsl:text> NOT NULL</xsl:text>
		</xsl:if>
	</xsl:if>

	<xsl:if test="not(position()=last())">
	    <xsl:text>,</xsl:text>
	    <xsl:text>&#x0A;</xsl:text>
	</xsl:if>
    </xsl:template>

	<xsl:template name="get-index-name">
	<xsl:variable name="index.name">
	    <xsl:call-template name="get-name"/>
	</xsl:variable>
	<xsl:variable name="table.name">
	    <xsl:call-template name="get-name">
		<xsl:with-param name="select" select="parent::table"/>
	    </xsl:call-template>
	</xsl:variable>
	<!-- because oracle don't allow index names longer than 30 -->
	<xsl:choose>
	<xsl:when test="not(string-length(concat($table.name, '_', $index.name)) > 30)">
		<xsl:value-of select="concat($table.name, '_', $index.name, ' ')"/>
	</xsl:when>
	<xsl:otherwise>
		<xsl:value-of select="concat('ORA_', $index.name, ' ')"/>
	</xsl:otherwise>
	</xsl:choose>
	</xsl:template>

</xsl:stylesheet>
