#include "ft_ping.h"

void	print_header(t_ping *ping)
{
	printf("PING %s (%s): %zu data bytes",
		ping->hostname, ping->ip_str, ping->data_length);
	if (ping->options & OPT_VERBOSE)
		printf(", id 0x%04x = %u", ping->ident, ping->ident);
	printf("\n");
}

void	print_statistics(t_ping *ping)
{
	double	total;
	double	avg;

	printf("--- %s ping statistics ---\n", ping->hostname);
	printf("%zu packets transmitted, ", ping->num_xmit);
	printf("%zu packets received, ", ping->num_recv);
	if (ping->num_rept)
		printf("+%zu duplicates, ", ping->num_rept);
	if (ping->num_xmit)
	{
		if (ping->num_recv > ping->num_xmit)
			printf("-- somebody is printing forged packets!");
		else
			printf("%d%% packet loss",
				(int)(((ping->num_xmit - ping->num_recv) * 100)
					/ ping->num_xmit));
	}
	printf("\n");
	if (ping->num_recv && ping->data_length >= sizeof(struct timeval))
	{
		total = (double)ping->num_recv;
		avg = ping->stats.tsum / total;
		printf("round-trip min/avg/max/stddev = "
			"%.3f/%.3f/%.3f/%.3f ms\n",
			ping->stats.tmin, avg, ping->stats.tmax,
			calc_stddev(ping->stats.tsumsq, ping->stats.tsum, total));
	}
}
