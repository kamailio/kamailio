#
# $Id$
#
# Perl module for Kamailio
#
# Copyright (C) 2006 Collax GmbH
#		     (Bastian Friedrich <bastian.friedrich@collax.com>)
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

=head1 Kamailio::LDAPUtils::LDAPConnection

Kamailio::LDAPUtils::LDAPConnection - Perl module to perform simple LDAP queries.

OO-Style interface:

  use Kamailio::LDAPUtils::LDAPConnection;
  my $ldap = new Kamailio::LDAPUtils::LDAPConnection;
  my @rows = $ldap-search("uid=andi","ou=people,ou=coreworks,ou=de");

Procedural interface:

  use Kamailio::LDAPUtils::LDAPConnection;
  my @rows = $ldap->search(
  	new Kamailio::LDAPUtils::LDAPConfig(), "uid=andi","ou=people,ou=coreworks,ou=de");

This perl module offers a somewhat simplified interface to the C<Net::LDAP>
functionality. It is intended for cases where just a few attributes should
be retrieved without the overhead of the full featured C<Net::LDAP>.

=cut

package Kamailio::LDAPUtils::LDAPConnection;

use Kamailio::LDAPUtils::LDAPConf;
use Net::LDAP;
use Authen::SASL;
use UNIVERSAL qw( isa );

my $ldap_singleton = undef;


=head2 Constructor new( [config, [authenticated]] )

Set up a new LDAP connection.

The first argument, when given, should be a hash reference pointing
to to the connection parameters, possibly an C<Kamailio::LDAPUtils::LDAPConfig>
object. This argument may be C<undef> in which case a new (default)
C<Kamailio::LDAPUtils::LDAPConfig> object is used.

When the optional second argument is a true value, the connection
will be authenticated. Otherwise an anonymous bind is done.

On success, a new C<LDAPConnection> object is returned, otherwise
the result is C<undef>.

=cut
sub new {
    my $class    = shift;
    my $conf     = shift;
    my $doauth   = shift;

    if( ! defined( $conf ) ) {
      $conf = new Kamailio::LDAPUtils::LDAPConf();
    }

    #print STDERR "new ldap checks\n";
    my $self = {
    	conf => $conf,
    };

    bless $self, $class;

    #
    # connect to ldap server and do an anonymous bind
    #
    $self->{'ldap'} = Net::LDAP->new( $self->conf->uri() );
    
    # return undef if ldap connection failed
    if( ! $self->ldap ) {
       print STDERR "Could not connect to LDAP server.\n";
       return undef;
    }

    if( ! $doauth ) {
      $self->ldap->bind ;    # an anonymous bind
    } else {

      if( $self->conf->uri && ( $self->conf->uri =~ m/^ldapi:/ ) ) {

        my $sasl = new Authen::SASL( mechanism => 'EXTERNAL ANONYMOUS');
	$self->ldap->bind($self->conf->binddn, sasl => $sasl );

      } else {
	# only simple binds for now.
	$self->ldap->bind($self->conf->binddn,
				password => $self->conf->bindpw);
      }

    }
    
    return $self;
};

sub DESTROY {
    my $self = shift;
    if( $self->ldap ) {
        $self->ldap->unbind();
        delete $self->{'ldap'};
    }
}

sub conf { return $_[0]->{'conf'}; }
sub ldap { return $_[0]->{'ldap'}; }

=head2 Function/Method search( conf, filter, base, [requested_attributes ...])

perform an ldap search, return the dn of the first matching
directory entry, unless a specific attribute has been requested,
in wich case the values(s) fot this attribute are returned.

When the first argument (conf) is a C<Kamailio::LDAPUtils::LDAPConnection>, it
will be used to perform the queries. You can pass the first argument 
implicitly by using the "method" syntax.

Otherwise the C<conf> argument should be a reference to a hash 
containing the connection setup parameters as contained in a 
C<Kamailio::LDAPUtils::LDAPConf> object. In this mode, the
C<Kamailio::LDAPUtils::LDAPConnection> from previous queries will be reused.

=head3 Arguments:

=over 12

=item conf

configuration object, used to find host,port,suffix and use_ldap_checks

=item filter

ldap search filter, eg '(mail=some@domain)' 

=item base

search base for this query. If undef use default suffix,
concat base with default suffix if the last char is a ','

=item requested_attributes

retrieve the given attributes instead of the dn from the ldap directory. 

=back

=head3 Result:

Without any specific C<requested_attributes>, return the dn of all matching
entries in the LDAP directory.

When some C<requested_attributes> are given, return an array with those
attibutes. When multiple entries match the query, the attribute lists
are concatenated.

=cut
sub search {

    my $conf = shift;

    my $ldap = undef;

    if( isa($conf ,"Kamailio::LDAPUtils::LDAPConnection") ) {
      $ldap = $conf;
    } else {
      if( ! $ldap_singleton ) {
        $ldap_singleton = new Kamailio::LDAPUtils::LDAPConnection($conf);
      }
      return undef unless $ldap_singleton;
      $ldap = $ldap_singleton;
    }


    my $filter         = shift;
    my $base           = shift;
    my @requested_attr = @_ ;

    if( ! $base ) {
        $base = $ldap->conf->base;
    } elsif ( substr($base,-1) eq ',' ) {
        $base .= $ldap->conf->base;
    }

    my @search_extra = ();
    if( @requested_attr ) {
        push @search_extra , 'attrs' => [ @requested_attr ];
    }

    my $mesg = $ldap->ldap->search (
                               base      => $base,
                               filter    => $filter,
                               #sizelimit => 100 ,
                               @search_extra
                             );


    if( $mesg->is_error ) { warn $mesg->error; return undef };

    my @result=();
    my $max_entries=$mesg->count;
    my $i;
    for($i = 0 ; $i < $max_entries ; $i++) {
        my $entry = $mesg->entry($i);
        foreach my $attr (@requested_attr) {
            push @result, $entry->get_value($attr);
        } 
    }
 
    return @result;
}

1;

