#include <kos/thread.h>
#include <sys/reent.h>

int _getpid_r(struct _reent * re) {
	return thd_current->tid;
}
