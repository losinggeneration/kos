/* KallistiOS ##version##

   fs_vmu.c
   Copyright (C)2003 Dan Potter

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>
#include <arch/types.h>
#include <kos/mutex.h>
#include <dc/fs_vmu.h>
#include <dc/vmufs.h>
#include <dc/maple.h>
#include <dc/maple/vmu.h>
#include <sys/queue.h>

CVSID("$Id: fs_vmu.c,v 1.4 2003/07/31 00:48:42 bardtx Exp $");

/*

This is the vmu filesystem module.  Because there are no directories on vmu's
it's pretty simple, however the filesystem uses a seperate directory for each
of the vmu slots, so if vmufs were mounted on /vmu, /vmu/a1/ is the dir for 
slot 1 on port a, and /vmu/c2 is slot 2 on port c, etc.

At the moment this FS is kind of a hack because of the simplicity (and weirdness)
of the VMU file system. For one, all files must be pretty small, so it loads
and caches the entire file on open. For two, all files are a multiple of 512
bytes in size (no way around this one). On top of it all, files may have an 
obnoxious header and you can't just read and write them with abandon like
a normal file system. We'll have to find ways around this later on, but for
now it gives the file data to you raw.

Note: this new version now talks directly to the vmufs module and doesn't do
any block-level I/O anymore. This layer and that one are interchangeable
and may be used pretty much simultaneously in the same program.

*/

/* Enable this if you want copious debug output */
/* #define VMUFS_DEBUG */

#define VMU_DIR 	0
#define VMU_FILE	1
#define VMU_ANY		-1	/* Used for checking validity */

/* File handles */
typedef struct vmu_fh_str {
	uint32		strtype;		/* 0==dir, 1==file */
	TAILQ_ENTRY(vmu_fh_str)	listent;	/* list entry */

	int		mode;			/* mode the file was opened with */
	char		path[17];		/* full path of the file */
	char		name[13];		/* name of the file */
	off_t		loc;			/* current position in the file (bytes) */
	maple_device_t	* dev;			/* maple address of the vmu to use */
	int		filesize;		/* file length from dirent (in 512-byte blks) */
	uint8		*data;			/* copy of the whole file */
} vmu_fh_t;

/* Directory handles */
typedef struct vmu_dh_str {
	uint32		strtype;		/* 0==dir, 1==file */
	TAILQ_ENTRY(vmu_dh_str) listent;	/* list entry */

	int		rootdir;		/* 1 if we're reading /vmu */
	dirent_t	dirent;			/* Dirent to pass back */
	vmu_dir_t	* dirblocks;		/* Copy of all directory blocks */
	uint16		entry;			/* Current dirent */
	uint16		dircnt;			/* Count of dir entries */	
	maple_device_t	* dev;			/* VMU address */
} vmu_dh_t;

/* Linked list of open files (controlled by "mutex") */
TAILQ_HEAD(vmu_fh_list, vmu_fh_str) vmu_fh;

/* Thread mutex for vmu_fh access */
static mutex_t * fh_mutex;


/* Take a VMUFS path and return the requested address */
static maple_device_t * vmu_path_to_addr(const char *p) {
	char port;

	if (p[0] != '/') return NULL;			/* Only absolute paths */
	port = p[1] | 32;				/* Lowercase the port */
	if (port < 'a' || port > 'd') return NULL;	/* Unit A-D, device 0-5 */
	if (p[2] < '0' || p[2] > '5') return NULL;
	
	return maple_enum_dev(port - 'a', p[2] - '0');
}

/* Open the fake vmu root dir /vmu */
vmu_fh_t *vmu_open_vmu_dir() {
	int p, u;
	unsigned int num = 0;
	char names[MAPLE_PORT_COUNT * MAPLE_UNIT_COUNT][2];
	vmu_dh_t *dh;
	maple_device_t * dev;

	/* Determine how many VMUs are connected */
	for (p=0; p<MAPLE_PORT_COUNT; p++) {
		for (u=0; u<MAPLE_UNIT_COUNT; u++) {
			dev = maple_enum_dev(p, u);
			if (!dev) continue;

			if (dev->info.functions & MAPLE_FUNC_MEMCARD) {
				names[num][0] = p+'a';
				names[num][1] = u+'0';
				num++;
#ifdef VMUFS_DEBUG
				dbglog(DBG_KDEBUG, "vmu_open_vmu_dir: found memcard (%c%d)\n", 'a'+p, u);
#endif
			}
		}
	}

#ifdef VMUFS_DEBUG
	dbglog(DBG_KDEBUG, "# of memcards found: %d\n", num);
#endif

	dh = malloc(sizeof(vmu_dh_t));
	memset(dh, 0, sizeof(vmu_dh_t));
	dh->strtype = VMU_DIR;
	dh->dirblocks = malloc(num * sizeof(vmu_dir_t));
	if (!dh->dirblocks) {
		free(dh);
		return NULL;
	}
	dh->rootdir = 1;
	dh->entry = 0;
	dh->dircnt = num;
	dh->dev = NULL;

	/* Create the directory entries */
	for (u=0; u<num; u++) {
		memset(dh->dirblocks + u, 0, sizeof(vmu_dir_t));	/* Start in a clean room */
		memcpy(dh->dirblocks[u].filename, names+u, 2);
		dh->dirblocks[u].filetype = 0xff;
	}
	return (vmu_fh_t *)dh;
}

/* opendir function */
static vmu_fh_t *vmu_open_dir(maple_device_t * dev) {
	vmu_dir_t	* dirents;
	int		dircnt;
	vmu_dh_t	* dh;

	/* Read the VMU's directory */
	if (vmufs_readdir(dev, &dirents, &dircnt) < 0)
		return NULL;

	/* Allocate a handle for the dir blocks */
	dh = malloc(sizeof(vmu_dh_t));
	dh->strtype = VMU_DIR;
	dh->dirblocks = dirents;
	dh->rootdir = 0;
	dh->entry = 0;
	dh->dircnt = dircnt;
	dh->dev = dev;
	
	return (vmu_fh_t *)dh;
}

/* openfile function */
static vmu_fh_t *vmu_open_file(maple_device_t * dev, const char *path, int mode) {
	vmu_fh_t	* fd;		/* file descriptor */
	int		realmode, rv;
	void		* data;
	int		datasize;

	/* Malloc a new fh struct */
	fd = malloc(sizeof(vmu_fh_t));

	/* Fill in the filehandle struct */
	fd->strtype = VMU_FILE;
	fd->mode = mode;
	strncpy(fd->path, path, 16);
	strncpy(fd->name, path + 4, 12);
	fd->loc = 0;
	fd->dev = dev;

	/* What mode are we opening in? If we're reading or writing without O_TRUNC
	   then we need to read the old file if there is one. */
	realmode = mode & O_MODE_MASK;
	if (realmode == O_RDONLY || ((realmode == O_RDWR || realmode == O_WRONLY) && !(mode & O_TRUNC))) {
		/* Try to open it */
		rv = vmufs_read(dev, fd->name, &data, &datasize);

		if (rv < 0) {
			if (realmode == O_RDWR || realmode == O_WRONLY) {
				/* In some modes failure is ok -- just setup a blank first block. */
				data = malloc(512);
				datasize = 512;
				memset(data, 0, 512);
			} else {
				free(fd);
				return NULL;
			}
		}
	} else {
		/* We're writing with truncate... just setup a blank first block. */
		data = malloc(512);
		datasize = 512;
		memset(data, 0, 512);
	}

	fd->data = (uint8 *)data;
	fd->filesize = datasize/512;

	if (fd->filesize == 0) {
		dbglog(DBG_WARNING, "VMUFS: can't open zero-length file %s\n", path);
		free(fd);
		return NULL;
	}
	
	return fd;
}

/* open function */
static void * vmu_open(vfs_handler_t * vfs, const char *path, int mode) {
	maple_device_t	* dev;		/* maple bus address of the vmu unit */
	vmu_fh_t	*fh;

	if (!*path || (path[0] == '/' && !path[1])) {
		/* /vmu should be opened */
		fh = vmu_open_vmu_dir();
	} else {
		/* Figure out which vmu slot is being opened */
		dev = vmu_path_to_addr(path);
		/* printf("VMUFS: card address is %02x\n", addr); */
		if (dev == NULL) return 0;

		/* Check for open as dir */
		if (strlen(path) == 3 || (strlen(path) == 4 && path[3] == '/')) {
			if (!(mode & O_DIR)) return 0;
			fh = vmu_open_dir(dev);
		} else {
			if (mode & O_DIR) return 0;
			fh = vmu_open_file(dev, path, mode);
		}
	}
	if (fh == NULL) return 0;

	/* link the fh onto the top of the list */
	mutex_lock(fh_mutex);
	TAILQ_INSERT_TAIL(&vmu_fh, fh, listent);
	mutex_unlock(fh_mutex);

	return (void *)fh;
}

/* Verify that a given hnd is actually in the list */
static int vmu_verify_hnd(void * hnd, int type) {
	vmu_fh_t	*cur;
	int		rv;

	rv = 0;
	
	mutex_lock(fh_mutex);
	TAILQ_FOREACH(cur, &vmu_fh, listent) {
		if ((void *)cur == hnd) {
			rv = 1;
			break;
		}
	}
	mutex_unlock(fh_mutex);
	
	if (rv)
		return type == VMU_ANY ? 1 : (cur->strtype == type);
	else
		return 0;
}

/* write a file out before closing it: we aren't perfect on error handling here */
static int vmu_write_close(void * hnd) {
	vmu_fh_t	*fh;

	fh = (vmu_fh_t*)hnd;
	return vmufs_write(fh->dev, fh->name, fh->data, fh->filesize*512, VMUFS_OVERWRITE);
}

/* close a file */
static void vmu_close(void * hnd) {
	vmu_fh_t *fh;

	/* Check the handle */
	if (!vmu_verify_hnd(hnd, VMU_ANY))
		return;

	fh = (vmu_fh_t *)hnd;

	switch (fh->strtype) {
	case VMU_DIR: {
		vmu_dh_t * dir = (vmu_dh_t *)hnd;
		free(dir->dirblocks);
		break;
	}

	case VMU_FILE:
		if ((fh->mode & O_MODE_MASK) == O_WRONLY ||
			(fh->mode & O_MODE_MASK) == O_RDWR)
		{
			vmu_write_close(hnd);
		}
		free(fh->data);
		break;
	
	}
	/* Look for the one to get rid of */
	mutex_lock(fh_mutex);
	TAILQ_REMOVE(&vmu_fh, fh, listent);
	mutex_unlock(fh_mutex);

	free(fh);
}

/* read function */
static ssize_t vmu_read(void * hnd, void *buffer, size_t cnt) {
	vmu_fh_t *fh;

	/* Check the handle */
	if (!vmu_verify_hnd(hnd, VMU_FILE))
		return -1;

	fh = (vmu_fh_t *)hnd;

	/* make sure we're opened for reading */
	if ((fh->mode & O_MODE_MASK) != O_RDONLY && (fh->mode & O_MODE_MASK) != O_RDWR)
		return 0;

	/* Check size */
	cnt = (fh->loc + cnt) > (fh->filesize*512) ?
		(fh->filesize*512 - fh->loc) : cnt;

	/* Reads past EOF return 0 */
	if ((long)cnt < 0)
		return 0;

	/* Copy out the data */
	memcpy(buffer, fh->data+fh->loc, cnt);
	fh->loc += cnt;
	
	return cnt;
}

/* write function */
static ssize_t vmu_write(void * hnd, const void *buffer, size_t cnt) {
	vmu_fh_t	*fh;
	void		*tmp;
	int		n;

	/* Check the handle we were given */
	if (!vmu_verify_hnd(hnd, VMU_FILE))
		return -1;

	fh = (vmu_fh_t *)hnd;

	/* Make sure we're opened for writing */
	if ((fh->mode & O_MODE_MASK) != O_WRONLY && (fh->mode & O_MODE_MASK) != O_RDWR)
		return -1;

	/* Check to make sure we have enough room in data */
	if (fh->loc + cnt > fh->filesize * 512) {
		/* Figure out the new block count */
		n = ((fh->loc + cnt) - (fh->filesize * 512));
		if (n & 511)
			n = (n+512) & ~511;
		n = n / 512;

#ifdef VMUFS_DEBUG
		dbglog(DBG_KDEBUG, "VMUFS: extending file's filesize by %d\n", n);
#endif
		
		/* We alloc another 512*n bytes for the file */
		tmp = realloc(fh->data, (fh->filesize + n) * 512);
		if (!tmp) {
			dbglog(DBG_ERROR, "VMUFS: unable to realloc another 512 bytes\n");
			return -1;
		}

		/* Assign the new pointer and clear out the new space */
		fh->data = tmp;
		memset(fh->data + fh->filesize * 512, 0, 512*n);
		fh->filesize += n;
	}

	/* insert the data in buffer into fh->data at fh->loc */
#ifdef VMUFS_DEBUG
	dbglog(DBG_KDEBUG, "VMUFS: adding %d bytes of data at loc %d (%d avail)\n",
		cnt, fh->loc, fh->filesize * 512);
#endif
	memcpy(fh->data + fh->loc, buffer, cnt);
	fh->loc += cnt;

	return cnt;
}

/* mmap a file */
/* note: writing past EOF will invalidate your pointer */
static void *vmu_mmap(void * hnd) {
	vmu_fh_t *fh;

	/* Check the handle */
	if (!vmu_verify_hnd(hnd, VMU_FILE))
		return NULL;

	fh = (vmu_fh_t *)hnd;

	return fh->data;
}

/* Seek elsewhere in a file */
static off_t vmu_seek(void * hnd, off_t offset, int whence) {
	vmu_fh_t *fh;

	/* Check the handle */
	if (!vmu_verify_hnd(hnd, VMU_FILE))
		return -1;

	fh = (vmu_fh_t *)hnd;

	/* Update current position according to arguments */
	switch (whence) {
		case SEEK_SET: break;
		case SEEK_CUR: offset += fh->loc; break;
		case SEEK_END: offset = fh->filesize * 512 - offset; break;
		default:
			return -1;
	}
	
	/* Check bounds; allow seek past EOF. */
	if (offset < 0) offset = 0;
	fh->loc = offset;
	
	return fh->loc;
}

/* tell the current position in the file */
static off_t vmu_tell(void * hnd) {
	/* Check the handle */
	if (!vmu_verify_hnd(hnd, VMU_FILE))
		return -1;

	return ((vmu_fh_t *) hnd)->loc;
}

/* return the filesize */
static size_t vmu_total(void * fd) {
	/* Check the handle */
	if (!vmu_verify_hnd(fd, VMU_FILE))
		return -1;

	/* note that all filesizes are multiples of 512 for the vmu */
	return (((vmu_fh_t *) fd)->filesize) * 512;
}

/* read a directory handle */
static dirent_t *vmu_readdir(void * fd) {
	vmu_dh_t	*dh;
	vmu_dir_t	*dir;

	/* Check the handle */
	if (!vmu_verify_hnd(fd, VMU_DIR)) {
		return NULL;
	}

	dh = (vmu_dh_t*)fd;

	/* printf("VMUFS: readdir on entry %d of %d\n", dh->entry, dh->dircnt); */

	/* Check if we have any entries left */
	if (dh->entry >= dh->dircnt)
		return NULL;
	
	/* printf("VMUFS: reading non-null entry %d\n", dh->entry); */
		
	/* Ok, extract it and fill the dirent struct */
	dir = dh->dirblocks + dh->entry;
	if (dh->rootdir)
		dh->dirent.size = -1;
	else
		dh->dirent.size = dir->filesize*512;
	strncpy(dh->dirent.name, dir->filename, 12);
	dh->dirent.name[12] = 0;
	dh->dirent.time = 0;	/* FIXME */
	dh->dirent.attr = 0;

	/* Move to the next entry */
	dh->entry++;

	return &dh->dirent;
}

/* Delete a file */
static int vmu_unlink(vfs_handler_t * vfs, const char *path) {
	maple_device_t	* dev = NULL;	/* address of VMU */

	/* convert path to valid VMU address */
	dev = vmu_path_to_addr(path);
	if (dev == NULL) {
		dbglog(DBG_ERROR, "VMUFS: vmu_unlink on invalid path '%s'\n", path);
		return -1;
	}

	return vmufs_delete(dev, path + 4);
}

static int vmu_stat(vfs_handler_t * vfs, const char * fn, stat_t * rv) {
	maple_device_t * dev;

	if (rv == NULL) {
		dbglog(DBG_ERROR, "vmu_stat: null output pointer\n");
		return -1;
	}
	
	/* The only thing we can stat right now is full VMUs, and what that
	   will get you is a count of free blocks in "size". */
	dev = vmu_path_to_addr(fn);
	if (dev == NULL) {
		dbglog(DBG_ERROR, "vmu_stat: couldn't resolve VMU name '%s'\n", fn);
		return -1;
	}

	/* Get the number of free blocks */
	rv->size = vmufs_free_blocks(dev);
	rv->dev = NULL;
	rv->unique = 0;
	rv->type = STAT_TYPE_DIR;
	rv->attr = STAT_ATTR_RW;
	rv->time = 0;

	return (rv->size < 0) ? -1 : 0;
}

/* handler interface */
static vfs_handler_t vh = {
	/* Name handler */
	{
		"/vmu",		/* name */
		0,		/* tbfi */
		0x00010000,	/* Version 1.0 */
		0,		/* flags */
		NMMGR_TYPE_VFS,	/* VFS handler */
		NMMGR_LIST_INIT
	},
	0, NULL,	/* In-kernel, privdata */
	
	vmu_open,
	vmu_close,
	vmu_read,
	vmu_write,	/* the write function */
	vmu_seek,	/* the seek function */
	vmu_tell,
	vmu_total,
	vmu_readdir,	/* readdir */
	NULL,		/* ioctl */
	NULL,		/* rename/move */
	vmu_unlink,	/* unlink */		
	vmu_mmap,	/* mmap */
	NULL,		/* complete */
	vmu_stat,	/* stat */
	NULL,		/* mkdir */
	NULL		/* rmdir */
};

int fs_vmu_init() {
	TAILQ_INIT(&vmu_fh);
	fh_mutex = mutex_create();
	return nmmgr_handler_add(&vh.nmmgr);
}

int fs_vmu_shutdown() {
	vmu_fh_t * c, * n;

	c = TAILQ_FIRST(&vmu_fh);
	while (c) {
		n = TAILQ_NEXT(c, listent);
		
		switch (c->strtype) {
		case VMU_DIR: {
			vmu_dh_t * dir = (vmu_dh_t *)c;
			free(dir->dirblocks);
			break;
		}

		case VMU_FILE:
			if ((c->mode & O_MODE_MASK) == O_WRONLY ||
				(c->mode & O_MODE_MASK) == O_RDWR)
			{
				dbglog(DBG_ERROR, "fs_vmu_shutdown: still-open file '%s' not written!\n", c->path);
			}
			free(c->data);
			break;
		}

		free(c);
		c = n;
	}
	
	if (fh_mutex != NULL)
		mutex_destroy(fh_mutex);
	fh_mutex = NULL;
	
	return nmmgr_handler_remove(&vh.nmmgr);
}

