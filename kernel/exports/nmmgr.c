/* KallistiOS ##version##

   nmmgr.c
   Copyright (C)2003 Dan Potter

*/

/*

This module manages "names". A name is a generic identifer that corresponds
to a handler for that name. These names can correspond to services exported
by a module or the kernel, they can be VFS handlers, they can be just about
anything. The only requirement is that they implement the nmmgr_handler_t
interface at the front of their struct.

*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <kos/nmmgr.h>
#include <kos/mutex.h>

CVSID("$Id: nmmgr.c,v 1.1 2003/06/19 04:29:00 bardtx Exp $");

/* Thread mutex for our name handler list */
static mutex_t * mutex;

/* Name handler structures; these structs contain path/type pairs that 
   describe how to handle a given path name. */
static nmmgr_list_t nmmgr_handlers;

/* Locate a name handler for a given path name */
nmmgr_handler_t * nmmgr_lookup(const char *fn) {
	nmmgr_handler_t	*cur;

	/* Scan the handler table and look for a path match */
	LIST_FOREACH(cur, &nmmgr_handlers, list_ent) {
		if (!strnicmp(cur->pathname, fn, strlen(cur->pathname)))
			break;
	}
	if (cur == NULL) {
		/* Couldn't find a handler */
		return NULL;
	} else
		return cur;
}

nmmgr_list_t * nmmgr_get_list() {
	return &nmmgr_handlers;
}

/* Add a name handler */
int nmmgr_handler_add(nmmgr_handler_t *hnd) {
	mutex_lock(mutex);

	LIST_INSERT_HEAD(&nmmgr_handlers, hnd, list_ent);

	mutex_unlock(mutex);

	return 0;
}

/* Remove a name handler */
int nmmgr_handler_remove(nmmgr_handler_t *hnd) {
	nmmgr_handler_t *c;
	int rv = -1;

	mutex_lock(mutex);

	/* Verify that it's actually in there */
	LIST_FOREACH(c, &nmmgr_handlers, list_ent) {
		if (c == hnd) {
			LIST_REMOVE(hnd, list_ent);
			rv = 0;
			break;
		}
	}

	mutex_unlock(mutex);

	return rv;
}

/* Initialize structures */
int nmmgr_init() {
	int rv = 0;

	/* Start with no handlers */
	LIST_INIT(&nmmgr_handlers);

	/* Init thread mutex */
	mutex = mutex_create();

	/* Initialize our internal exports */
	export_init();

	return rv;
}

void nmmgr_shutdown() {
	nmmgr_handler_t *c, *n;

	c = LIST_FIRST(&nmmgr_handlers);
	while (c != NULL) {
		n = LIST_NEXT(c, list_ent);
		if (c->flags & NMMGR_FLAGS_NEEDSFREE)
			free(c);
		c = n;
	}

	mutex_destroy(mutex);
}
