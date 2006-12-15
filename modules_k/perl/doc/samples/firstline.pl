use OpenSER;
use OpenSER::Constants;

sub firstline {
	my $m = shift;

	my $v = $m->getVersion();
	my $t = $m->getType();

	OpenSER::log(L_INFO, "type is $t; version is $v\n");

	if ($t == SIP_REQUEST) {
		OpenSER::log(L_INFO, "A request. Method is ".$m->getMethod()." to RURI ".$m->getRURI()."\n");
	} else {
		OpenSER::log(L_INFO, "A reply. status is ".$m->getStatus()." with reason ".$m->getReason()."\n");
	}
	
	return 1;
}

1;
