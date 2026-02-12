#include "ft_ping.h"

int	recv_ping(t_ping *ping)
{
	uint8_t				buf[RECV_BUFSIZE];
	struct sockaddr_in	from;
	struct iovec		iov;
	struct msghdr		msg;
	ssize_t				bytes;
	struct ip			*ip_hdr;
	t_icmphdr			*icmp_hdr;
	int					ip_hdr_len;

	memset(&msg, 0, sizeof(msg));
	iov.iov_base = buf;
	iov.iov_len = sizeof(buf);
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	bytes = recvmsg(ping->sockfd, &msg, 0);
	if (bytes < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return (0);
		fprintf(stderr, "ft_ping: recvmsg: %s\n", strerror(errno));
		return (-1);
	}
	ip_hdr = (struct ip *)buf;
	ip_hdr_len = ip_hdr->ip_hl << 2;
	if (bytes < ip_hdr_len + PING_PKT_HDR_SZ)
		return (0);
	icmp_hdr = (t_icmphdr *)(buf + ip_hdr_len);
	if (ICMP_HDR_TYPE(icmp_hdr) == ICMP_ECHOREPLY
		&& ntohs(ICMP_HDR_ID(icmp_hdr)) == ping->ident)
	{
		print_echo_reply(ping, &msg, buf, bytes);
		return (0);
	}
	if (ICMP_HDR_TYPE(icmp_hdr) != ICMP_ECHO)
	{
		print_icmp_error(&from, ip_hdr, icmp_hdr,
			(int)bytes, ping->options);
	}
	return (0);
}
