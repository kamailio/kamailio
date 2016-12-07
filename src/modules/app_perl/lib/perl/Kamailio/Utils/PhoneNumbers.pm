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


=head1 Kamailio::Utils::PhoneNumbers

Kamailio::Utils::PhoneNumbers - Functions for canonical forms of phone numbers.

 use Kamailio::Utils::PhoneNumbers;

 my $phonenumbers = new Kamailio::Utils::PhoneNumbers(
      publicAccessPrefix => "0",
      internationalPrefix => "+",
      longDistancePrefix => "0",
      areaCode => "761",
      pbxCode => "456842",
      countryCode => "49"
    );
					
 $canonical = $phonenumbers->canonicalForm("07612034567");
 $number    = $phonenumbers->dialNumber("+497612034567");

A telphone number starting with a plus sign and containing all dial prefixes 
is in canonical form. This is usally not the number to dial at any location,
so the dialing number depends on the context of the user/system.

The idea to canonicalize numbers were taken from hylafax.

Example: +497614514829 is the canonical form of my phone number, 829 is the 
number to dial at Pyramid, 4514829 is the dialing number from Freiburg are and
so on.

To canonicalize any number, we strip off any dial prefix we find and 
then add the prefixes for the location. So, when the user enters the
number 04514829 in context pyramid, we remove the publicAccessPrefix 
(at Pyramid this is 0) and the  pbxPrefix (4514 here). The result 
is 829. Then we add all the general dial prefixes - 49 (country) 761 (area) 
4514 (pbx) and 829, the number itself => +497614514829 

To get the dialing number from a canonical phone number, we substract all
general prefixes until we have something 

As said before, the interpretation of a phone number depends on the context of
the location. For the functions in this package, the context is created through
the C<new> operator.

The following fields should be set:

      'longDistancePrefix' 
      'areaCode'
      'pbxCode' 
      'internationalPrefix'
      'publicAccessPrefix'
      'countryCode'

This module exports the following functions when C<use>ed:

=cut

package Kamailio::Utils::PhoneNumbers;

use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(
	     canonicalForm
	     dialNumber
	    );

=head2 new(publicAccessPrefix,internationalPrefix,longDistancePrefix,countryCode,areaCode,pbxCode)

The new operator returns an object of this type and sets its locational
context according to the passed parameters. See
L<Kamailio::Utils::PhoneNumbers> above.

=cut

sub new {
	my $class = shift;
	my %setup = @_;

	my $self = {
		PAPrefix => %setup->{'publicAccessPrefix'},
		IDPrefix => %setup->{'internationalPrefix'},
		LDPrefix => %setup->{'longDistancePrefix'},

		Country => %setup->{'countryCode'},
		Area => %setup->{'areaCode'},
		PBX => %setup->{'pbxCode'}
	};

	bless $self, $class;

	return $self;

}

=head2 canonicalForm( number [, context] )

Convert a phone number (given as first argument) into its canonical form. When
no context is passed in as the second argument, the default context from the
systems configuration file is used.

=cut
sub canonicalForm {
  my $self = shift;
  my $number  = shift;

  $number =~ s/[^+0-9]*//g;                         # strip white space etc.

  if( $number =~ m/^[^+]/ ) {
    $number =~ s/^$self->{'PAPrefix'}$self->{'IDPrefix'}/+/;                          # replace int. dialing code
    if( $number =~ m/^[^+]/ ) {
      $number =~ s/^$self->{'PAPrefix'}$->{'LDPrefix'}/+$self->{'Country'}/;                # long distance number
      if( $number =~ m/^[^+]/ ) {
        $number =~ s/^$self->{'PAPrefix'}/+$self->{'Country'}$self->{'Area'}/;                  # local number
        if( $number =~ m/^[^+]/ ) {
           $number =~ s/^(.*)/+$self->{'Country'}$self->{'Area'}$self->{PBX}$1/;              # else cononicalize
	}
      }
    }
  }
  return $number;
}

=head2 dialNumber( number [, context] )

Convert a canonical phone number (given in the first argument) into a number to
to dial.  WHen no context is given in the second argument, a default context
from the systems configuration is used.

=cut
sub dialNumber {
	my $self = shift;
	my $number  = shift;

	$number =~ s/[^+0-9]+//g;											# strip syntactical sugar.
	$number =~ s/^$self->{'PAPrefix'}$self->{'IDPrefix'}$self->{'Country'}$self->{'Area'}$self->{'PBX'}//;		# inhouse phone call
	$number =~ s/^$self->{'PAPrefix'}$self->{'IDPrefix'}$self->{'Country'}$self->{'Area'}/$self->{'PAPrefix'}/;     # local phone call
	$number =~ s/^$self->{'PAPrefix'}$self->{'LDPrefix'}$self->{'Area'}$self->{'PBX'}//;				# inhouse phone call
	$number =~ s/^$self->{'PAPrefix'}$self->{'LDPrefix'}$self->{'Area'}/$self->{'PAPrefix'}/;			# local phone call
	$number =~ s/^$self->{'PAPrefix'}$self->{'IDPrefix'}$self->{'Country'}/$self->{'PAPrefix'}$self->{'LDPrefix'}/; # long distance call
	$number =~ s/^$self->{'PAPrefix'}$self->{'PBX'}//;								# inhouse call
	$number =~ s/^[+]$self->{'Country'}$self->{'Area'}$self->{'PBX'}//;						# inhouse phone call
	$number =~ s/^[+]$self->{'Country'}$self->{'Area'}/$self->{'PAPrefix'}/;					# local phone call
	$number =~ s/^[+]$self->{'Country'}/$self->{'PAPrefix'}$self->{'LDPrefix'}/;					# long distance call
	$number =~ s/^[+]/$self->{'PAPrefix'}$self->{'IDPrefix'}/;							# international call

	return $number;
}

1;
