use Kamailio qw ( log );
use Kamailio::Constants;

sub setflag{
	my $m = shift;

	$m->setFlag(FL_GREEN);
	log(L_INFO, "Setting flag GREEN...");

	return 1;
}

sub readflag {
	my $m = shift;

	if ($m->isFlagSet(FL_GREEN)) {
		log(L_INFO, "flag GREEN set");
	} else {
		log(L_INFO, "flag GREEN not set");
	}

	if ($m->isFlagSet(FL_MAGENTA)) {
		log(L_INFO, "flag MAGENTA set");
	} else {
		log(L_INFO, "flag MAGENTA not set");
	}

	return 1;
}
