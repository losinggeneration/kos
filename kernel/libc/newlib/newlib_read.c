#include <kos/fs.h>

long _read_r(void * reent, int fd, void * buf, size_t cnt) {
	return fs_read(fd, buf, cnt);
}
