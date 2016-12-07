use Kamailio::Constants;
use Kamailio::Utils::PhoneNumbers;

# Demonstrate how the PhoneNumbers class works.
sub canonical {
	my $m = shift;

	if ($m->getMethod() eq "INVITE") {
		my $p = new Kamailio::Utils::PhoneNumbers(publicAccessPrefix => "0",
		                                         internationalPrefix => "+",
							 longDistancePrefix => "0",
							 countryCode => "49",
							 areaCode => "761",
							 pbxCode => "456842");

		my $u = $m->getRURI();

		if ($u =~ m/(.*)sip:([+0-9]+)\@(.*)/) {
			my $c = $p->canonicalForm($2);
			Kamailio::log(L_INFO, "canonical number in '$u' is '$c'\n");
		} else {
			Kamailio::log(L_INFO, "Not a POTS number.\n");
		}
	}
	return 1;
}

sub dialnr {
	my $m = shift;

	if ($m->getMethod() eq "INVITE") {
		my $p = new Kamailio::Utils::PhoneNumbers(publicAccessPrefix => "0",
		                                         internationalPrefix => "+",
							 longDistancePrefix => "0",
							 countryCode => "49",
							 areaCode => "761",
							 pbxCode => "456842");

		my $u = $m->getRURI();

		if ($u =~ m/(.*)sip:([+0-9]+)\@(.*)/) {
			my $c = $p->dialNumber($2);
			Kamailio::log(L_INFO, "dial number in '$u' is '$c'\n");
		} else {
			Kamailio::log(L_INFO, "Not a POTS number.\n");
		}
	}
	return 1;
}
