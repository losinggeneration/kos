// These calls can be nested.
#include <sys/reent.h>
#include "lock_common.h"

static _newlib_lock_t lock = _NEWLIB_LOCK_INIT;

void __env_lock(struct _reent * r) {
	__newlib_lock(&lock);
}

void __env_unlock(struct _reent * r) {
	__newlib_unlock(&lock);
}


