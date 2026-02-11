#ifndef FT_PING_H
# define FT_PING_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <signal.h>
# include <math.h>
# include <limits.h>
# include <stdbool.h>
# include <strings.h>
# include <getopt.h>

# include <sys/types.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <sys/select.h>

# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <arpa/inet.h>
# include <netdb.h>

# ifdef __APPLE__

typedef struct icmp	t_icmphdr;

#  define ICMP_HDR_TYPE(p)		((p)->icmp_type)
#  define ICMP_HDR_CODE(p)		((p)->icmp_code)
#  define ICMP_HDR_CKSUM(p)		((p)->icmp_cksum)
#  define ICMP_HDR_ID(p)		((p)->icmp_hun.ih_idseq.icd_id)
#  define ICMP_HDR_SEQ(p)		((p)->icmp_hun.ih_idseq.icd_seq)

#  define ICMP_DEST_UNREACH		ICMP_UNREACH
#  define ICMP_SOURCE_QUENCH	ICMP_SOURCEQUENCH
#  define ICMP_TIME_EXCEEDED	ICMP_TIMXCEED
#  define ICMP_PARAMETERPROB	ICMP_PARAMPROB

# else

typedef struct icmphdr	t_icmphdr;

#  define ICMP_HDR_TYPE(p)		((p)->type)
#  define ICMP_HDR_CODE(p)		((p)->code)
#  define ICMP_HDR_CKSUM(p)		((p)->checksum)
#  define ICMP_HDR_ID(p)		((p)->un.echo.id)
#  define ICMP_HDR_SEQ(p)		((p)->un.echo.sequence)

#  ifndef ICMP_ECHOREPLY
#   define ICMP_ECHOREPLY		0
#  endif
#  ifndef ICMP_ECHO
#   define ICMP_ECHO			8
#  endif

# endif

# define PING_PKT_DATA_SZ		56
# define PING_PKT_HDR_SZ		8
# define PING_MAX_DATALEN		65507
# define RECV_BUFSIZE			65536
# define PING_DEFAULT_TTL		64
# define PING_DEFAULT_INTERVAL	1000000
# define PING_FLOOD_INTERVAL	10000
# define MAXPATTERN				16

# define OPT_VERBOSE		(1 << 0)
# define OPT_FLOOD			(1 << 1)
# define OPT_IPTIMESTAMP	(1 << 4)

# define SOPT_TSONLY		(1 << 0)
# define SOPT_TSADDR		(1 << 1)

# ifndef MAX_IPOPTLEN
#  define MAX_IPOPTLEN		40
# endif

# ifndef IPOPT_TS_TSONLY
#  define IPOPT_TS_TSONLY	0
# endif
# ifndef IPOPT_TS_TSANDADDR
#  define IPOPT_TS_TSANDADDR	1
# endif

typedef struct s_ping_stat
{
	double	tmin;
	double	tmax;
	double	tsum;
	double	tsumsq;
}	t_ping_stat;

typedef struct s_ping
{
	int					sockfd;

	struct sockaddr_in	dest_addr;
	char				*hostname;
	char				ip_str[INET_ADDRSTRLEN];

	size_t				num_xmit;
	size_t				num_recv;

	uint16_t			ident;
	uint16_t			seq;

	unsigned int		options;
	size_t				data_length;
	int					ttl;
	int					tos;
	size_t				count;
	long				interval;
	int					timeout;
	int					linger;
	unsigned long		preload;

	unsigned char		pattern[MAXPATTERN];
	int					pattern_len;
	bool				pattern_set;

	unsigned int		ip_ts_type;

	unsigned char		*data_buffer;

	struct timeval		start_time;

	t_ping_stat			stats;
}	t_ping;

extern volatile sig_atomic_t	g_stop;

void		parse_args(t_ping *ping, int argc, char **argv);
void		print_usage(void);

int			create_socket(t_ping *ping);
int			set_ip_timestamp(t_ping *ping);

int			resolve_host(t_ping *ping, const char *host);

int			send_ping(t_ping *ping);
void		init_data_buffer(t_ping *ping);

int			recv_ping(t_ping *ping);

uint16_t	checksum(void *data, size_t len);

void		print_header(t_ping *ping);
void		print_statistics(t_ping *ping);

void		print_echo_reply(t_ping *ping, struct msghdr *msg,
				uint8_t *buf, ssize_t bytes_recv);
void		print_icmp_error(struct sockaddr_in *from, struct ip *ip_hdr,
				t_icmphdr *icmp_hdr, int datalen, unsigned int options);

void		setup_signals(void);

double		calc_stddev(double tsumsq, double tsum, double count);
long		parse_number(const char *str, long max_val, const char *name);
void		decode_pattern(const char *arg, int *pattern_len,
				unsigned char *pattern);

#endif
