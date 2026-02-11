#include "ft_ping.h"

int	resolve_host(t_ping *ping, const char *host)
{
	struct addrinfo	hints;
	struct addrinfo	*res;
	int				ret;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;
	ret = getaddrinfo(host, NULL, &hints, &res);
	if (ret != 0)
		return (-1);
	ping->dest_addr = *(struct sockaddr_in *)res->ai_addr;
	inet_ntop(AF_INET, &ping->dest_addr.sin_addr,
		ping->ip_str, INET_ADDRSTRLEN);
	ping->hostname = strdup(host);
	if (!ping->hostname)
	{
		freeaddrinfo(res);
		return (-1);
	}
	freeaddrinfo(res);
	return (0);
}
