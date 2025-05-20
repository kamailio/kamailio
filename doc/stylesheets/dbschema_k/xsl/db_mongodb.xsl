<?xml version='1.0'?>
<!--
 * $Id: dbtext.xsl 4518 2008-07-28 15:39:28Z henningw $
 *
 * XSL converter script for mongodb
 *
 * Copyright (C) 2015 Daniel-Constantin Mierla (asipto.com)
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

	<!-- Create the file for the table in mongodb subdirectory -->
	<xsl:template match="table">
		<xsl:variable name="name">
			<xsl:call-template name="get-name"/>
		</xsl:variable>

		<xsl:variable name="path" select="concat($dir, concat('/', concat($prefix, concat($name, '.json'))))"/>
		<xsl:document href="{$path}" method="text" indent="no" omit-xml-declaration="yes">
			<xsl:text>{&#x0A;</xsl:text>
			<!-- Insert version data -->
			<xsl:apply-templates select="version"/> 
			<xsl:text>,&#x0A;</xsl:text>
			<xsl:apply-imports/>
			<xsl:text>&#x0A;}</xsl:text>
		</xsl:document>
	</xsl:template>

	<!-- version data template -->
	<xsl:template match="version">
		<xsl:text>  "name": "</xsl:text>
		<xsl:call-template name="get-name">
			<xsl:with-param name="select" select="parent::table"/>
		</xsl:call-template>
		<xsl:text>",&#x0A;  "version": </xsl:text>
		<xsl:value-of select="text()"/>
	</xsl:template>

	<!-- Create column definitions -->
	<xsl:template match="column">
		<xsl:if test="position()=1">
			<xsl:text>  "columns": [&#x0A;</xsl:text>
		</xsl:if>
		<xsl:variable name="type">
			<xsl:call-template name="get-type"/>
		</xsl:variable>

		<xsl:variable name="null">
			<xsl:call-template name="get-null"/>
		</xsl:variable>

		<xsl:text>    "</xsl:text>
		<xsl:call-template name="get-name"/>
		<xsl:text>": {&#x0A;      "type": "</xsl:text>
		<xsl:choose>
			<xsl:when test="type[@db=$db]">
				<xsl:value-of select="normalize-space(type[@db=$db])"/>
			</xsl:when>
			<xsl:when test="$type='string'">
				<xsl:text>string</xsl:text>
			</xsl:when>
			<xsl:when test="$type='char' or 
				$type='short' or 
				$type='int' or
				$type='long'">
				<xsl:text>int</xsl:text>
			</xsl:when>
			<xsl:when test="$type='float' or 
				$type='double'">
				<xsl:text>double</xsl:text>
			</xsl:when>
			<xsl:when test="$type='datetime'">
				<xsl:text>datetime</xsl:text>
			</xsl:when>
			<xsl:when test="$type='text' or
				$type='binary' or 
				$type='largetext' or
				$type='largebinary'">
				<xsl:text>text</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:call-template name="type-error"/>
			</xsl:otherwise>
		</xsl:choose>
		
		<xsl:text>",&#x0A;      "default": </xsl:text>

		<xsl:choose>
			<xsl:when test="default[@db=$db]">
				<xsl:choose>
					<xsl:when test="default[@db=$db]/null">
						<xsl:text>null</xsl:text>
					</xsl:when>
					<xsl:otherwise>
						<xsl:text>-00</xsl:text>
						<xsl:value-of select="default[@db=$db]"/>
					</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:when test="default">
				<xsl:choose>
					<xsl:when test="default/null">
						<xsl:text>null</xsl:text>
					</xsl:when>
					<xsl:when test="string(number(default))='NaN'"><!-- test for string value -->
						<xsl:text>"</xsl:text>
						<xsl:value-of select="default"/>
						<xsl:text>"</xsl:text>
					</xsl:when>
					<xsl:otherwise>
						<xsl:value-of select="default"/><!-- ommit the quotes for numbers -->
					</xsl:otherwise>
				</xsl:choose>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>null</xsl:text>
			</xsl:otherwise>
		</xsl:choose>

		<xsl:choose>
			<xsl:when test="$null=1">
				<xsl:text>,&#x0A;      "null": true</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:text>,&#x0A;      "null": false</xsl:text>
			</xsl:otherwise>
		</xsl:choose>
		<xsl:text>&#x0A;    }</xsl:text>
		<xsl:if test="not(position()=last())">
			<xsl:text>,</xsl:text>
		</xsl:if>
		<xsl:text>&#x0A;</xsl:text>
		<xsl:if test="position()=last()">
			<xsl:text>  ]</xsl:text>
		</xsl:if>
	</xsl:template>

	<!-- Escape all : occurrences -->
	<xsl:template name="escape">
		<xsl:param name="value"/>
		<xsl:choose>
			<xsl:when test="contains($value, ':')">
				<xsl:value-of select="concat(substring-before($value, ':'), '\:')"/>
				<xsl:call-template name="escape">
					<xsl:with-param name="value" select="substring-after($value, ':')"/>
				</xsl:call-template>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$value"/>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>

</xsl:stylesheet>
