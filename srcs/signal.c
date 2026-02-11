#include "ft_ping.h"

static void	sig_int_handler(int sig)
{
	(void)sig;
	g_stop = 1;
}

void	setup_signals(void)
{
	struct sigaction	sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_int_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) < 0)
	{
		fprintf(stderr, "ft_ping: sigaction: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}
