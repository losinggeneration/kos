#include <sys/reent.h>
#include <sys/times.h>

int _times_r(struct _reent * re, struct tms * tmsbuf) {
	return -1;
}
