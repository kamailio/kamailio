use OpenSER::Constants;

sub pseudo {
	my $m = shift;

	my $varstring = "User: \$rU - UA: \$ua";

	my $v = $m->pseudoVar($varstring);
	OpenSER::log(L_INFO, "pseudovar substitution demo; original: '$varstring' - substituted: '$v'\n");

	return 1;
}

