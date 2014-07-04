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

	<!-- Create the file for the mod in pi_http subdirectory -->
	<xsl:template match="/">
		<xsl:variable name="path" select="concat($dir, concat('/', concat($prefix, 'mod')))"/>
		<xsl:document href="{$path}" method="text" indent="no" omit-xml-declaration="yes">
			<xsl:apply-templates select="/database[1]"/>
		</xsl:document>
	</xsl:template>

	<xsl:template match="table">
		<xsl:variable name="table.name">
			<xsl:call-template name="get-name"/>
		</xsl:variable>
		<xsl:text>&#x9;&lt;!-- </xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text> provisionning --&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&lt;mod&gt;&lt;mod_name&gt;</xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text>&lt;/mod_name&gt;&#xa;</xsl:text>

		<!-- show/DB1_QUERY -->
		<xsl:text>&#x9;&#x9;&lt;cmd&gt;&lt;cmd_name&gt;show&lt;/cmd_name&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&#x9;&lt;db_table_id&gt;</xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text>&lt;/db_table_id&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&#x9;&lt;cmd_type&gt;DB1_QUERY&lt;/cmd_type&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&#x9;&lt;query_cols&gt;&#xa;</xsl:text>
		<xsl:for-each select="column">
			<xsl:choose>
				<xsl:when test="primary">
					<xsl:text>&#x9;&#x9;&#x9;&#x9;&lt;col&gt;&lt;field&gt;</xsl:text>
					<xsl:call-template name="get-name"/>
					<xsl:text>&lt;/field&gt;&lt;link_cmd&gt;update&lt;/link_cmd&gt;&lt;/col&gt;&#xa;</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>&#x9;&#x9;&#x9;&#x9;&lt;col&gt;&lt;field&gt;</xsl:text>
					<xsl:call-template name="get-name"/>
					<xsl:text>&lt;/field&gt;&lt;/col&gt;&#xa;</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:for-each>
		<xsl:text>&#x9;&#x9;&#x9;&lt;/query_cols&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&lt;/cmd&gt;&#xa;</xsl:text>

		<!-- add/DB1_INSERT -->
		<xsl:text>&#x9;&#x9;&lt;cmd&gt;&lt;cmd_name&gt;add&lt;/cmd_name&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&#x9;&lt;db_table_id&gt;</xsl:text>
		<xsl:value-of select="$table.name"/>
		<xsl:text>&lt;/db_table_id&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&#x9;&lt;cmd_type&gt;DB1_INSERT&lt;/cmd_type&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&#x9;&lt;query_cols&gt;&#xa;</xsl:text>
		<xsl:for-each select="column">
			<xsl:choose>
				<xsl:when test="autoincrement">
				</xsl:when>
				<xsl:otherwise>
					<xsl:text>&#x9;&#x9;&#x9;&#x9;&lt;col&gt;&lt;field&gt;</xsl:text>
					<xsl:call-template name="get-name"/>
					<xsl:text>&lt;/field&gt;&lt;/col&gt;&#xa;</xsl:text>
				</xsl:otherwise>
			</xsl:choose>
		</xsl:for-each>
		<xsl:text>&#x9;&#x9;&#x9;&lt;/query_cols&gt;&#xa;</xsl:text>
		<xsl:text>&#x9;&#x9;&lt;/cmd&gt;&#xa;</xsl:text>

		<!-- update/DB1_UPDATE -->
		<xsl:if test="column/primary">
			<xsl:text>&#x9;&#x9;&lt;cmd&gt;&lt;cmd_name&gt;update&lt;/cmd_name&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;db_table_id&gt;</xsl:text>
			<xsl:value-of select="$table.name"/>
			<xsl:text>&lt;/db_table_id&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;cmd_type&gt;DB1_UPDATE&lt;/cmd_type&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;clause_cols&gt;&#xa;</xsl:text>
			<xsl:for-each select="column">
				<xsl:if test="primary">
					<xsl:text>&#x9;&#x9;&#x9;&#x9;&lt;col&gt;&lt;field&gt;</xsl:text>
					<xsl:call-template name="get-name"/>
					<xsl:text>&lt;/field&gt;&lt;operator&gt;=&lt;/operator&gt;&lt;/col&gt;&#xa;</xsl:text>
				</xsl:if>
			</xsl:for-each>
			<xsl:text>&#x9;&#x9;&#x9;&lt;/clause_cols&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;query_cols&gt;&#xa;</xsl:text>
			<xsl:for-each select="column">
				<xsl:choose>
					<xsl:when test="primary">
					</xsl:when>
					<xsl:otherwise>
						<xsl:text>&#x9;&#x9;&#x9;&#x9;&lt;col&gt;&lt;field&gt;</xsl:text>
						<xsl:call-template name="get-name"/>
						<xsl:text>&lt;/field&gt;&lt;/col&gt;&#xa;</xsl:text>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:for-each>
			<xsl:text>&#x9;&#x9;&#x9;&lt;/query_cols&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&lt;/cmd&gt;&#xa;</xsl:text>
		</xsl:if>

		<!-- delete/DB1_DELETE -->
		<xsl:if test="column/primary">
			<xsl:text>&#x9;&#x9;&lt;cmd&gt;&lt;cmd_name&gt;delete&lt;/cmd_name&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;db_table_id&gt;</xsl:text>
			<xsl:value-of select="$table.name"/>
			<xsl:text>&lt;/db_table_id&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;cmd_type&gt;DB1_DELETE&lt;/cmd_type&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&#x9;&lt;clause_cols&gt;&#xa;</xsl:text>
			<xsl:for-each select="column">
				<xsl:if test="primary">
					<xsl:text>&#x9;&#x9;&#x9;&#x9;&lt;col&gt;&lt;field&gt;</xsl:text>
					<xsl:call-template name="get-name"/>
					<xsl:text>&lt;/field&gt;&lt;operator&gt;=&lt;/operator&gt;&lt;/col&gt;&#xa;</xsl:text>
				</xsl:if>
			</xsl:for-each>
			<xsl:text>&#x9;&#x9;&#x9;&lt;/clause_cols&gt;&#xa;</xsl:text>
			<xsl:text>&#x9;&#x9;&lt;/cmd&gt;&#xa;</xsl:text>
		</xsl:if>

		<xsl:text>&#x9;&lt;/mod&gt;&#xa;</xsl:text>
	</xsl:template>

</xsl:stylesheet>

