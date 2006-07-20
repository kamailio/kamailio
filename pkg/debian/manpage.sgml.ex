<!doctype refentry PUBLIC "-//OASIS//DTD DocBook V4.1//EN" [

<!-- Process this file with docbook-to-man to generate an nroff manual
     page: `docbook-to-man manpage.sgml > manpage.1'.  You may view
     the manual page with: `docbook-to-man manpage.sgml | nroff -man |
     less'.  A typical entry in a Makefile or Makefile.am is:

manpage.1: manpage.sgml
	docbook-to-man $< > $@

    
	The docbook-to-man binary is found in the docbook-to-man package.
	Please remember that if you create the nroff version in one of the
	debian/rules file targets (such as build), you will need to include
	docbook-to-man in your Build-Depends control field.

  -->

  <!-- Fill in your name for FIRSTNAME and SURNAME. -->
  <!ENTITY dhfirstname "<firstname>FIRSTNAME</firstname>">
  <!ENTITY dhsurname   "<surname>SURNAME</surname>">
  <!-- Please adjust the date whenever revising the manpage. -->
  <!ENTITY dhdate      "<date>July 16, 2002</date>">
  <!-- SECTION should be 1-8, maybe w/ subsection other parameters are
       allowed: see man(7), man(1). -->
  <!ENTITY dhsection   "<manvolnum>SECTION</manvolnum>">
  <!ENTITY dhemail     "<email>pelinescu-onciul@fokus.gmd.de</email>">
  <!ENTITY dhusername  "Andrei Pelinescu-Onciul">
  <!ENTITY dhucpackage "<refentrytitle>SER-0.8.7</refentrytitle>">
  <!ENTITY dhpackage   "ser-0.8.7">

  <!ENTITY debian      "<productname>Debian</productname>">
  <!ENTITY gnu         "<acronym>GNU</acronym>">
]>

<refentry>
  <refentryinfo>
    <address>
      &dhemail;
    </address>
    <author>
      &dhfirstname;
      &dhsurname;
    </author>
    <copyright>
      <year>2002</year>
      <holder>&dhusername;</holder>
    </copyright>
    &dhdate;
  </refentryinfo>
  <refmeta>
    &dhucpackage;

    &dhsection;
  </refmeta>
  <refnamediv>
    <refname>&dhpackage;</refname>

    <refpurpose>program to do something</refpurpose>
  </refnamediv>
  <refsynopsisdiv>
    <cmdsynopsis>
      <command>&dhpackage;</command>

      <arg><option>-e <replaceable>this</replaceable></option></arg>

      <arg><option>--example <replaceable>that</replaceable></option></arg>
    </cmdsynopsis>
  </refsynopsisdiv>
  <refsect1>
    <title>DESCRIPTION</title>

    <para>This manual page documents briefly the
      <command>&dhpackage;</command> and <command>bar</command>
      commands.</para>

    <para>This manual page was written for the &debian; distribution
      because the original program does not have a manual page.
      Instead, it has documentation in the &gnu;
      <application>Info</application> format; see below.</para>

    <para><command>&dhpackage;</command> is a program that...</para>

  </refsect1>
  <refsect1>
    <title>OPTIONS</title>

    <para>These programs follow the usual GNU command line syntax,
      with long options starting with two dashes (`-').  A summary of
      options is included below.  For a complete description, see the
      <application>Info</application> files.</para>

    <variablelist>
      <varlistentry>
        <term><option>-h</option>
          <option>--help</option>
        </term>
        <listitem>
          <para>Show summary of options.</para>
        </listitem>
      </varlistentry>
      <varlistentry>
        <term><option>-v</option>
          <option>--version</option>
        </term>
        <listitem>
          <para>Show version of program.</para>
        </listitem>
      </varlistentry>
    </variablelist>
  </refsect1>
  <refsect1>
    <title>SEE ALSO</title>

    <para>bar (1), baz (1).</para>

    <para>The programs are documented fully by <citetitle>The Rise and
      Fall of a Fooish Bar</citetitle> available via the
      <application>Info</application> system.</para>
  </refsect1>
  <refsect1>
    <title>AUTHOR</title>

    <para>This manual page was written by &dhusername; &dhemail; for
      the &debian; system (but may be used by others).  Permission is
      granted to copy, distribute and/or modify this document under
      the terms of the <acronym>GNU</acronym> Free Documentation
      License, Version 1.1 or any later version published by the Free
      Software Foundation; with no Invariant Sections, no Front-Cover
      Texts and no Back-Cover Texts.</para>

  </refsect1>
</refentry>

<!-- Keep this comment at the end of the file
Local variables:
mode: sgml
sgml-omittag:t
sgml-shorttag:t
sgml-minimize-attributes:nil
sgml-always-quote-attributes:t
sgml-indent-step:2
sgml-indent-data:t
sgml-parent-document:nil
sgml-default-dtd-file:nil
sgml-exposed-tags:nil
sgml-local-catalogs:nil
sgml-local-ecat-files:nil
End:
-->


