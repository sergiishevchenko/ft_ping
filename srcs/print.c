#include "ft_ping.h"

static void	print_ip_opt(struct ip *ip, int hlen);

static void	update_timing(t_ping *ping, uint8_t *buf, int ip_hdr_len,
				int datalen, double *triptime, bool *timing)
{
	struct timeval	tv_send;
	struct timeval	tv_recv;

	*timing = false;
	*triptime = 0.0;
	if (datalen > PING_PKT_HDR_SZ
		&& (size_t)(datalen - PING_PKT_HDR_SZ) >= sizeof(struct timeval))
	{
		*timing = true;
		memcpy(&tv_send, buf + ip_hdr_len + PING_PKT_HDR_SZ,
			sizeof(tv_send));
		gettimeofday(&tv_recv, NULL);
		*triptime = (double)(tv_recv.tv_sec - tv_send.tv_sec) * 1000.0
			+ (double)(tv_recv.tv_usec - tv_send.tv_usec) / 1000.0;
		ping->stats.tsum += *triptime;
		ping->stats.tsumsq += *triptime * *triptime;
		if (*triptime < ping->stats.tmin)
			ping->stats.tmin = *triptime;
		if (*triptime > ping->stats.tmax)
			ping->stats.tmax = *triptime;
	}
}

static int	check_duplicate(t_ping *ping, uint16_t seq)
{
	int	idx;
	int	bit;

	idx = (seq % (PING_CKTAB_SZ * 8)) >> 3;
	bit = 1 << ((seq % (PING_CKTAB_SZ * 8)) & 0x07);
	if (ping->recv_table[idx] & bit)
		return (1);
	ping->recv_table[idx] |= bit;
	return (0);
}

void	print_echo_reply(t_ping *ping, struct msghdr *msg,
			uint8_t *buf, ssize_t bytes_recv)
{
	struct ip		*ip_hdr;
	t_icmphdr		*icmp_hdr;
	int				ip_hdr_len;
	int				datalen;
	double			triptime;
	bool			timing;
	int				dupflag;

	ip_hdr = (struct ip *)buf;
	ip_hdr_len = ip_hdr->ip_hl << 2;
	icmp_hdr = (t_icmphdr *)(buf + ip_hdr_len);
	datalen = (int)bytes_recv - ip_hdr_len;
	update_timing(ping, buf, ip_hdr_len, datalen, &triptime, &timing);
	dupflag = check_duplicate(ping, ntohs(ICMP_HDR_SEQ(icmp_hdr)));
	ping->num_recv++;
	if (dupflag)
		ping->num_rept++;
	if (ping->options & OPT_FLOOD)
	{
		putchar('\b');
		return ;
	}
	printf("%d bytes from %s: icmp_seq=%u",
		datalen,
		inet_ntoa(((struct sockaddr_in *)msg->msg_name)->sin_addr),
		ntohs(ICMP_HDR_SEQ(icmp_hdr)));
	printf(" ttl=%d", ip_hdr->ip_ttl);
	if (timing)
		printf(" time=%.3f ms", triptime);
	if (dupflag)
		printf(" (DUP!)");
	print_ip_opt(ip_hdr, ip_hdr_len);
	printf("\n");
}

static const char	*get_dest_unreach_str(int code)
{
	static const char	*msgs[] = {
		"Destination Net Unreachable",
		"Destination Host Unreachable",
		"Destination Protocol Unreachable",
		"Destination Port Unreachable",
		"Fragmentation needed and DF set",
		"Source Route Failed",
		"Network Unknown",
		"Host Unknown",
		"Host Isolated",
		"Destination Network Unreachable At This TOS",
		"Destination Host Unreachable At This TOS",
		"Packet Filtered",
		"Precedence Violation",
		"Precedence Cutoff"
	};

	if (code >= 0 && code < (int)(sizeof(msgs) / sizeof(msgs[0])))
		return (msgs[code]);
	return (NULL);
}

static const char	*get_redirect_str(int code)
{
	static const char	*msgs[] = {
		"Redirect Network",
		"Redirect Host",
		"Redirect Type of Service and Network",
		"Redirect Type of Service and Host"
	};

	if (code >= 0 && code < (int)(sizeof(msgs) / sizeof(msgs[0])))
		return (msgs[code]);
	return (NULL);
}

static const char	*get_time_exceeded_str(int code)
{
	static const char	*msgs[] = {
		"Time to live exceeded",
		"Frag reassembly time exceeded"
	};

	if (code >= 0 && code < (int)(sizeof(msgs) / sizeof(msgs[0])))
		return (msgs[code]);
	return (NULL);
}

static void	print_ip_header_dump(struct ip *ip_hdr)
{
	size_t			j;
	unsigned char	*cp;

	printf("IP Hdr Dump:\n ");
	j = 0;
	while (j < sizeof(*ip_hdr))
	{
		printf("%02x%s", *((unsigned char *)ip_hdr + j),
			(j % 2) ? " " : "");
		j++;
	}
	printf("\n");
	printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks"
		"      Src\tDst\tData\n");
	printf(" %1x  %1x  %02x", ip_hdr->ip_v, ip_hdr->ip_hl, ip_hdr->ip_tos);
	printf(" %04x %04x",
		(ip_hdr->ip_len > 0x2000) ? ntohs(ip_hdr->ip_len) : ip_hdr->ip_len,
		ntohs(ip_hdr->ip_id));
	printf("   %1x %04x", (ntohs(ip_hdr->ip_off) & 0xe000) >> 13,
		ntohs(ip_hdr->ip_off) & 0x1fff);
	printf("  %02x  %02x %04x", ip_hdr->ip_ttl, ip_hdr->ip_p,
		ntohs(ip_hdr->ip_sum));
	printf(" %s ", inet_ntoa(*(struct in_addr *)&ip_hdr->ip_src));
	printf(" %s ", inet_ntoa(*(struct in_addr *)&ip_hdr->ip_dst));
	cp = (unsigned char *)ip_hdr + sizeof(*ip_hdr);
	j = (ip_hdr->ip_hl << 2) - sizeof(*ip_hdr);
	while (j > 0)
	{
		printf("%02x", *cp++);
		j--;
	}
	printf("\n");
}

static void	print_inner_protocol(unsigned char *cp, struct ip *inner_ip,
				int hlen)
{
	if (inner_ip->ip_p == IPPROTO_TCP)
		printf("TCP: from port %u, to port %u (decimal)\n",
			(*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
	else if (inner_ip->ip_p == IPPROTO_UDP)
		printf("UDP: from port %u, to port %u (decimal)\n",
			(*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
	else if (inner_ip->ip_p == IPPROTO_ICMP)
	{
		printf("ICMP: type %u, code %u, size %u",
			*cp, *(cp + 1), ntohs(inner_ip->ip_len) - hlen);
		if (*cp == ICMP_ECHOREPLY || *cp == ICMP_ECHO)
			printf(", id 0x%04x, seq 0x%04x",
				*(cp + 4) * 256 + *(cp + 5),
				*(cp + 6) * 256 + *(cp + 7));
		printf("\n");
	}
}

static void	print_inner_ip_data(t_icmphdr *icmp_hdr)
{
	struct ip		*inner_ip;
	int				hlen;
	unsigned char	*cp;

	inner_ip = (struct ip *)((uint8_t *)icmp_hdr + PING_PKT_HDR_SZ);
	print_ip_header_dump(inner_ip);
	hlen = inner_ip->ip_hl << 2;
	cp = (unsigned char *)inner_ip + hlen;
	print_inner_protocol(cp, inner_ip, hlen);
}

static const char	*get_icmp_error_str(int type, int code)
{
	if (type == ICMP_DEST_UNREACH)
		return (get_dest_unreach_str(code));
	if (type == ICMP_REDIRECT)
		return (get_redirect_str(code));
	if (type == ICMP_TIME_EXCEEDED)
		return (get_time_exceeded_str(code));
	if (type == ICMP_SOURCE_QUENCH)
		return ("Source Quench");
	if (type == ICMP_PARAMETERPROB)
		return ("Parameter Problem");
	return (NULL);
}

void	print_icmp_error(struct sockaddr_in *from, struct ip *ip_hdr,
		t_icmphdr *icmp_hdr, int datalen, t_ping *ping)
{
	int			hlen;
	const char	*err_str;
	struct ip	*inner_ip;

	hlen = ip_hdr->ip_hl << 2;
	if (datalen - hlen < PING_PKT_HDR_SZ + (int)sizeof(struct ip))
		return ;
	inner_ip = (struct ip *)((uint8_t *)icmp_hdr + PING_PKT_HDR_SZ);
	if (!(ping->options & OPT_VERBOSE)
		&& inner_ip->ip_dst.s_addr != ping->dest_addr.sin_addr.s_addr)
		return ;
	printf("%d bytes from %s: ", datalen - hlen,
		inet_ntoa(from->sin_addr));
	err_str = get_icmp_error_str(ICMP_HDR_TYPE(icmp_hdr),
			ICMP_HDR_CODE(icmp_hdr));
	if (err_str)
		printf("%s\n", err_str);
	else
		printf("Bad ICMP type: %d\n", ICMP_HDR_TYPE(icmp_hdr));
	if (ping->options & OPT_VERBOSE)
		print_inner_ip_data(icmp_hdr);
}

static void	print_ts_entry(unsigned char *cp, int flag, int j)
{
	uint32_t	val;

	if ((flag & 0x0f) != IPOPT_TS_TSONLY && ((j / 4) % 2 == 1))
	{
		memcpy(&val, cp, sizeof(val));
		printf("\t%s", inet_ntoa(*(struct in_addr *)&val));
	}
	else
	{
		memcpy(&val, cp, sizeof(val));
		val = ntohl(val);
		printf("\t%u ms", val);
	}
}

static void	print_ip_opt_ts(unsigned char **cpp, int *hlen)
{
	int				j;
	int				i;
	int				k;
	unsigned char	*cp;

	cp = *cpp;
	j = *++cp;
	i = *++cp;
	*hlen -= 2;
	if (i > j)
		i = j;
	if (j <= (int)(4 + sizeof(uint32_t)))
		return ;
	k = *++cp;
	++cp;
	*hlen -= 2;
	printf("\nTS:");
	j = 5;
	while (1)
	{
		print_ts_entry(cp, k, j);
		*hlen -= sizeof(uint32_t);
		cp += sizeof(uint32_t);
		j += sizeof(uint32_t);
		if (!((k & 0x0f) != IPOPT_TS_TSONLY && ((j / 4) % 2 == 1)))
			putchar('\n');
		if (j >= i)
			break ;
	}
	if (k & 0xf0)
		printf("\t(%u overflowing hosts)", k >> 4);
	*cpp = cp;
}

static void	print_ip_opt_rr(unsigned char **cpp, int *hlen)
{
	int				j;
	int				i;
	uint32_t		l;
	unsigned char	*cp;

	cp = *cpp;
	j = *++cp;
	i = *++cp;
	*hlen -= 2;
	if (i > j)
		i = j;
	i -= IPOPT_MINOFF;
	if (i <= 0)
		return ;
	printf("\nRR: ");
	while (1)
	{
		memcpy(&l, cp + 1, sizeof(l));
		if (l == 0)
			printf("\t0.0.0.0");
		else
			printf("\t%s", inet_ntoa(*(struct in_addr *)(cp + 1)));
		cp += 4;
		*hlen -= 4;
		i -= 4;
		if (i <= 0)
			break ;
		putchar('\n');
	}
	*cpp = cp;
}

static void	print_ip_opt(struct ip *ip, int hlen)
{
	unsigned char	*cp;

	cp = (unsigned char *)(ip + 1);
	while (hlen > (int)sizeof(struct ip))
	{
		if (*cp == IPOPT_EOL)
			break ;
		else if (*cp == IPOPT_NOP)
		{
			printf("\nNOP");
			--hlen;
			++cp;
		}
		else if (*cp == IPOPT_TS)
			print_ip_opt_ts(&cp, &hlen);
		else if (*cp == IPOPT_RR)
			print_ip_opt_rr(&cp, &hlen);
		else
		{
			printf("\nunknown option %x", *cp);
			--hlen;
			++cp;
		}
	}
}
