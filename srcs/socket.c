#include "ft_ping.h"

extern int	g_dontroute;

static int	set_sock_options(t_ping *ping)
{
	int				one;
	struct timeval	tv;

	one = 1;
	if (setsockopt(ping->sockfd, SOL_SOCKET, SO_BROADCAST,
			&one, sizeof(one)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(SO_BROADCAST): %s\n",
			strerror(errno));
		return (-1);
	}
	if (setsockopt(ping->sockfd, IPPROTO_IP, IP_TTL,
			&ping->ttl, sizeof(ping->ttl)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(IP_TTL): %s\n",
			strerror(errno));
		return (-1);
	}
	if (ping->tos >= 0)
	{
		if (setsockopt(ping->sockfd, IPPROTO_IP, IP_TOS,
				&ping->tos, sizeof(ping->tos)) < 0)
		{
			fprintf(stderr, "ft_ping: setsockopt(IP_TOS): %s\n",
				strerror(errno));
			return (-1);
		}
	}
	if (g_dontroute)
	{
		one = 1;
		if (setsockopt(ping->sockfd, SOL_SOCKET, SO_DONTROUTE,
				&one, sizeof(one)) < 0)
		{
			fprintf(stderr, "ft_ping: setsockopt(SO_DONTROUTE): %s\n",
				strerror(errno));
			return (-1);
		}
	}
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (setsockopt(ping->sockfd, SOL_SOCKET, SO_RCVTIMEO,
			&tv, sizeof(tv)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(SO_RCVTIMEO): %s\n",
			strerror(errno));
		return (-1);
	}
	return (0);
}

int	set_ip_timestamp(t_ping *ping)
{
	unsigned char	rspace[MAX_IPOPTLEN];
	int				type;

	if (ping->ip_ts_type & SOPT_TSADDR)
		type = IPOPT_TS_TSANDADDR;
	else
		type = IPOPT_TS_TSONLY;
	memset(rspace, 0, sizeof(rspace));
	rspace[0] = IPOPT_TS;
	rspace[1] = sizeof(rspace);
	if (type != IPOPT_TS_TSONLY)
		rspace[1] -= sizeof(uint32_t);
	rspace[2] = 5;
	rspace[3] = type;
	if (setsockopt(ping->sockfd, IPPROTO_IP,
			IP_OPTIONS, rspace, rspace[1]) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt(IP_OPTIONS): %s\n",
			strerror(errno));
		return (-1);
	}
	return (0);
}

int	create_socket(t_ping *ping)
{
	ping->sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping->sockfd < 0)
	{
		fprintf(stderr, "ft_ping: socket: %s\n", strerror(errno));
		return (-1);
	}
	if (set_sock_options(ping) != 0)
	{
		close(ping->sockfd);
		ping->sockfd = -1;
		return (-1);
	}
	return (0);
}
