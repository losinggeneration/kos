#ifndef __NEWLIB_LOCK_COMMON_H
#define __NEWLIB_LOCK_COMMON_H

#include <arch/spinlock.h>

typedef struct {
	void	* owner;
	int	nest;
	spinlock_t lock;
} _newlib_lock_t;

#define _NEWLIB_LOCK_INIT { NULL, 0, SPINLOCK_INITIALIZER }

void __newlib_lock(_newlib_lock_t * lock);
void __newlib_unlock(_newlib_lock_t * lock);

#endif // __NEWLIB_LOCK_COMMON_H
