#include <sys/reent.h>
#include <kos/fs.h>

int _close_r(struct _reent * reent, int f) {
	fs_close(f);
	return 0;
}
