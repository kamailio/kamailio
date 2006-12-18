<?xml version="1.0" encoding="UTF-8" standalone="no" ?>

<!--
<xsl:stylesheet version="1.0" 
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	>
-->

<!-- Namespace needed here bacuse namespaces are used within db schema
description -->

<xsl:stylesheet version="1.0" 
	xmlns="http://docbook.org/ns/docbook"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	>

<!-- doctype-system, doctyp-public found in http://www.xml.com/pub/a/2002/09/04/xslt.html -->
<xsl:output method="xml" indent="yes" version="1.0"
	doctype-system="http://www.oasis-open.org/docbook/xml/4.2/docbookx.dtd" 
	doctype-public="-//OASIS//DTD DocBook XML V4.2//EN"/>

<!--<xsl:template match="//database/name" mode="column_table">
</xsl:template>-->

<xsl:template match="//database">
<section><title><xsl:value-of select="name"/> database tables</title>
<!--	<para><table frame='all'><title>Tables</title>
	<tgroup cols='2' align='left' colsep='0' rowsep='0'>
	<colspec colname="c1"/><colspec colname="c2"/>
	<thead>
		<row>
			<entry>name</entry>
			<entry>comment</entry>
		</row>
	</thead>
	<tbody>
		<xsl:for-each select="table">
			<xsl:call-template name="table_proc_desc" mode="table_desc"/>
			<!- -<xsl:apply-templates mode="table_desc"/>- ->
		</xsl:for-each>
	</tbody></tgroup></table></para>
-->
	<para><variablelist>
		<xsl:for-each select="table">
			<xsl:call-template name="table_proc_desc" mode="table_desc"/>
		</xsl:for-each>
	</variablelist></para>

<para>
	<xsl:for-each select="table">
		<xsl:call-template name="table_proc" mode="column_table"/>
	</xsl:for-each>
	<!--<xsl:apply-templates mode="column_table"/>-->
</para>
</section>
</xsl:template>

<xsl:template name="table_proc_desc" match="table" mode="table_desc">
<!--	<row>
		<entry><xsl:value-of select="name"/></entry>
		<entry><xsl:value-of select="description"/></entry>
	</row>-->
	<xsl:variable name="tmp" select="name"/>
	<varlistentry>
<!--		<term><xsl:value-of select="name"/></term>-->
		<term><link linkend='gen_db_{$tmp}'><xsl:value-of select="name"/></link></term>
		<listitem>
			<!-- <para><xsl:value-of select="description"/></para>-->
			<!-- ! hack ! copied contents of the node and text within it. I
			recommend to copy only description/* and in all table descriptions
			use <para> -->
			<para><xsl:copy-of
			select="description/text()|description/*"/></para>
		</listitem>
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
			<!--<entry><xsl:value-of select="description"/></entry>-->

			<!-- ! hack ! copied contents of the node and text within it. I
			recommend to copy only description/* and in all column descriptions
			use <para> -->
			<entry><xsl:copy-of select="description/text()|description/*"/></entry>

			<!-- <entry><xsl:for-each
			select="description"><xsl:apply-templates/></xsl:for-each></entry>
			-->
		</row>
	</xsl:for-each>
<!--		<row>
			<entry namest="c1" nameend="c4"></entry>
		</row>
		<row>
			<entry namest="c1" nameend="c4"><xsl:value-of select="description"/></entry>
		</row>-->
	</tbody></tgroup></table>

<!-- </section> -->
</xsl:template>

</xsl:stylesheet>
