use Kamailio;
use Kamailio::Constants;

sub firstline {
	my $m = shift;

	my $v = $m->getVersion();
	my $t = $m->getType();

	Kamailio::log(L_INFO, "type is $t; version is $v\n");

	if ($t == SIP_REQUEST) {
		Kamailio::log(L_INFO, "A request. Method is ".$m->getMethod()." to RURI ".$m->getRURI()."\n");
	} else {
		Kamailio::log(L_INFO, "A reply. status is ".$m->getStatus()." with reason ".$m->getReason()."\n");
	}
	
	return 1;
}

1;
