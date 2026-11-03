#include <stdio.h>
#include <time.h>

/* this file is for the sake of the self test byte/second calc,
 * as `date + '%N'` is not portable.
 */
int
main(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
		perror("clock_gettime");
		return 1;
	}
	printf("%ld.%09ld\n", (long)ts.tv_sec, ts.tv_nsec);
	return 0;
}

