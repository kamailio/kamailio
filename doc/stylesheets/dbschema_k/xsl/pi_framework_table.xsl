<?xml version='1.0'?>
<!--
  * Copyright (C) 2012 VoIP Embedded, Inc.
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
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
				version='1.0'
				xmlns:xi="http://www.w3.org/2001/XInclude">

	<xsl:import href="sql.xsl"/>

	<!-- Create the file for the table in pi_http subdirectory -->
	<xsl:template match="/">

		<xsl:variable name="path" select="concat($dir, concat('/', concat($prefix, 'table')))"/>
		<xsl:document href="{$path}" method="text" indent="no" omit-xml-declaration="yes">
			<xsl:apply-templates select="/database[1]"/>
		</xsl:document>
	</xsl:template>

	<xsl:template match="table">
		<xsl:variable name="table.name">
			<xsl:call-template name="get-name"/>
		</xsl:variable>
		<xsl:text>&#x9;&lt;!-- Declaration of </xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text> table--&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&lt;db_table id="</xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text>"&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&lt;table_name&gt;</xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text>&lt;/table_name&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&lt;db_url_id&gt;mysql&lt;/db_url_id&gt;&#xa;</xsl:text>
		<xsl:apply-templates select="column"/>
		<xsl:text>&#x9;&lt;/db_table&gt;&#xa;</xsl:text>
	</xsl:template>

	<xsl:template match="column">
		<xsl:text>&#x9;&#x9;&lt;column&gt;&lt;field&gt;</xsl:text>
		<xsl:call-template name="get-name"/>
		<xsl:text>&lt;/field&gt;&lt;type&gt;</xsl:text>
		<xsl:call-template name="column.type"/>
		<xsl:text>&lt;/type&gt;&lt;/column&gt;&#xa;</xsl:text>
	</xsl:template>

	<xsl:template name="column.type">
		<xsl:variable name="type">
			<xsl:call-template name="get-type"/>
		</xsl:variable>
		<xsl:choose>
			<xsl:when test="type[@db='mysql']">
			<xsl:value-of select="normalize-space(type[@db='mysql'])"/>
			</xsl:when>
			<xsl:when test="$type='char'">
			<xsl:text>DB1_INT</xsl:text>
			</xsl:when>
			<xsl:when test="$type='short'">
			<xsl:text>DB1_INT</xsl:text>
			</xsl:when>
			<xsl:when test="$type='int'">
			<xsl:text>DB1_INT</xsl:text>
			</xsl:when>
			<xsl:when test="$type='long'">
			<xsl:text>DB1_BIGINT</xsl:text>
			</xsl:when>
			<xsl:when test="$type='datetime'">
			<xsl:text>DB1_DATETIME</xsl:text>
			</xsl:when>
			<xsl:when test="$type='double'">
			<xsl:text>DB1_DOUBLE</xsl:text>
			</xsl:when>
			<xsl:when test="$type='float'">
			<xsl:text>DB1_DOUBLE</xsl:text>
			</xsl:when>
			<xsl:when test="$type='string'">
			<xsl:text>DB1_STR</xsl:text>
			</xsl:when>
			<xsl:when test="$type='binary'">
			<xsl:text>DB1_BLOB</xsl:text>
			</xsl:when>
			<xsl:when test="$type='largebinary'">
			<xsl:text>DB1_BLOB</xsl:text>
			</xsl:when>
			<xsl:when test="$type='text'">
			<xsl:text>DB1_BLOB</xsl:text>
			</xsl:when>
			<xsl:when test="$type='largetext'">
			<xsl:text>DB1_BLOB</xsl:text>
			</xsl:when>
			<xsl:otherwise>
			<xsl:call-template name="type-error"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>

