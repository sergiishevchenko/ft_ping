#include "ft_ping.h"

double	calc_stddev(double tsumsq, double tsum, double count)
{
	double	avg;
	double	vari;

	if (count <= 0)
		return (0.0);
	avg = tsum / count;
	vari = tsumsq / count - avg * avg;
	if (vari < 0.0)
		vari = 0.0;
	return (sqrt(vari));
}

long	parse_number(const char *str, long max_val, const char *name)
{
	char	*endptr;
	long	val;

	errno = 0;
	val = strtol(str, &endptr, 10);
	if (*endptr != '\0' || errno == ERANGE)
	{
		fprintf(stderr, "ft_ping: invalid %s value: '%s'\n", name, str);
		exit(EXIT_FAILURE);
	}
	if (val < 0 || val > max_val)
	{
		fprintf(stderr, "ft_ping: %s value out of range: '%s'\n", name, str);
		exit(EXIT_FAILURE);
	}
	return (val);
}

static int	hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return (-1);
}

void	decode_pattern(const char *arg, int *pattern_len,
			unsigned char *pattern)
{
	int		i;
	int		high;
	int		low;

	i = 0;
	while (*arg && i < MAXPATTERN)
	{
		high = hex_digit(*arg);
		if (high < 0)
		{
			fprintf(stderr, "ft_ping: error in pattern near '%s'\n", arg);
			exit(EXIT_FAILURE);
		}
		arg++;
		if (*arg)
		{
			low = hex_digit(*arg);
			if (low < 0)
			{
				fprintf(stderr,
					"ft_ping: error in pattern near '%s'\n", arg);
				exit(EXIT_FAILURE);
			}
			pattern[i] = (unsigned char)((high << 4) | low);
			arg++;
		}
		else
			pattern[i] = (unsigned char)(high << 4);
		i++;
	}
	*pattern_len = i;
}
