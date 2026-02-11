#include "ft_ping.h"

uint16_t	checksum(void *data, size_t len)
{
	uint16_t	*ptr;
	uint32_t	sum;

	ptr = (uint16_t *)data;
	sum = 0;
	while (len > 1)
	{
		sum += *ptr++;
		len -= 2;
	}
	if (len == 1)
		sum += *(uint8_t *)ptr;
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	return ((uint16_t)~sum);
}
