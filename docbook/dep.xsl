<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
				version='1.0' xmlns:xi="http://www.w3.org/2001/XInclude">
	<xsl:param name="output"/>

	<!-- Write the output into a plaintext file which will be later included
		 into the Makefile -->
	<xsl:template match="/">
		<xsl:document href="{$output}" method="text" indent="no" 
					  omit-xml-declaration="yes">
			<xsl:value-of select="concat($output, ':$(wildcard  ')"/>
			<xsl:apply-templates mode="subroot"/>
			<xsl:text>)&#xA;</xsl:text>
		</xsl:document>
	</xsl:template>
	
	<!-- This template extract the name of the directory from a full pathname,
	     in other words it returns everything but the name of the file -->
	<xsl:template name="dirname">
		<xsl:param name="filename"/>
		<xsl:if test="contains($filename, '/')">
			<xsl:value-of 
				select="concat(substring-before($filename, '/'), '/')"/>
			<xsl:call-template name="dirname">
				<xsl:with-param name="filename" 
								select="substring-after($filename, '/')"/>
			</xsl:call-template>
		</xsl:if>
	</xsl:template>

	<!-- This template is used to add a directory preefix to a filename. The
	     prefix is only added if the filename is not absolute (i.e. it does
	     not start with a / and if the prefix is not an empty string -->
	<xsl:template name="addprefix">
		<xsl:param name="prefix"/>
		<xsl:param name="filename"/>
		<xsl:if test="(string-length($prefix) > 0) and not(starts-with($filename, '/'))">
			<xsl:value-of select="$prefix"/>
		</xsl:if>
		<xsl:value-of select="$filename"/>
	</xsl:template>
	
	<!-- This template processes xi:include directives that include other XML
	     documents. First the template outputs the name of the file being
	     included and then the template traverses the included file
	     recursively, searching fro other dependencies in that file.  The
	     template passes the parameter prefix to other templates with its
	     value set to the directory name of the file being included. This
	     ensures that paths to all dependencies are relative to the main
	     file. -->
	<xsl:template match='xi:include' mode="subroot">
		<xsl:param name="prefix"/>

		<!-- Add the prefix to the name of the file being included and store
		     the result in variable fullpath -->
		<xsl:variable name="fullpath">
			<xsl:call-template name="addprefix">
				<xsl:with-param name="prefix" select="$prefix"/>
				<xsl:with-param name="filename" select="@href"/>
			</xsl:call-template>
		</xsl:variable>

		<!-- First of all, output the name of the file being included, with
		     proper prefix so that the resulting dependency is relative to the
		     top-most file being processed, not the file we are are processing
		     in this step. -->
		<xsl:value-of select="concat($fullpath, ' ')"/>

		<!-- Traverse the file being included and search for more depencencies
		     in that file and other files included from there. -->
		<xsl:apply-templates select="document(@href)" mode="subroot">
			<!-- Extract the directory name from $fullpath and set it as a new
			     value of the prefix parameter before calling other templates.
			-->
			<xsl:with-param name="prefix">
				<xsl:call-template name="dirname">
					<xsl:with-param name="filename" select="$fullpath"/>
				</xsl:call-template>
			</xsl:with-param>

			<!-- Process the included file recursively -->
		</xsl:apply-templates>
	</xsl:template>
	
	<!-- This template processes files included with xi:include that are not
	     XML files, such files will only be output as dependencies and will be
	     not traversed recursively. -->
	<xsl:template match='xi:include[@parse="text"]' mode="subroot">
		<xsl:param name="prefix"/>
		<xsl:call-template name="addprefix">
			<xsl:with-param name="prefix" select="$prefix"/>
			<xsl:with-param name="filename" select="@href"/>
		</xsl:call-template>
		<xsl:text> </xsl:text>
	</xsl:template>
	
	<!-- This template processes mediaobjects (such as images) included in
	     docbook. -->
	<xsl:template match="graphic|imagedata|inlinemediaobject|textdata" 
				  mode="subroot">
		<xsl:param name="prefix"/>
		<xsl:call-template name="addprefix">
			<xsl:with-param name="prefix" select="$prefix"/>
			<xsl:with-param name="filename" select="@fileref"/>
		</xsl:call-template>
		<xsl:text> </xsl:text>
	</xsl:template>
	
	<!-- Supress all other output -->
	<xsl:template match="text()|@*" mode="subroot"/>
</xsl:stylesheet>
