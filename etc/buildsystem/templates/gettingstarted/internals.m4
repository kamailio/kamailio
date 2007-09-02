dnl Use this file to define global macros to be included when generating GETTINGSTARTED files.
dnl These macros are not intended to be used by users.
dnl
dnl The config.m4 file should have defined either GS_NAT_RTPPROXY or GS_NAT_MEDIAPROXY if GS_HANDLINGNAT is defined
dnl If not, rtpproxy is default
ifdef(`GS_NAT_RTPPROXY',`',
`ifdef(`GS_NAT_MEDIAPROXY',`',
`define(`GS_NAT_RTPPROXY', `yes')dnl
')dnl
')dnl
dnl
dnl When we generate XML, we want to turn off all separators
ifdef(`XMLOUT',
`undefine(`INCLUDE_SEPARATORS')dnl
')dnl
dnl 
dnl These macros are used for creation of annotated docbook of each 
dnl Getting Started config file. They are included here globally as they can be
dnl useful for others who want to annotate their code to document their ser.cfg.
dnl
dnl NOTE!!! Before you use these, you need to use changequote({{,}}) and changequote() to change back
dnl         the quote characters (and thus use {{ instead of `and }} instead of ')
dnl
dnl Use PARA within ANNOTATE to start a new paragraph.
dnl 
define(`PARA',`<\para>
<para>')dnl
dnl
dnl Usage: ANNOTATE(`Text for annotation in xml',`#Short text for cfg file', `code')
dnl
dnl First the one with regular escapes
dnl define(`ANNOTATE',`ifdef(`XMLOUT',`<programlisting linenumbering="numbered" continuation="continues">
dnl $3
dnl </programlisting>
dnl <para>
dnl $1
dnl </para>',`$2
dnl $3')')dnl
dnl Then the one with {{ and }}
define(`ANNOTATE',`ifdef({{XMLOUT}},{{<programlisting linenumbering="numbered" continuation="continues">
$3
</programlisting>
<para>
$1
</para>}},{{ifelse(len({{$2}}), {{0}}, {{dnl}}, {{$2}})
$3}})')dnl
dnl
dnl Used for the first annotation in each file.
dnl Note how the definition uses {{ }} instead of regular quotes.
dnl This is because quotes are changed in this package.
dnl
define(`ANNOTATEHEAD',`ifdef({{XMLOUT}},{{<programlisting linenumbering="numbered">
$3
</programlisting>
<para>
$1
</para>}},{{
ifelse(len({{$2}}), {{0}}, {{dnl}}, {{$2}})
$3}})')dnl
