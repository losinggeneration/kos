#include <kos/fs.h>

off_t _lseek_r(void * reent, int fd, off_t pos, int whence) {
	return fs_seek(fd, pos, whence);
}
