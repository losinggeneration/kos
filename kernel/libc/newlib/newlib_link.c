#include <sys/reent.h>
#include <errno.h>

int _link_r(struct _reent * reent, const char * oldf, const char * newf) {
	reent->_errno = EMLINK;
	return -1;
}
