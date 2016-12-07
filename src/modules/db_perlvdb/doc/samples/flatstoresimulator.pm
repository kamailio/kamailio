package flatstoresimulator;

sub init {}

sub insert {
	my $sub = shift;

	my $vals = shift;

	my $logline = "";
	open F, ">>/tmp/myfile";

	for my $v (@$vals) {
		$logline .= $v->data()." | ";
		#$logline .= "keys are ".$v->key().", type is ".$v->type().", data is ".$v->data()."\n");
	}

	$logline .= "\n";

	print F $logline;
	close F;

	return 0;
}
				

1;
