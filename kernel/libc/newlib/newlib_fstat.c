#include <unistd.h>
#include <sys/stat.h>

int _fstat_r(struct _reent  * reent, int fd, struct stat * pstat) {
	pstat->st_mode = S_IFCHR;
	return 0;
}
