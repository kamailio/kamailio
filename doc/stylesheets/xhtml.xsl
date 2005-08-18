<?xml version='1.0'?>
<xsl:stylesheet  
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>

<!-- Stylesheets common for all transformations (xhtml, txt, fo) -->
<xsl:import href="common.xsl"/>
    
<!-- HTML Customization -->
    <xsl:param name="html.base"/>
    <xsl:param name="html.cellpadding" select="''"/>
    <xsl:param name="html.cellspacing" select="''"/>
    <xsl:param name="html.cleanup" select="0"/>
    <xsl:param name="html.ext" select="'.html'"/>
    <xsl:param name="html.extra.head.links" select="1"/>
    <xsl:param name="html.longdesc" select="1"/>
    <xsl:param name="html.longdesc.link" select="$html.longdesc"/>
    <xsl:param name="html.stylesheet" select="'ser_rpc.css'"/>
    <xsl:param name="html.stylesheet.type">text/css</xsl:param>
    <xsl:param name="css.decoration" select="1"/>

    
<!-- Customization of callouts -->
    <xsl:param name="callout.defaultcolumn" select="'60'"/>
    <xsl:param name="callout.graphics.extension" select="'.png'"/>
    <xsl:param name="callout.graphics" select="'1'"/>
    <xsl:param name="callout.graphics.number.limit" select="'15'"/>
    <xsl:param name="callout.graphics.path" select="'images/callouts/'"/>
    <xsl:param name="callout.list.table" select="'1'"/>
    <xsl:param name="callout.unicode" select="0"/>
    <xsl:param name="callout.unicode.number.limit" select="'10'"/>
    <xsl:param name="callout.unicode.start.character" select="10102"/>
    <xsl:param name="callouts.extension" select="'1'"/>

<!-- Customization of admon -->
<!-- Note, Warning, Tip, Caution, Important and such -->
    <xsl:param name="admon.graphics.extension" select="'.png'"/>
    <xsl:param name="admon.graphics" select="0"/>
    <xsl:param name="admon.graphics.path">images/</xsl:param>
    <xsl:param name="admon.style">
	<xsl:text>margin-left: 0.5in; margin-right: 0.5in;</xsl:text>
    </xsl:param>
    <xsl:param name="admon.textlabel" select="1"/>

<!-- Glossary -->
    <xsl:param name="glossary.collection" select="''"/>
    <xsl:param name="glossentry.show.acronym" select="'no'"/>
    <xsl:param name="glossterm.auto.link" select="0"/>


<!-- Tables -->
    <xsl:param name="table.borders.with.css" select="0"/>
    <xsl:param name="table.cell.border.color" select="''"/>
    <xsl:param name="table.cell.border.style" select="'solid'"/>
    <xsl:param name="table.cell.border.thickness" select="'0.5pt'"/>
    <xsl:param name="table.footnote.number.format" select="'a'"/>
    <xsl:param name="table.footnote.number.symbols" select="''"/>
    <xsl:param name="table.frame.border.color" select="''"/>
    <xsl:param name="table.frame.border.style" select="'solid'"/>
    <xsl:param name="table.frame.border.thickness" select="'0.5pt'"/>
    <xsl:param name="tablecolumns.extension" select="'1'"/>

<!-- Chunking -->    
    <xsl:param name="chunk.first.sections" select="0"/>
    <xsl:param name="chunk.quietly" select="0"/>
    <xsl:param name="chunk.section.depth" select="1"/>
    <xsl:param name="chunk.toc" select="''"/>
    <xsl:param name="chunk.tocs.and.lots" select="0"/>
    <xsl:param name="chunk.separate.lots" select="0"/>

<!-- Bibliography -->
    <xsl:param name="biblioentry.item.separator">. </xsl:param>
    <xsl:param name="bibliography.collection" select="'http://docbook.sourceforge.net/release/bibliography/bibliography.xml'"/>
    <xsl:param name="bibliography.numbered" select="0"/>

<!-- olink -->
    <xsl:param name="olink.lang.fallback.sequence" select="''"/> 
    <xsl:param name="olink.doctitle" select="no"/> 
    <xsl:param name="olink.fragid" select="'fragid='"/>
    <xsl:param name="olink.outline.ext" select="'.olink'"/>
    <xsl:param name="olink.pubid" select="'pubid='"/>
    <xsl:param name="olink.resolver" select="'/cgi-bin/olink'"/>
    <xsl:param name="olink.sysid" select="'sysid='"/>
    <xsl:param name="insert.olink.page.number">no</xsl:param>
    <xsl:param name="insert.olink.pdf.frag" select="0"/>
    <xsl:param name="olink.base.uri" select="''"/> 
    <xsl:param name="olink.debug" select="0"/>
    <xsl:attribute-set name="olink.properties">
    </xsl:attribute-set>
    <xsl:param name="prefer.internal.olink" select="0"/>
    <xsl:param name="use.local.olink.style" select="0"/> 


<!-- xref -->
    <xsl:param name="xref.with.number.and.title" select="1"/>
    <xsl:param name="xref.label-title.separator">: </xsl:param>
    <xsl:param name="xref.label-page.separator"><xsl:text> </xsl:text></xsl:param>
    <xsl:param name="xref.title-page.separator"><xsl:text> </xsl:text></xsl:param>
    <xsl:param name="insert.xref.page.number">no</xsl:param>

<!-- Table of Contents -->
    <xsl:param name="annotate.toc" select="1"/>
    <xsl:param name="autotoc.label.separator" select="'. '"/>
    <xsl:param name="bridgehead.in.toc" select="0"/>
    <xsl:param name="generate.section.toc.level" select="1"/>
    <xsl:param name="generate.toc">
appendix  toc,title
article/appendix  nop
article   toc,title
book      toc,title,figure,table,example,equation
chapter   toc,title
part      toc,title
preface   toc,title
qandadiv  toc
qandaset  toc
reference toc,title
sect1     toc
sect2     toc
sect3     toc
sect4     toc
sect5     toc
section   toc,title
set       toc,title
    </xsl:param>
    <xsl:param name="manual.toc" select="''"/>
    <xsl:param name="process.empty.source.toc" select="0"/>
    <xsl:param name="process.source.toc" select="0"/>
    <xsl:param name="simplesect.in.toc" select="0"/>
    <xsl:param name="toc.list.type">dl</xsl:param>
    <xsl:param name="toc.section.depth">2</xsl:param>
    <xsl:param name="toc.max.depth">8</xsl:param>

<!-- Profile -->
    <xsl:param name="profile.arch" select="''"/>
    <xsl:param name="profile.attribute" select="''"/>
    <xsl:param name="profile.condition" select="''"/>
    <xsl:param name="profile.conformance" select="''"/>
    <xsl:param name="profile.lang" select="''"/>
    <xsl:param name="profile.os" select="''"/>
    <xsl:param name="profile.revision" select="''"/>
    <xsl:param name="profile.revisionflag" select="''"/>
    <xsl:param name="profile.role" select="''"/>
    <xsl:param name="profile.security" select="''"/>
    <xsl:param name="profile.separator" select="';'"/>
    <xsl:param name="profile.userlevel" select="''"/>
    <xsl:param name="profile.value" select="''"/>
    <xsl:param name="profile.vendor" select="''"/>


<xsl:param name="appendix.autolabel" select="1"/>
<xsl:param name="author.othername.in.middle" select="1"/>
<xsl:param name="base.dir" select="''"/>


<xsl:param name="chapter.autolabel" select="0"/>

<xsl:param name="citerefentry.link" select="'0'"/>
<xsl:param name="collect.xref.targets" select="'no'"/>
<xsl:param name="component.label.includes.part.label" select="0"/>


<xsl:param name="current.docid" select="''"/> 
<xsl:param name="default.float.class" select="'before'"/>
<xsl:param name="default.image.width" select="''"/>
<xsl:param name="default.table.width" select="''"/>
<xsl:param name="draft.mode" select="'maybe'"/>
<xsl:param name="draft.watermark.image" select="'http://docbook.sourceforge.net/release/images/draft.png'"/>
<xsl:param name="ebnf.table.bgcolor" select="'#F5DCB3'"/>
<xsl:param name="ebnf.table.border" select="1"/>
<xsl:param name="ebnf.assignment">
<code>::=</code>
</xsl:param>

<xsl:param name="ebnf.statement.terminator"/>

<xsl:param name="eclipse.autolabel" select="0"/>
<xsl:param name="eclipse.plugin.name">DocBook Online Help Sample</xsl:param>
<xsl:param name="eclipse.plugin.id">com.example.help</xsl:param>
<xsl:param name="eclipse.plugin.provider">Example provider</xsl:param>
<xsl:param name="emphasis.propagates.style" select="1"/>
<xsl:param name="entry.propagates.style" select="1"/>
<xsl:param name="firstterm.only.link" select="0"/>
<xsl:param name="footer.rule" select="1"/>
<xsl:param name="footnote.number.format" select="'1'"/>
<xsl:param name="footnote.number.symbols" select="''"/>
<xsl:param name="formal.procedures" select="1"/>
<xsl:param name="formal.title.placement">
figure before
example before
equation before
table before
procedure before
task before
</xsl:param>
<xsl:param name="funcsynopsis.decoration" select="1"/>
<xsl:param name="funcsynopsis.style">kr</xsl:param>
<xsl:param name="funcsynopsis.tabular.threshold" select="40"/>
<xsl:param name="function.parens">0</xsl:param>
<xsl:param name="generate.id.attributes" select="0"/>
<xsl:param name="generate.index" select="1"/>
<xsl:param name="generate.legalnotice.link" select="0"/>
<xsl:param name="generate.manifest" select="0"/>
<xsl:param name="generate.meta.abstract" select="1"/>

<xsl:param name="graphic.default.extension"/>
<xsl:param name="graphicsize.extension" select="1"/>
<xsl:param name="header.rule" select="1"/>
<xsl:param name="img.src.path"/>
<xsl:param name="index.on.role" select="0"/>
<xsl:param name="index.on.type" select="0"/>
<xsl:param name="index.prefer.titleabbrev" select="0"/>
<xsl:param name="ignore.image.scaling" select="0"/>
<xsl:param name="inherit.keywords" select="'1'"/>
<xsl:param name="l10n.gentext.default.language" select="'en'"/>
<xsl:param name="l10n.gentext.language" select="''"/>
<xsl:param name="l10n.gentext.use.xref.language" select="0"/>
<xsl:param name="label.from.part" select="'0'"/>
<xsl:param name="linenumbering.everyNth" select="'5'"/>
<xsl:param name="linenumbering.extension" select="'1'"/>
<xsl:param name="linenumbering.separator" select="' '"/>
<xsl:param name="linenumbering.width" select="'3'"/>
<xsl:param name="link.mailto.url"/>
<xsl:param name="make.graphic.viewport" select="1"/>
<xsl:param name="make.single.year.ranges" select="0"/>
<xsl:param name="make.valid.html" select="0"/>
<xsl:param name="make.year.ranges" select="0"/>
<xsl:param name="manifest" select="'HTML.manifest'"/>
<xsl:param name="manifest.in.base.dir" select="0"/>
<xsl:param name="menuchoice.menu.separator" select="'-&gt;'"/>
<xsl:param name="menuchoice.separator" select="'+'"/>
<xsl:param name="navig.graphics.extension" select="'.gif'"/>
<xsl:param name="navig.graphics" select="0"/>
<xsl:param name="navig.graphics.path">images/</xsl:param>
<xsl:param name="navig.showtitles">1</xsl:param>
<xsl:param name="nominal.image.depth" select="4 * $pixels.per.inch"/>
<xsl:param name="nominal.image.width" select="6 * $pixels.per.inch"/>
<xsl:param name="nominal.table.width" select="'6in'"/>
<xsl:param name="para.propagates.style" select="1"/>
<xsl:param name="part.autolabel" select="1"/>
<xsl:param name="phrase.propagates.style" select="1"/>
<xsl:param name="pixels.per.inch" select="90"/>
<xsl:param name="points.per.em" select="10"/>
<xsl:param name="preface.autolabel" select="0"/>
<xsl:param name="preferred.mediaobject.role"/>
<xsl:param name="punct.honorific" select="'.'"/>
<xsl:param name="qanda.defaultlabel">number</xsl:param>
<xsl:param name="qanda.inherit.numeration" select="1"/>
<xsl:param name="qandadiv.autolabel" select="1"/>
<xsl:param name="refentry.generate.name" select="1"/>
<xsl:param name="refentry.generate.title" select="0"/>
<xsl:param name="refentry.separator" select="'1'"/>
<xsl:param name="refentry.xref.manvolnum" select="1"/>
<xsl:param name="root.filename" select="'index'"/>
<xsl:param name="rootid" select="''"/>
<xsl:param name="runinhead.default.title.end.punct" select="'.'"/>
<xsl:param name="runinhead.title.end.punct" select="'.!?:'"/>
<xsl:param name="section.autolabel" select="1"/>
<xsl:param name="section.autolabel.max.depth" select="8"/>
<xsl:param name="section.label.includes.component.label" select="0"/>
<xsl:param name="segmentedlist.as.table" select="0"/>
<xsl:param name="shade.verbatim" select="0"/>
<xsl:attribute-set name="shade.verbatim.style">
  <xsl:attribute name="border">0</xsl:attribute>
  <xsl:attribute name="bgcolor">#E0E0E0</xsl:attribute>
</xsl:attribute-set>

<xsl:param name="show.comments">1</xsl:param>
<xsl:param name="show.revisionflag">0</xsl:param>
<xsl:param name="spacing.paras" select="'0'"/>
<xsl:param name="suppress.footer.navigation">0</xsl:param>
<xsl:param name="suppress.header.navigation">0</xsl:param>
<xsl:param name="suppress.navigation">0</xsl:param>
<xsl:param name="target.database.document" select="''"/>
<xsl:param name="targets.filename" select="'target.db'"/>
<xsl:param name="textdata.default.encoding" select="''"/>
<xsl:param name="tex.math.delims" select="'1'"/>
<xsl:param name="tex.math.file" select="'tex-math-equations.tex'"/>
<xsl:param name="tex.math.in.alt" select="''"/>
<xsl:param name="textinsert.extension" select="'1'"/>
<xsl:param name="ulink.target" select="'_top'"/>
<xsl:param name="use.embed.for.svg" select="0"/>
<xsl:param name="use.extensions" select="'0'"/>
<xsl:param name="use.id.as.filename" select="'0'"/>
<xsl:param name="use.role.as.xrefstyle" select="1"/>
<xsl:param name="use.role.for.mediaobject" select="1"/>
<xsl:param name="use.svg" select="1"/>
<xsl:param name="variablelist.as.table" select="0"/>
    
</xsl:stylesheet>
