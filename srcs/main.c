#include "ft_ping.h"

volatile sig_atomic_t	g_stop = 0;

static void	init_ping(t_ping *ping)
{
	memset(ping, 0, sizeof(t_ping));
	ping->sockfd = -1;
	ping->ident = (uint16_t)(getpid() & 0xFFFF);
	ping->data_length = PING_PKT_DATA_SZ;
	ping->ttl = PING_DEFAULT_TTL;
	ping->tos = -1;
	ping->count = 0;
	ping->interval = PING_DEFAULT_INTERVAL;
	ping->timeout = -1;
	ping->linger = 10;
	ping->preload = 0;
	ping->pattern_len = MAXPATTERN;
	ping->pattern_set = false;
	ping->data_buffer = NULL;
	ping->stats.tmin = 999999999.0;
	ping->stats.tmax = 0.0;
}

void	print_usage(void)
{
	printf(
		"Usage: ft_ping [options] <destination>\n\n"
		"Options:\n"
		"  -c <count>    stop after sending <count> packets\n"
		"  -f            flood ping (root only)\n"
		"  -l <preload>  send <preload> packets as fast as possible\n"
		"  -n            numeric output only (no DNS)\n"
		"  -p <pattern>  fill payload with hex pattern\n"
		"  -r            bypass normal routing tables\n"
		"  -s <size>     set payload size in bytes\n"
		"  -T <tos>      set Type of Service\n"
		"  -v            verbose output\n"
		"  -w <timeout>  stop after <timeout> seconds\n"
		"  -W <linger>   seconds to wait for response\n"
		"  --ttl <N>     set time to live\n"
		"  --ip-timestamp <FLAG>  IP timestamp: tsonly or tsaddr\n"
		"  -?            display this help and exit\n");
}

enum {
	OPT_TTL = 256,
	OPT_IPTS,
};

static struct option	g_long_opts[] = {
	{"ttl",           required_argument, NULL, OPT_TTL},
	{"ip-timestamp",  required_argument, NULL, OPT_IPTS},
	{"help",          no_argument,       NULL, '?'},
	{NULL,            0,                 NULL, 0}
};

static void	handle_option(t_ping *ping, int opt)
{
	if (opt == 'c')
		ping->count = (size_t)parse_number(optarg, LONG_MAX, "count");
	else if (opt == 'f')
	{
		ping->options |= OPT_FLOOD;
		ping->interval = PING_FLOOD_INTERVAL;
	}
	else if (opt == 'l')
		ping->preload = (unsigned long)parse_number(optarg, INT_MAX,
				"preload");
	else if (opt == 'n')
		;
	else if (opt == 'p')
	{
		decode_pattern(optarg, &ping->pattern_len, ping->pattern);
		ping->pattern_set = true;
	}
	else if (opt == 'r')
		;
	else if (opt == 's')
		ping->data_length = (size_t)parse_number(optarg,
				PING_MAX_DATALEN, "size");
	else if (opt == 'T')
		ping->tos = (int)parse_number(optarg, 255, "TOS");
	else if (opt == 'v')
		ping->options |= OPT_VERBOSE;
	else if (opt == 'w')
		ping->timeout = (int)parse_number(optarg, INT_MAX, "timeout");
	else if (opt == 'W')
		ping->linger = (int)parse_number(optarg, INT_MAX, "linger");
	else if (opt == OPT_TTL)
		ping->ttl = (int)parse_number(optarg, 255, "TTL");
	else if (opt == OPT_IPTS)
	{
		ping->options |= OPT_IPTIMESTAMP;
		if (strcasecmp(optarg, "tsonly") == 0)
			ping->ip_ts_type = SOPT_TSONLY;
		else if (strcasecmp(optarg, "tsaddr") == 0)
			ping->ip_ts_type = SOPT_TSADDR;
		else
		{
			fprintf(stderr,
				"ft_ping: unsupported timestamp type: %s\n", optarg);
			exit(EXIT_FAILURE);
		}
	}
}

int	g_dontroute = 0;

void	parse_args(t_ping *ping, int argc, char **argv)
{
	int		opt;
	int		host_found;

	host_found = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argv, "c:fl:np:rs:T:vw:W:?",
				g_long_opts, NULL)) != -1)
	{
		if (opt == '?')
		{
			if (optopt && optopt != '?')
			{
				fprintf(stderr,
					"ft_ping: invalid option -- '%c'\n", optopt);
				print_usage();
				exit(EXIT_FAILURE);
			}
			print_usage();
			exit(EXIT_SUCCESS);
		}
		if (opt == 'r')
			g_dontroute = 1;
		else
			handle_option(ping, opt);
	}
	while (optind < argc)
	{
		if (host_found)
		{
			fprintf(stderr, "ft_ping: only one host allowed\n");
			exit(EXIT_FAILURE);
		}
		if (resolve_host(ping, argv[optind]) != 0)
		{
			fprintf(stderr, "ft_ping: unknown host %s\n", argv[optind]);
			exit(EXIT_FAILURE);
		}
		host_found = 1;
		optind++;
	}
	if (!host_found)
	{
		fprintf(stderr, "ft_ping: missing host operand\n");
		print_usage();
		exit(EXIT_FAILURE);
	}
}

static int	timeout_reached(t_ping *ping)
{
	struct timeval	now;
	long			elapsed;

	if (ping->timeout < 0)
		return (0);
	gettimeofday(&now, NULL);
	elapsed = (now.tv_sec - ping->start_time.tv_sec);
	return (elapsed >= ping->timeout);
}

static void	ping_loop(t_ping *ping)
{
	fd_set			readfds;
	struct timeval	tv;
	struct timeval	last_send;
	struct timeval	now;
	int				ret;
	size_t			i;
	long			elapsed_us;
	int				finishing;

	gettimeofday(&ping->start_time, NULL);
	print_header(ping);
	i = 0;
	while (i < ping->preload)
	{
		send_ping(ping);
		i++;
	}
	send_ping(ping);
	gettimeofday(&last_send, NULL);
	finishing = 0;
	while (!g_stop)
	{
		if (timeout_reached(ping))
			break ;
		FD_ZERO(&readfds);
		FD_SET(ping->sockfd, &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		ret = select(ping->sockfd + 1, &readfds, NULL, NULL, &tv);
		if (ret < 0)
		{
			if (errno == EINTR)
				continue ;
			break ;
		}
		if (ret > 0 && FD_ISSET(ping->sockfd, &readfds))
		{
			recv_ping(ping);
			if (ping->count
				&& (ping->num_recv - ping->num_rept) >= ping->count)
				break ;
		}
		gettimeofday(&now, NULL);
		elapsed_us = (now.tv_sec - last_send.tv_sec) * 1000000L
			+ (now.tv_usec - last_send.tv_usec);
		if (!g_stop && elapsed_us >= ping->interval)
		{
			if (!ping->count || ping->num_xmit < ping->count)
			{
				send_ping(ping);
				if (ping->options & OPT_FLOOD)
					putchar('.');
				gettimeofday(&last_send, NULL);
			}
			else if (finishing)
				break ;
			else
			{
				finishing = 1;
				ping->interval = ping->linger * 1000000L;
				gettimeofday(&last_send, NULL);
			}
		}
	}
}

static void	cleanup(t_ping *ping)
{
	if (ping->sockfd >= 0)
		close(ping->sockfd);
	if (ping->hostname)
		free(ping->hostname);
	if (ping->data_buffer)
		free(ping->data_buffer);
}

int	main(int argc, char **argv)
{
	t_ping	ping;

	init_ping(&ping);
	parse_args(&ping, argc, argv);
	if (getuid() != 0)
	{
		fprintf(stderr, "ft_ping: socket: Operation not permitted\n");
		return (EXIT_FAILURE);
	}
	if (create_socket(&ping) != 0)
	{
		cleanup(&ping);
		return (EXIT_FAILURE);
	}
	if (ping.options & OPT_IPTIMESTAMP)
	{
		if (set_ip_timestamp(&ping) != 0)
		{
			cleanup(&ping);
			return (EXIT_FAILURE);
		}
	}
	if (setuid(getuid()) != 0)
	{
		fprintf(stderr, "ft_ping: setuid: %s\n", strerror(errno));
		cleanup(&ping);
		return (EXIT_FAILURE);
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	setup_signals();
	init_data_buffer(&ping);
	ping_loop(&ping);
	print_statistics(&ping);
	cleanup(&ping);
	return (ping.num_recv == 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
