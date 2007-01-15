<?xml version="1.0" encoding="UTF-8" standalone="no" ?>

<!-- Namespaces are NOT used in docbook < 5.0 - they SHOULD NOT be used in db schema description -->
<!--
<xsl:stylesheet version="1.0" xmlns="http://docbook.org/ns/docbook"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
>-->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<!-- doctype-system, doctyp-public found in http://www.xml.com/pub/a/2002/09/04/xslt.html -->
<xsl:output method="xml" indent="yes" version="1.0"
	doctype-system="http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd" 
	doctype-public="-//OASIS//DTD DocBook XML V4.2//EN"/>

<xsl:template match="//database">
<section><title><xsl:value-of select="name"/> database tables</title>
	<!-- generate table descriptions -->
	<para><variablelist>
		<xsl:for-each select="table">
			<xsl:call-template name="table_proc_desc" mode="table_desc"/>
		</xsl:for-each>
	</variablelist></para>

	<!-- generate table contents -->
	<para>
		<xsl:for-each select="table">
			<xsl:call-template name="table_proc" mode="column_table"/>
		</xsl:for-each>
		<!--<xsl:apply-templates mode="column_table"/>-->
	</para>
</section>
</xsl:template>

<!-- Needed for copying whole nodes from db schema description. We
can not use xsl:copy because in such case are always included namespaces
defined in compiled document (ser.xml for example uses 
xmlns:xi="http://www.w3.org/2001/XInclude") but Docbook DTD (version less 
than 5) doesn't allow "xmlns" attributes -->


<xsl:template match="@*|node()" mode="copying">
	<xsl:choose>	
		<xsl:when test="local-name() and node()"> <!-- it is probably an element ;-) -->
					<xsl:element name="{local-name()}">
						<xsl:apply-templates select="@*|node()" mode="copying"/>
					</xsl:element>
		</xsl:when>
		<xsl:otherwise> <!-- anything else - copy it -->
			<xsl:copy>
				<xsl:apply-templates select="@*|node()" mode="copying"/>
			</xsl:copy>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="copy_content_without_namespaces">
	<xsl:apply-templates select="@*|node()" mode="copying"/>
</xsl:template>

<!-- Common processing <description> node within <table> and within <column>:
       - text in <description> element is given at first (if not empty, it is nested in para)
	   - all nested elements are included
	   - if there are no nested elements the text is added even if empty
-->
<xsl:template name="process_description">
	<xsl:choose>
		<xsl:when test="description/*">
			<xsl:choose>
				<xsl:when test="description/para"> <!-- there are some para elements -->
					<xsl:for-each select="description"><xsl:call-template
					name="copy_content_without_namespaces"/></xsl:for-each>
				</xsl:when>
				
				<xsl:when test="description/*[local-name()='para']"> <!-- there are some para elements -->
<!--					<xsl:message>copying description X: '<xsl:value-of select="description/text()"/>'</xsl:message>-->
					<xsl:for-each select="description"><xsl:call-template
					name="copy_content_without_namespaces"/></xsl:for-each>
				</xsl:when>
				
				<!-- if text of description is not empty add description
				internals into a para element -->
				<xsl:otherwise>
<!--					<xsl:message>copying description into para: '<xsl:value-of select="description/text()"/>'</xsl:message>-->
					<para><xsl:for-each select="description">
					<xsl:call-template name="copy_content_without_namespaces"/></xsl:for-each></para>
				</xsl:otherwise>

			</xsl:choose>
		</xsl:when>
		<xsl:otherwise>
			<!-- use text within description element (may be empty) -->
			<para><xsl:value-of select="description/text()"/></para>
		</xsl:otherwise>
	</xsl:choose>
</xsl:template>

<xsl:template name="table_proc_desc" match="table" mode="table_desc">
	<xsl:variable name="tmp" select="name"/>
	<varlistentry>
		<term><link linkend='gen_db_{$tmp}'><xsl:value-of select="name"/></link></term>
		<listitem><xsl:call-template name="process_description"/></listitem>
	</varlistentry>
</xsl:template>

<xsl:template name="table_proc" match="table" mode="column_table">

	<!--<section><title><xsl:value-of select="name"/></title>
	<para><xsl:value-of select="description"/></para>-->

	<xsl:variable name="tmp" select="name"/>
	<table id='gen_db_{$tmp}' frame='all'><title>Table "<xsl:value-of select="name"/>"</title>
	<tgroup cols='4' align='left' colsep='1' rowsep='1'>
	<colspec colname="c1"/><colspec colname="c2"/><colspec colname="c3"/><colspec colname="c4"/>
	<thead>
		<row>
			<entry>name</entry>
			<entry>type</entry>
			<entry>size</entry>
			<entry>description</entry>
		</row>
	</thead>
	<tbody>
	<xsl:for-each select="column">
		<row>
			<entry><varname><xsl:value-of select="name"/></varname></entry>
			<entry><varname><xsl:value-of select="type"/></varname></entry>
			<entry><constant><xsl:value-of select="size"/></constant></entry>
			<entry><xsl:call-template name="process_description"/></entry>
		</row>
	</xsl:for-each>
	</tbody></tgroup></table>
</xsl:template>

</xsl:stylesheet>
