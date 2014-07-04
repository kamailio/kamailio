<?xml version='1.0'?>
<!--
 * $Id: docbook.xsl 4518 2008-07-28 15:39:28Z henningw $
 *
 * XSL converter script for docbook
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


<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>
    <xsl:import href="common.xsl"/>
    <xsl:import href="dbschema2docbook.xsl"/>

    <xsl:template match="/">
	<xsl:variable name="path" select="concat($dir, concat('/', concat($prefix, 'dbschema.xml')))"/>
	<xsl:document href="{$path}" method="xml" indent="yes" omit-xml-declaration="yes">
	<!-- only needed for stand alone documents for each table -->
	<!--doctype-system="http://www.oasis-open.org/docbook/sgml/4.4/docbook.dtd"
	doctype-public="-//OASIS//DTD DocBook V4.4//EN">-->
	<!--<xsl:element name="book">-->
	<xsl:element name="chapter">
	    <!--<xsl:element name="section">-->
		<xsl:element name="title">
		    <xsl:call-template name="get-name">
			<xsl:with-param name="select" select="database[1]"/>
		    </xsl:call-template>
		</xsl:element>
		<xsl:apply-templates select="/database[1]"/>
	    <!--</xsl:element>-->
	</xsl:element>
        <!--</xsl:element>-->
	</xsl:document>
    </xsl:template>

</xsl:stylesheet>
