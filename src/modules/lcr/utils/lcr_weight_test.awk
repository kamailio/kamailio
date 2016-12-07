# This script can be used to find out actual probabilities
# that correspond to a list of LCR gateway weights.

BEGIN {

    if (ARGC < 2) {
	printf("Usage: lcr_weight_test.php <list of weights (integers 1-254)>\n");
	exit;
    }

    iters = 100000;

    for (i = 1; i < ARGC; i++) {
	counts[i] = 0;
    }

    for (i = 1; i <= iters; i++) {
	for (j = 1; j < ARGC; j++) {
	    elem[j] = ARGV[j] * rshift(int(2147483647 * rand()), 8);
	};
	at = 1;
	max = elem[at];
	for (j = 2; j < ARGC; j++) {
	    if (elem[j] > max) {
		max = elem[j];
		at = j;
	    }
	}
	counts[at] = counts[at] + 1;
    }

    for (i = 1; i < ARGC; i++) {
	printf("weight %d probability %.4f\n", ARGV[i], counts[i]/iters);
    }
}
