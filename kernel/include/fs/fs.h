#ifndef FS_H
#define FS_H

#include <types.h>
#include "pm.h"

/*
 * Path seperator and root directory
 */
#define FS_PATH_SEP '/'
#define FS_ROOT_DIR '/'

#define FS_MAX_SYMLOOP 20

/*
 * maximal length for a file name
 */
#define FS_MAX_NAME 256

/*
 * Inode type flags.
 * Note: FS_MOUNTP and FS_DIR can be or'd together
 */
#define FS_FILE     0x01
#define FS_DIR      0x02
#define FS_CHARDEV  0x03
#define FS_BLOCKDEV 0x04
#define FS_PIPE     0x05
#define FS_SYMLINK  0x06
#define FS_MOUNTP   0x08

/*
 * Fstype flags
 */
#define FST_NEED_DEV 0x01

/*
 * Mount flags
 */
#define FSM_READ   0x01
#define FSM_WRITE  0x02
#define FSM_EXEC   0x03
#define FSM_UMOUNT 0x08

struct inode;
struct superblock;
struct dirent;

/*
 * Operations that can be done on an inode
 */
typedef struct inode_ops
{
	int (*open)(struct inode* inode, dword flags);
	int (*close)(struct inode* inode);

	int (*read)(struct inode* inode, dword offset, void *buffer, dword size);
	int (*write)(struct inode* inode, dword offset, void *buffer, dword size);

	struct dirent* (*readdir)(struct inode *inode, dword index);
	struct inode*  (*finddir)(struct inode *inode, char *name);
} inode_ops_t;

/*
 * Any inode data comes here
 */
typedef struct inode
{
	char *name;
	dword flags;

	dword mask;
	dword length;

	uid_t uid;
	gid_t gid;

	dword impl;

	struct inode *link;

	inode_ops_t *ops;
	struct superblock *sb;
} inode_t;

typedef struct sb_ops
{
	void (*read_inode)(inode_t *inode);
	void (*write_inode)(inode_t *inode);
	void (*release_inode)(inode_t *inode);

	void (*write_super)(struct superblock *sb);
	void (*release_super)(struct superblock *sb);

	int  (*remount)(struct superblock *sb, dword flags);
} sb_ops_t;

typedef struct superblock
{
	inode_t *root;

	sb_ops_t *ops;
} superblock_t;

typedef struct fstype
{
	char  *name;
	dword flags;
	int (*get_sb)(superblock_t *sb, char *device, int flags);
} fstype_t;

typedef struct dirent
{
	char name[FS_MAX_NAME];
	dword inode;
} dirent_t;

/* The root of the file system */
extern inode_t *fs_root;

/**
 *  fs_register(type)
 *
 * Registers a new filesystem type.
 */
int fs_register(fstype_t *type);

/**
 *  fs_unregister(type)
 *
 * Unregisters a previously registered filesystem type.
 */
int fs_unregister(fstype_t *type);

/**
 *  fs_find_type(name)
 *
 * Returns the filesystem type with the given name or 0.
 */
fstype_t *fs_find_type(char *name);

int fs_open(inode_t *inode);
int fs_close(inode_t *inode);
int fs_read(inode_t *inode, dword offset, void *buffer, dword size);
int fs_write(inode_t *inode, dword offset, void *buffer, dword size);
struct dirent *fs_readdir(inode_t *inode, dword index);
inode_t *fs_finddir(inode_t *inode, char *name);

#endif /*FS_H*/
