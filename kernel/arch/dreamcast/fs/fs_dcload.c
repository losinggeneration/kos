/* KallistiOS ##version##

   kernel/arch/dreamcast/fs/fs_dcload.c
   Copyright (C)2002 Andrew Kieschnick
   Copyright (C)2004 Dan Potter
   
*/

/*

This is a rewrite of Dan Potter's fs_serconsole to use the dcload / dc-tool
fileserver and console. 

printf goes to the dc-tool console
/pc corresponds to / on the system running dc-tool

*/

#include <dc/fs_dcload.h>
#include <kos/thread.h>
#include <arch/spinlock.h>
#include <kos/dbgio.h>
#include <kos/fs.h>

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>

CVSID("$Id: fs_dcload.c,v 1.14 2003/04/24 03:05:49 bardtx Exp $");

static spinlock_t mutex = SPINLOCK_INITIALIZER;

#define plain_dclsc(...) ({ \
	int old = 0, rv; \
	if (!irq_inside_int()) { \
		old = irq_disable(); \
	} \
	while ((*(vuint32 *)0xa05f688c) & 0x20) \
		; \
	rv = dcloadsyscall(__VA_ARGS__); \
	if (!irq_inside_int()) \
		irq_restore(old); \
	rv; \
})

// #define plain_dclsc(...) dcloadsyscall(__VA_ARGS__)

static void * lwip_dclsc = 0;

#define dclsc(...) ({ \
    int rv; \
    if (lwip_dclsc) \
	rv = (*(int (*)()) lwip_dclsc)(__VA_ARGS__); \
    else \
	rv = plain_dclsc(__VA_ARGS__); \
    rv; \
})

/* Printk replacement */

int dcload_write_buffer(const uint8 *data, int len, int xlat) {
    if (lwip_dclsc && irq_inside_int()) {
	errno = EAGAIN;
	return -1;
    }
    spinlock_lock(&mutex);
    dclsc(DCLOAD_WRITE, 1, data, len);
    spinlock_unlock(&mutex);

    return len;
}

int dcload_read_cons() {
    return -1;
}

static char *dcload_path = NULL;
uint32 dcload_open(vfs_handler_t * vfs, const char *fn, int mode) {
    int hnd = 0;
    uint32 h;
    int dcload_mode = 0;
    
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
    
    if (mode & O_DIR) {
        if (fn[0] == '\0') {
            fn = "/";
        }
	hnd = dclsc(DCLOAD_OPENDIR, fn);
	if (hnd) {
	    if (dcload_path)
		free(dcload_path);
	    if (fn[strlen(fn) - 1] == '/') {
		dcload_path = malloc(strlen(fn)+1);
		strcpy(dcload_path, fn);
	    } else {
		dcload_path = malloc(strlen(fn)+2);
		strcpy(dcload_path, fn);
		strcat(dcload_path, "/");
	    }
	}
    } else { /* hack */
	if ((mode & O_MODE_MASK) == O_RDONLY)
	    dcload_mode = 0;
	if ((mode & O_MODE_MASK) == O_RDWR)
	    dcload_mode = 2 | 0x0200;
	if ((mode & O_MODE_MASK) == O_WRONLY)
	    dcload_mode = 1 | 0x0200;
	if ((mode & O_MODE_MASK) == O_APPEND)
	    dcload_mode =  2 | 8 | 0x0200;
	if (mode & O_TRUNC)
	    dcload_mode |= 0x0400;
	hnd = dclsc(DCLOAD_OPEN, fn, dcload_mode, 0644);
	hnd++; /* KOS uses 0 for error, not -1 */
    }
    
    h = hnd;

    spinlock_unlock(&mutex);
    return h;
}

void dcload_close(uint32 hnd) {
    if (lwip_dclsc && irq_inside_int())
	return;

    spinlock_lock(&mutex);
    
    if (hnd) {
	if (hnd > 100) /* hack */
	    dclsc(DCLOAD_CLOSEDIR, hnd);
	else {
	    hnd--; /* KOS uses 0 for error, not -1 */
	    dclsc(DCLOAD_CLOSE, hnd);
	}
    }
    spinlock_unlock(&mutex);
}

ssize_t dcload_read(uint32 hnd, void *buf, size_t cnt) {
    ssize_t ret = -1;
    
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
    
    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_READ, hnd, buf, cnt);
    }
    
    spinlock_unlock(&mutex);
    return ret;
}

ssize_t dcload_write(uint32 hnd, const void *buf, size_t cnt) {
    ssize_t ret = -1;
    	
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
    
    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_WRITE, hnd, buf, cnt);
    }

    spinlock_unlock(&mutex);
    return ret;
}

off_t dcload_seek(uint32 hnd, off_t offset, int whence) {
    off_t ret = -1;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_LSEEK, hnd, offset, whence);
    }

    spinlock_unlock(&mutex);
    return ret;
}

off_t dcload_tell(uint32 hnd) {
    off_t ret = -1;
    
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	ret = dclsc(DCLOAD_LSEEK, hnd, 0, SEEK_CUR);
    }

    spinlock_unlock(&mutex);
    return ret;
}

size_t dcload_total(uint32 hnd) {
    size_t ret = -1;
    size_t cur;
	
    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);
	
    if (hnd) {
	hnd--; /* KOS uses 0 for error, not -1 */
	cur = dclsc(DCLOAD_LSEEK, hnd, 0, SEEK_CUR);
	ret = dclsc(DCLOAD_LSEEK, hnd, 0, SEEK_END);
	dclsc(DCLOAD_LSEEK, hnd, cur, SEEK_SET);
    }
	
    spinlock_unlock(&mutex);
    return ret;
}

/* Not thread-safe, but that's ok because neither is the FS */
static dirent_t dirent;
dirent_t *dcload_readdir(uint32 hnd) {
    dirent_t *rv = NULL;
    dcload_dirent_t *dcld;
    dcload_stat_t filestat;
    char *fn;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    if (hnd < 100) return NULL; /* hack */

    spinlock_lock(&mutex);

    dcld = (dcload_dirent_t *)dclsc(DCLOAD_READDIR, hnd);
    
    if (dcld) {
	rv = &dirent;
	strcpy(rv->name, dcld->d_name);
	rv->size = 0;
	rv->time = 0;
	rv->attr = 0; /* what the hell is attr supposed to be anyways? */

	fn = malloc(strlen(dcload_path)+strlen(dcld->d_name)+1);
	strcpy(fn, dcload_path);
	strcat(fn, dcld->d_name);

	if (!dclsc(DCLOAD_STAT, fn, &filestat)) {
	    if (filestat.st_mode & S_IFDIR)
		rv->size = -1;
	    else
		rv->size = filestat.st_size;
	    rv->time = filestat.st_mtime;
	    
	}
	
	free(fn);
    }
    
    spinlock_unlock(&mutex);
    return rv;
}

int dcload_rename(vfs_handler_t * vfs, const char *fn1, const char *fn2) {
    int ret;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    /* really stupid hack, since I didn't put rename() in dcload */

    ret = dclsc(DCLOAD_LINK, fn1, fn2);

    if (!ret)
	ret = dclsc(DCLOAD_UNLINK, fn1);

    spinlock_unlock(&mutex);
    return ret;
}

int dcload_unlink(vfs_handler_t * vfs, const char *fn) {
    int ret;

    if (lwip_dclsc && irq_inside_int())
	return 0;

    spinlock_lock(&mutex);

    ret = dclsc(DCLOAD_UNLINK, fn);

    spinlock_unlock(&mutex);
    return ret;
}

/* Pull all that together */
static vfs_handler_t vh = {
	/* Name handler */
	{
		"/pc",		/* name */
		0,		/* tbfi */
		0x00010000,	/* Version 1.0 */
		0,		/* flags */
		NMMGR_TYPE_VFS,
		NMMGR_LIST_INIT
	},

	0, NULL,	/* no cache, privdata */

	dcload_open, 
	dcload_close,
	dcload_read,
	dcload_write,
	dcload_seek,
	dcload_tell,
	dcload_total,
	dcload_readdir,
	NULL,               /* ioctl */
	dcload_rename,
	dcload_unlink,
	NULL                /* mmap */
};

// We have to provide a minimal interface in case dcload usage is
// disabled through init flags.
static int never_detected() { return 0; }
dbgio_handler_t dbgio_dcload = {
	"fs_dcload_uninit",
	never_detected
};

int fs_dcload_detected() {
    /* Check for dcload */
    if (*DCLOADMAGICADDR == DCLOADMAGICVALUE)
	return 1;
    else
	return 0;
}

/* Call this before arch_init_all (or any call to dbgio_*) to use dcload's
   console output functions. */
void fs_dcload_init_console() {
    /* Setup our dbgio handler */
    memcpy(&dbgio_dcload, &dbgio_null, sizeof(dbgio_dcload));
    dbgio_dcload.detected = fs_dcload_detected;
    dbgio_dcload.write_buffer = dcload_write_buffer;
    // dbgio_dcload.read = dcload_read_cons;
}

static int *dcload_wrkmem = NULL;
int dcload_type = DCLOAD_TYPE_NONE;

/* Call fs_dcload_init_console() before calling fs_dcload_init() */
int fs_dcload_init() {
    
    /* Check for dcload */
    if (*DCLOADMAGICADDR != DCLOADMAGICVALUE)
	return -1;

    /* Give dcload the 64k it needs to compress data (if on serial) */
    dcload_wrkmem = malloc(65536);
    if (dcload_wrkmem) {
    	if (dclsc(DCLOAD_ASSIGNWRKMEM, dcload_wrkmem) == -1) {
    	    free(dcload_wrkmem);
    	    dcload_type = DCLOAD_TYPE_IP;
    	    dcload_wrkmem = NULL;
    	} else {
    	    dcload_type = DCLOAD_TYPE_SER;
    	}
    }

    /* Check for combination of KOS networking and dcload-ip */
    if ((dcload_type == DCLOAD_TYPE_IP) && (__kos_init_flags & INIT_NET)) {
	dbglog(DBG_INFO, "dc-load console+kosnet, will switch to internal ethernet\n");
	return -1;
	/* if (old_printk) {
	    dbgio_set_printk(old_printk);
	    old_printk = 0;
	}
	return -1; */
    }

    /* Register with VFS */
    return nmmgr_handler_add(&vh.nmmgr);
}

int fs_dcload_shutdown() {
    /* Check for dcload */
    if (*DCLOADMAGICADDR != DCLOADMAGICVALUE)
	return -1;

    /* Free dcload wrkram */
    if (dcload_wrkmem) {
        dclsc(DCLOAD_ASSIGNWRKMEM, 0);
        free(dcload_wrkmem);
    }

    /* If we're not on serial, and we're not on lwIP, we can
       continue using the debug channel */
    if (dcload_type != DCLOAD_TYPE_IP && !lwip_dclsc) {
        dcload_type = DCLOAD_TYPE_NONE;
        dbgio_dev_select("scif");
    }

    return nmmgr_handler_remove(&vh.nmmgr);
}

/* used for dcload-ip + lwIP
 * assumes fs_dcload_init() was previously called
 */
int fs_dcload_init_lwip(void *p)
{
    /* Check for combination of KOS networking and dcload-ip */
    if ((dcload_type == DCLOAD_TYPE_IP) && (__kos_init_flags & INIT_NET)) {
	lwip_dclsc = p;

	dbglog(DBG_INFO, "dc-load console support enabled (lwIP)\n");
    } else
	return -1;

    /* Register with VFS */
    return nmmgr_handler_add(&vh.nmmgr);
}

