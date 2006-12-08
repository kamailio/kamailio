<?xml version="1.0" encoding="US-ASCII"?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:exsl="http://exslt.org/common" xmlns="http://www.w3.org/1999/xhtml" version="1.0" exclude-result-prefixes="exsl">

    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>
    <xsl:import href="xhtml.common.xsl"/>

    <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk-common.xsl"/>

    <xsl:include href="http://docbook.sourceforge.net/release/xsl/current/xhtml/manifest.xsl"/>
    <xsl:include href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk-code.xsl"/>
    
    <xsl:param name="use.id.as.filename">yes</xsl:param>
    <xsl:param name="chunk.quietly">1</xsl:param>
    <xsl:param name="suppress.header.navigation">1</xsl:param>


<xsl:template name="chunk-element-content">
  <xsl:param name="prev"/>
  <xsl:param name="next"/>
  <xsl:param name="nav.context"/>
  <xsl:param name="content">
    <xsl:apply-imports/>
  </xsl:param>

<!--
 <xsl:call-template name="user.preroot"/>
-->
      <xsl:call-template name="user.header.navigation"/>

      <xsl:call-template name="header.navigation">
        <xsl:with-param name="prev" select="$prev"/>
        <xsl:with-param name="next" select="$next"/>
        <xsl:with-param name="nav.context" select="$nav.context"/>
      </xsl:call-template>

      <xsl:call-template name="user.header.content"/>

      <xsl:copy-of select="$content"/>

      <xsl:call-template name="user.footer.content"/>

      <xsl:call-template name="footer.navigation">
        <xsl:with-param name="prev" select="$prev"/>
        <xsl:with-param name="next" select="$next"/>
        <xsl:with-param name="nav.context" select="$nav.context"/>
      </xsl:call-template>

      <xsl:call-template name="user.footer.navigation"/>
</xsl:template>

<!---    
    <xsl:template name="dbhtml-chunk">
	<xsl:param name="pis" select="./processing-instruction('dbhtml')"/>
	<xsl:call-template name="dbhtml-attribute">
	    <xsl:with-param name="pis" select="$pis"/>
	    <xsl:with-param name="attribute">chunk</xsl:with-param>
	</xsl:call-template>
    </xsl:template>
-->
    <!-- Override chunk template, it will also check if the dbhtml PI contains
         chunk="yes" attribute and if so then it will start a new chunk. This
         allows for fine-grained manual chunking
    -->
<!--
    <xsl:template name="chunk">
	<xsl:param name="node" select="."/>

	<xsl:variable name="make_chunk">
	    <xsl:call-template name="dbhtml-chunk">
		<xsl:with-param name="pis" select="$node/processing-instruction('dbhtml')"/>
	    </xsl:call-template>
	</xsl:variable>

	<xsl:choose>
	    <xsl:when test="$make_chunk='yes'">1</xsl:when>
	    <xsl:when test="not($node/parent::*)">1</xsl:when>
	    <xsl:when test="local-name($node)='sect1' and 
		            $chunk.section.depth &gt;= 1 and 
		            ($chunk.first.sections != 0 or
		            count($node/preceding-sibling::sect1) &gt; 0)
		           ">
		<xsl:text>1</xsl:text>
	    </xsl:when>
	    <xsl:when test="local-name($node)='sect2' and 
		            $chunk.section.depth &gt;= 2 and 
		            ($chunk.first.sections != 0 or 
		            count($node/preceding-sibling::sect2) &gt; 0)
		           ">
		<xsl:call-template name="chunk">
		    <xsl:with-param name="node" select="$node/parent::*"/>
		</xsl:call-template>
	    </xsl:when>
	    <xsl:when test="local-name($node)='sect3' and 
		            $chunk.section.depth &gt;= 3 and 
		            ($chunk.first.sections != 0 or
		            count($node/preceding-sibling::sect3) &gt; 0)
		           ">
		<xsl:call-template name="chunk">
		    <xsl:with-param name="node" select="$node/parent::*"/>
		</xsl:call-template>
	    </xsl:when>
	    <xsl:when test="local-name($node)='sect4' and 
		            $chunk.section.depth &gt;= 4 and 
		            ($chunk.first.sections != 0 or 
		            count($node/preceding-sibling::sect4) &gt; 0)
		           ">
		<xsl:call-template name="chunk">
		    <xsl:with-param name="node" select="$node/parent::*"/>
		</xsl:call-template>
	    </xsl:when>
	    <xsl:when test="local-name($node)='sect5' and 
		            $chunk.section.depth &gt;= 5 and 
		            ($chunk.first.sections != 0 or 
		            count($node/preceding-sibling::sect5) &gt; 0)
		           ">
		<xsl:call-template name="chunk">
		    <xsl:with-param name="node" select="$node/parent::*"/>
		</xsl:call-template>
	    </xsl:when>
	    <xsl:when test="local-name($node)='section' and 
		            $chunk.section.depth&gt;=count($node/ancestor::section)+1
		            and  ($chunk.first.sections != 0 or 
		            count($node/preceding-sibling::section) &gt; 0)
		           ">
		<xsl:call-template name="chunk">
		    <xsl:with-param name="node" select="$node/parent::*"/>
		</xsl:call-template>
	    </xsl:when>
	    
	    <xsl:when test="local-name($node)='preface'">1</xsl:when>
	    <xsl:when test="local-name($node)='chapter'">1</xsl:when>
	    <xsl:when test="local-name($node)='appendix'">1</xsl:when>
	    <xsl:when test="local-name($node)='article'">1</xsl:when>
	    <xsl:when test="local-name($node)='part'">1</xsl:when>
	    <xsl:when test="local-name($node)='reference'">1</xsl:when>
	    <xsl:when test="local-name($node)='refentry'">1</xsl:when>

	    <xsl:when test="local-name($node)='index' and 
		            ($generate.index != 0 or count($node/*) &gt; 0) 
		            and (local-name($node/parent::*) = 'article' or 
		            local-name($node/parent::*) = 'book' or 
		            local-name($node/parent::*) = 'part')">1
	    </xsl:when>
	    <xsl:when test="local-name($node)='bibliography' and 
		            (local-name($node/parent::*) = 'article' or 
		            local-name($node/parent::*) = 'book' or 
		            local-name($node/parent::*) = 'part')">1</xsl:when>
	    <xsl:when test="local-name($node)='glossary' and 
		            (local-name($node/parent::*) = 'article' or 
		            local-name($node/parent::*) = 'book' or 
		            local-name($node/parent::*) = 'part')">1</xsl:when>
	    <xsl:when test="local-name($node)='colophon'">1</xsl:when>
	    <xsl:when test="local-name($node)='book'">1</xsl:when>
	    <xsl:when test="local-name($node)='set'">1</xsl:when>
	    <xsl:when test="local-name($node)='setindex'">1</xsl:when>
	    <xsl:when test="local-name($node)='legalnotice' and $generate.legalnotice.link != 0">1</xsl:when>
	    <xsl:otherwise>0</xsl:otherwise>
	</xsl:choose>
    </xsl:template>
-->

</xsl:stylesheet>
