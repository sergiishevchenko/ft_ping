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

/*
** ICMP header portability: macOS (BSD struct icmp) vs Linux (struct icmphdr)
** use different field names. t_icmphdr + ICMP_HDR_* macros let send.c/recv.c
** stay identical on both platforms.
*/
# ifdef __APPLE__

/* BSD names members icmp_type, icmp_code, … in <netinet/ip_icmp.h> */
typedef struct icmp	t_icmphdr;

#  define ICMP_HDR_TYPE(p)		((p)->icmp_type)
#  define ICMP_HDR_CODE(p)		((p)->icmp_code)
#  define ICMP_HDR_CKSUM(p)		((p)->icmp_cksum)
#  define ICMP_HDR_ID(p)		((p)->icmp_hun.ih_idseq.icd_id)
#  define ICMP_HDR_SEQ(p)		((p)->icmp_hun.ih_idseq.icd_seq)

/* Map Linux-style error type names to BSD constants in print.c */
#  define ICMP_DEST_UNREACH		ICMP_UNREACH
#  define ICMP_SOURCE_QUENCH	ICMP_SOURCEQUENCH
#  define ICMP_TIME_EXCEEDED	ICMP_TIMXCEED
#  define ICMP_PARAMETERPROB	ICMP_PARAMPROB

# else

/* Linux/glibc: flat struct icmphdr with type, code, un.echo.id, … */
typedef struct icmphdr	t_icmphdr;

#  define ICMP_HDR_TYPE(p)		((p)->type)
#  define ICMP_HDR_CODE(p)		((p)->code)
#  define ICMP_HDR_CKSUM(p)		((p)->checksum)
#  define ICMP_HDR_ID(p)		((p)->un.echo.id)
#  define ICMP_HDR_SEQ(p)		((p)->un.echo.sequence)

/* RFC 792 echo types; fallback if headers omit the macros */
#  ifndef ICMP_ECHOREPLY
#   define ICMP_ECHOREPLY		0
#  endif
#  ifndef ICMP_ECHO
#   define ICMP_ECHO			8
#  endif

# endif

/* ICMP data bytes; 56 + 8-byte hdr → "64 bytes" in reply (inetutils default) */
# define PING_PKT_DATA_SZ		56
/* Size of ICMP header (type, code, cksum, id, seq) */
# define PING_PKT_HDR_SZ		8
/* Max ICMP payload: 65535 IP max − 20 IP hdr − 8 ICMP hdr */
# define PING_MAX_DATALEN		65507
/* recvmsg buffer; holds max IPv4 datagram (64 KiB) */
# define RECV_BUFSIZE			65536
/* Outgoing IP TTL when --ttl not set; common OS default */
# define PING_DEFAULT_TTL		64
/* Microseconds between probes (1 s); inetutils default rate */
# define PING_DEFAULT_INTERVAL	1000000
/* Microseconds between probes in flood mode (-f); ~100 probes/s */
# define PING_FLOOD_INTERVAL	10000
/* Max bytes for -p pattern; repeats to fill payload */
# define MAXPATTERN				16
/* Duplicate bitmap: 128 bytes × 8 bits → track 1024 seq numbers */
# define PING_CKTAB_SZ			128

/* -v: print id in header, ICMP errors, IP Hdr Dump */
# define OPT_VERBOSE		(1 << 0)
/* -f: minimal output, fast send interval */
# define OPT_FLOOD			(1 << 1)
/* --ip-timestamp: attach IP timestamp option to probes */
# define OPT_IPTIMESTAMP	(1 << 4)

/* --ip-timestamp tsonly: record timestamps only */
# define SOPT_TSONLY		(1 << 0)
/* --ip-timestamp tsaddr: timestamp + address per hop */
# define SOPT_TSADDR		(1 << 1)

/*
** IP option fallbacks for --ip-timestamp (RFC 791). Linux defines these in
** <netinet/ip.h>; macOS may not — #ifndef keeps the OS value when present.
*/
# ifndef MAX_IPOPTLEN
/* Max IPv4 options area (bytes); buffer size in set_ip_timestamp() */
#  define MAX_IPOPTLEN		40
# endif

# ifndef IPOPT_TS_TSONLY
/* Timestamp option: record time only (flag --ip-timestamp tsonly) */
#  define IPOPT_TS_TSONLY	0
# endif
# ifndef IPOPT_TS_TSANDADDR
/* Timestamp option: time + router address (flag tsaddr) */
#  define IPOPT_TS_TSANDADDR	1
# endif

typedef struct s_ping_stat
{
	double	tmin;		/* shortest RTT (ms); init high sentinel until 1st reply */
	double	tmax;		/* longest RTT (ms) */
	double	tsum;		/* sum of RTTs for average */
	double	tsumsq;		/* sum of RTT² for stddev (calc_stddev) */
}	t_ping_stat;

typedef struct s_ping
{
	/* raw ICMP socket; -1 until create_socket() */
	int					sockfd;

	/* target IPv4 + port (unused); filled by resolve_host() */
	struct sockaddr_in	dest_addr;

	/* CLI hostname argument; strdup'd for header line */
	char				*hostname;

	/* resolved target as "dotted.quad" for PING line */
	char				ip_str[INET_ADDRSTRLEN];

	/* probes sent (successful sendto) */
	size_t				num_xmit;

	/* echo replies accepted (incl. duplicates) */
	size_t				num_recv;

	/* duplicate replies (same seq seen twice) */
	size_t				num_rept;

	/* bitmap: bit set when seq was already received */
	unsigned char		recv_table[PING_CKTAB_SZ];

	/* ICMP id; getpid() & 0xFFFF — unique per process */
	uint16_t			ident;

	/* next ICMP seq; incremented after each send */
	uint16_t			seq;

	/* OPT_* bitmask from flags (-v, -f, --ip-timestamp) */
	unsigned int		options;

	/* ICMP payload bytes (-s); default PING_PKT_DATA_SZ */
	size_t				data_length;

	/* IP TTL (--ttl); default PING_DEFAULT_TTL */
	int					ttl;

	/* IP TOS (-T); -1 = do not setsockopt(IP_TOS) */
	int					tos;

	/* -c: stop after N unique replies; 0 = unlimited */
	size_t				count;

	/* µs between sends; PING_DEFAULT_INTERVAL or PING_FLOOD_INTERVAL */
	long				interval;

	/* -w: max run time (s); -1 = no wall-clock limit */
	int					timeout;

	/* -W: wait for late replies after -c done (s); default 10 */
	int					linger;

	/* -l: send this many probes before entering main loop */
	unsigned long		preload;

	/* -p: hex bytes repeated into payload */
	unsigned char		pattern[MAXPATTERN];

	/* valid length in pattern[]; default MAXPATTERN if -p not set */
	int					pattern_len;

	/* true when -p given; else auto 0x00,0x01,… fill */
	bool				pattern_set;

	/* SOPT_TSONLY or SOPT_TSADDR for --ip-timestamp */
	unsigned int		ip_ts_type;

	/* true when -r given; SO_DONTROUTE bypasses routing table */
	bool				dontroute;

	/* malloc'd payload template (pattern or default fill) */
	unsigned char		*data_buffer;

	/* session start; used for -w and statistics duration */
	struct timeval		start_time;

	/* min/avg/max/stddev RTT accumulators */
	t_ping_stat			stats;
}	t_ping;

/* set to 1 by SIGINT handler; ends ping_loop */
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
				t_icmphdr *icmp_hdr, int datalen, t_ping *ping);

void		setup_signals(void);

double		calc_stddev(double tsumsq, double tsum, double count);
long		parse_number(const char *str, long max_val, const char *name);
void		decode_pattern(const char *arg, int *pattern_len,
				unsigned char *pattern);

#endif
