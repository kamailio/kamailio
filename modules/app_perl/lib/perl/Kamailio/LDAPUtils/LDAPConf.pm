#
# $Id$
#
# Perl module for Kamailio
#
# Copyright (C) 2006 Collax GmbH 
#                    (Bastian Friedrich <bastian.friedrich@collax.com>)
#
# This file is part of Kamailio, a free SIP server.
#
# Kamailio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version
#
# Kamailio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

# This file was kindly donated by Collax GmbH

=head1 Kamailio::LDAPUtils::LDAPConf

Kamailio::LDAPUtils::LDAPConf - Read openldap config from standard config files.

 use Kamailio::LDAPUtils::LDAPConf;
 my $conf = new Kamailio::LDAPUtils::LDAPConf();

This module may be used to retrieve the global LDAP configuration as
used by other LDAP software, such as C<nsswitch.ldap> and C<pam-ldap>.
The configuration is usualy stored in C</etc/openldap/ldap.conf> 

When used from an account with sufficient privilegs (e.g. root), the
ldap manager passwort is also retrieved.

=cut
package Kamailio::LDAPUtils::LDAPConf;

my $def_ldap_path = "/etc/openldap" ;
my $def_conf      = "ldap.conf";
my $def_secret    = "ldap.secret";

=head2 Constructor new()

Returns a new, initialized C<Kamailio::LDAPUtils::LDAPConf> object.

=cut
sub new {
  my $class = shift;

  my $self  = _init( $def_ldap_path . "/" . $def_conf , 
  		     $def_ldap_path . "/" . $def_secret);

  if( ! $self ) { return undef; } ## can happen during customizing

  bless $self , $class;

  return $self;
}


sub _init {
  my $conf_file   = shift;
  my $secret_file = shift;

  my $result = {};

  if( open(LDAPCONF,"<$conf_file") ) {
    while(<LDAPCONF>) {
      chomp;
      s/#.*$//;
      if( m/^\s*$/ ) { next; }
      my ($var,$val) = split(" ",$_,2);
      
      $result->{lc($var)} = $val;

    }
    close(LDAPCONF);
  } else {
    return undef;
  }

  if( -r $secret_file ) {
    if( open(SECRET,"<$secret_file") ) {
      my $secret = <SECRET>;
      chomp $secret;
      $result->{'rootbindpw'} = $secret;
      close(SECRET);
    }
  }

  return $result;
}

=head2 Method base()

Returns the servers base-dn to use when doing queries.

=cut
sub base { return $_[0]->{'base'}; }

=head2 Method host()

Returns the ldap host to contact.

=cut
sub host { return $_[0]->{'host'}; }


=head2 Method port()

Returns the ldap servers port.

=cut
sub port { return $_[0]->{'port'}; }

=head2 Method uri()

Returns an uri to contact the ldap server. When there is no ldap_uri in
the configuration file, an C<ldap:> uri is constucted from host and port.

=cut
sub uri {
    my $self = shift;

    if( $self->{'ldap_uri'} ) { return $self->{'ldap_uri'}; }

    return "ldap://" . $self->host . ":" . $self->port ;

}

=head2 Method rootbindpw()

Returns the ldap "root" password.

Note that the C<rootbindpw> is only available when the current account has
sufficient privilegs to access C</etc/openldap/ldap.secret>.

=cut
sub rootbindpw { return $_[0]->{'rootbindpw'}; }

=head2 Method rootbinddn()

Returns the DN to use for "root"-access to the ldap server.

=cut
sub rootbinddn { return $_[0]->{'rootbinddn'}; }

=head2 Method binddn()

Returns the DN to use for authentication to the ldap server. When no
bind dn has been specified in the configuration file, returns the
C<rootbinddn>.

=cut
sub binddn { 
    my $self = shift;

    if( $self->{'binddn'} ) { return $self->{'binddn'}; }
    return $self->rootbinddn;
}

=head2 Method bindpw()

Returns the password to use for authentication to the ldap server. When no
bind password has been specified, returns the C<rootbindpw> if any.

=cut
sub bindpw { 
    my $self = shift;

    if( $self->{'bindpw'} ) { return $self->{'bindpw'}; }
    return $self->rootbindpw;
}




1;

