#ifndef stats_h
#define stats_h

struct stats_s {

	/* total/valid, received/sent, request/response */
	unsigned long 	ok_rx_rq,
			ok_rx_rs,
			ok_tx_rq,
			ok_tx_rs,
			total_rx,
			total_tx;
};


extern struct stats_s stats;

#endif
