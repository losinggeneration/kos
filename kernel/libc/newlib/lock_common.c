
#include "lock_common.h"
#include <arch/irq.h>
#include <kos/thread.h>

void __newlib_lock(_newlib_lock_t * lock) {
	int old;
	int iscur;

	// Check to see if we already own it. If so, everything is clear
	// to incr nest. Otherwise, we can safely go on to do a normal
	// spinlock wait.
	old = irq_disable();
	iscur = lock->owner == thd_current;
	irq_restore(old);
	if (iscur) {
		lock->nest++;
		return;
	}

	// It doesn't belong to us. Wait for it to come free.
	spinlock_lock(&lock->lock);

	// We own it now, so it's safe to init the rest of this.
	lock->owner = thd_current;
	lock->nest = 1;
}

void __newlib_unlock(_newlib_lock_t * lock) {
	// Check to see how much we own it.
	if (lock->nest == 1) {
		lock->owner = NULL;
		lock->nest = -1;
		spinlock_unlock(&lock->lock);
	} else {
		lock->nest--;
	}
}
