/* KallistiOS ##version##

   genwait.c
   Copyright (c)2002,2003 Dan Potter
*/

/* This is a generic wait system, much like that used in the BSD kernel.
   Basically what this allows you to do is to sleep on any random object
   (as long as you have a pointer to it) and to later wake one or all
   processes sleeping on that same identifier. This is done by taking a
   hash of the address to a set of sleep queues, and then searching
   through them from the top to find matching sleepers. This functionality
   is enough to implement all of the various thread sync primitives
   as well as some more advanced stuff. */

#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include <arch/timer.h>
#include <kos/genwait.h>
#include <kos/sem.h>

CVSID("$Id: genwait.c,v 1.4 2003/02/16 04:55:24 bardtx Exp $");

/* Our sleep queues table. This is also modeled after the BSD numbers. I
   figure if they've been using it as long as they have, they must be
   on to something. :) */
#define TABLESIZE	128
static TAILQ_HEAD(slpquehead, kthread) slpque[TABLESIZE];
#define LOOKUP(x)	(((ptr_t)(x) >> 8) & (TABLESIZE - 1))

/* Timed event queue. Anything that isn't ready to run yet, but will be
   ready to run at a later time will be placed here. Note that this doesn't
   deal with pre-emptive timeslice context switching, only things that are
   specifically blocked for a timed event (thd_sleep, genwait_wait, etc).
   This queue is sorted by remaining time (smallest at the front). */
static struct ktqueue timer_queue;

/* Internal function to insert a thread on the timer queue. Maintains
   sorting order by wait time. */
static void tq_insert(kthread_t * thd) {
	kthread_t * t;

	/* Search for its place; note that new threads will be placed at
	   the end of a group with the same timeout. */
	TAILQ_FOREACH(t, &timer_queue, timerq) {
		if (thd->wait_timeout < t->wait_timeout) {
			TAILQ_INSERT_BEFORE(t, thd, timerq);
			return;
		}
	}

	/* Couldn't find anything scheduled later, put this at the end. */
	TAILQ_INSERT_TAIL(&timer_queue, thd, timerq);
}

/* Internal function to remove a thread from the timer queue. */
static void tq_remove(kthread_t * thd) {
	TAILQ_REMOVE(&timer_queue, thd, timerq);
}

/* Returns the top thread on the timer queue (next event). If nothing is
   queued, we'll return NULL. */
static kthread_t * tq_next() {
	return TAILQ_FIRST(&timer_queue);
}

int genwait_wait(void * obj, const char * mesg, int timeout, void (*callback)(void *)) {
	int		old, rv;
	kthread_t	* me;

	/* Twiddle interrupt state */
	if (irq_inside_int()) {
		dbglog(DBG_WARNING, "genwait_wait: called inside interrupt\n");
		return -1;
	}
	old = irq_disable();

	/* Prepare us for sleep */
	me = thd_current; thd_current = NULL;
	me->state = STATE_WAIT;
	me->wait_obj = obj;
	me->wait_msg = mesg;
	if (timeout > 0) {
		/* If we have a timeout, insert us on the timer queue. */
		me->wait_timeout = timer_ms_gettime64() + timeout;
		tq_insert(me);
	} else
		me->wait_timeout = 0;
	me->wait_callback = callback;

	/* Insert us on the appropriate wait queue */
	TAILQ_INSERT_TAIL(&slpque[LOOKUP(obj)], me, thdq);

	/* Block us until we're signaled */
	rv = thd_block_now(&me->context);

	irq_restore(old);

	return rv;
}

/* Removes a thread from its wait queue; assumes ints are disabled. */
static void genwait_unqueue(kthread_t * thd) {
	if (thd->wait_obj) {
		/* Remove it from the queue */
		TAILQ_REMOVE(&slpque[LOOKUP(thd->wait_obj)], thd, thdq);

		/* Also remove it from the timer queue if applicable */
		if (thd->wait_timeout)
			tq_remove(thd);

		/* Clean up wait stuff */
		thd->wait_obj = NULL;
		thd->wait_msg = NULL;
		thd->wait_timeout = 0;
		thd->wait_callback = NULL;

		/* Make it runnable again */
		thd->state = STATE_READY;
		thd_add_to_runnable(thd, 0);
	}
}

int genwait_wake_cnt(void * obj, int cntmax) {
	kthread_t		* t, * nt;
	struct slpquehead	* qp;
	int			cnt, old;

	/* Twiddle interrupt state */
	old = irq_disable();

	/* Find the queue */
	qp = &slpque[LOOKUP(obj)];

	/* Go through and find any matching entries */
	for (cnt=0, t = TAILQ_FIRST(qp); t != NULL; t = nt) {
		/* Get the next thread up front */
		nt = TAILQ_NEXT(t, thdq);

		/* Is this thread a match? */
		if (t->wait_obj == obj) {
			/* Yes, remove it from the wait queue */
			genwait_unqueue(t);

			/* Set a successful wake return value */
			CONTEXT_RET(t->context) = 0;

			/* Check to see if we've filled our quota */
			if (cntmax > 0) {
				cnt++;
				if (cnt >= cntmax)
					break;
			}
		}
	}

	/* Re-fix IRQs */
	irq_restore(old);

	return cnt;
}

void genwait_wake_all(void * obj) {
	genwait_wake_cnt(obj, -1);
}

void genwait_wake_one(void * obj) {
	genwait_wake_cnt(obj, 1);
}

void genwait_check_timeouts(uint64 tm) {
	kthread_t	*t;

	t = tq_next();
	while (t) {
		/* If the next timeout is beyond our current time, then
		   forget about it. */
		if (t->wait_timeout > tm)
			return;

		/* Set an error code */
		t->thd_errno = EAGAIN;	/* This is fairly close */
		CONTEXT_RET(t->context) = -1;

		/* If there's a callback, then call it */
		if (t->wait_callback)
			t->wait_callback(t->wait_obj);

		/* Re-activate it */
		genwait_unqueue(t);

		/* Get the next one */
		t = tq_next();
	}
}

uint64 genwait_next_timeout() {
	kthread_t * t;

	t = tq_next();
	if (t == NULL)
		return 0;
	else
		return t->wait_timeout;
}

int genwait_init() {
	int i;
	for (i=0; i<TABLESIZE; i++)
		TAILQ_INIT(&slpque[i]);
	TAILQ_INIT(&timer_queue);
	return 0;
}

void genwait_shutdown() {
	/* XXX Do something about queued up procs */
}


