#include "ft_ping.h"

void	init_data_buffer(t_ping *ping)
{
	size_t	i;
	size_t	buf_sz;

	if (ping->data_length == 0)
		return ;
	buf_sz = ping->data_length;
	ping->data_buffer = malloc(buf_sz);
	if (!ping->data_buffer)
	{
		fprintf(stderr, "ft_ping: out of memory\n");
		exit(EXIT_FAILURE);
	}
	if (ping->pattern_set)
	{
		i = 0;
		while (i < buf_sz)
		{
			ping->data_buffer[i] = ping->pattern[i % ping->pattern_len];
			i++;
		}
	}
	else
	{
		i = 0;
		while (i < buf_sz)
		{
			ping->data_buffer[i] = (unsigned char)(i & 0xFF);
			i++;
		}
	}
}

int	send_ping(t_ping *ping)
{
	uint8_t		*packet;
	t_icmphdr	*icmp_hdr;
	struct timeval	tv;
	ssize_t		bytes;
	size_t		pkt_sz;
	size_t		off;

	pkt_sz = PING_PKT_HDR_SZ + ping->data_length;
	packet = malloc(pkt_sz);
	if (!packet)
	{
		fprintf(stderr, "ft_ping: out of memory\n");
		return (-1);
	}
	memset(packet, 0, pkt_sz);
	icmp_hdr = (t_icmphdr *)packet;
	ICMP_HDR_TYPE(icmp_hdr) = ICMP_ECHO;
	ICMP_HDR_CODE(icmp_hdr) = 0;
	ICMP_HDR_ID(icmp_hdr) = htons(ping->ident);
	ICMP_HDR_SEQ(icmp_hdr) = htons(ping->seq);
	off = 0;
	if (ping->data_length >= sizeof(struct timeval))
	{
		gettimeofday(&tv, NULL);
		memcpy(packet + PING_PKT_HDR_SZ, &tv, sizeof(tv));
		off = sizeof(tv);
	}
	if (ping->data_buffer && ping->data_length > off)
		memcpy(packet + PING_PKT_HDR_SZ + off,
			ping->data_buffer + off, ping->data_length - off);
	ICMP_HDR_CKSUM(icmp_hdr) = 0;
	ICMP_HDR_CKSUM(icmp_hdr) = checksum(packet, pkt_sz);
	bytes = sendto(ping->sockfd, packet, pkt_sz, 0,
			(struct sockaddr *)&ping->dest_addr,
			sizeof(ping->dest_addr));
	free(packet);
	if (bytes < 0)
	{
		fprintf(stderr, "ft_ping: sendto: %s\n", strerror(errno));
		return (-1);
	}
	ping->num_xmit++;
	ping->seq++;
	return (0);
}
