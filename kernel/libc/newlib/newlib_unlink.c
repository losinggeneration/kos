#include <kos/fs.h>

int _unlink_r(void * reent, const char * fn) {
	return fs_unlink(fn);
}
