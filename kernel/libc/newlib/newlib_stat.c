#include <unistd.h>
#include <sys/stat.h>

int _stat_r(void * reent, const char * fn, struct stat * pstat) {
	pstat->st_mode = S_IFCHR;
	return 0;
}
