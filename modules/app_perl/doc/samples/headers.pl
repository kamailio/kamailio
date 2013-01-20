use Kamailio qw ( log );
use Kamailio::Constants;

# Header demos

sub headernames {
	my $m = shift;

	my @h = $m->getHeaderNames();
	foreach (@h) {
		my $f = $_;
		log(L_INFO, "header $f\n");
	}

	return 1;
}


sub someheaders {
	my $m = shift;

	foreach ( qw ( To WWW-Contact )) {
		my $srch = $_;
		my @h = $m->getHeader($srch);
		foreach (@h) {
			my $f = $_;
			log(L_INFO, "$srch retrieved from array is $f\n");
		}
		log(L_INFO, "$srch as array is @h\n");

		my $scalarto = $m->getHeader($srch);
		log(L_INFO, "$srch as scalar is $scalarto\n");
	}

	return 1;
}

