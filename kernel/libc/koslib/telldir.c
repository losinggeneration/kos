#include <kos/dbglog.h>
#include <errno.h>

// This isn't properly prototyped... sosume :)
off_t telldir() {
	dbglog(DBG_WARNING, "telldir: call ignored\n");
	errno = ENOSYS;
	return -1;
}
