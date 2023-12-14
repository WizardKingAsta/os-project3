/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *
 *	File:	rufs.h
 *
 */

#include <linux/limits.h>
#include <sys/stat.h>
#include <unistd.h>

/* 
 * Moved these includes to the header file so that test programs
 * can use run_rufs to simulate running rufs from the command line.
 * This still allows rufs to be compiled with the original Makefile 
 * and function as intended.
*/
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>
#include "block.h"


#ifndef _TFS_H
#define _TFS_H

#define MAGIC_NUM 0x5C3A
#define MAX_INUM 1024
#define MAX_DNUM 16384


struct superblock {
	uint32_t	magic_num;			/* magic number */
	uint16_t	max_inum;			/* maximum inode number */
	uint16_t	max_dnum;			/* maximum data block number */
	uint32_t	i_bitmap_blk;		/* start block of inode bitmap */
	uint32_t	d_bitmap_blk;		/* start block of data block bitmap */
	uint32_t	i_start_blk;		/* start block of inode region */
	uint32_t	d_start_blk;		/* start block of data block region */
};

struct inode {
	uint16_t	ino;				/* inode number */
	uint16_t	valid;				/* validity of the inode */
	uint32_t	size;				/* size of the file */
	uint32_t	type;				/* type of the file */
	uint32_t	link;				/* link count */
	int			direct_ptr[16];		/* direct pointer to data block */
	int			indirect_ptr[8];	/* indirect pointer to data block */
	struct stat	vstat;				/* inode stat */
};

struct dirent {
	uint16_t ino;					/* inode number of the directory entry */
	uint16_t valid;					/* validity of the directory entry */
	char name[208];					/* name of the directory entry */
	uint16_t len;					/* length of name */
};


/*
 * bitmap operations
 */
typedef unsigned char* bitmap_t;

void set_bitmap(bitmap_t b, int i);
void unset_bitmap(bitmap_t b, int i);
uint8_t get_bitmap(bitmap_t b, int i);


int get_avail_ino();
int get_avail_blkno();
int readi(uint16_t ino, struct inode *inode);
int writei(uint16_t ino, struct inode *inode);
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent);
int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len);
int dir_remove(struct inode dir_inode, const char *fname, size_t name_len);
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode);
int rufs_mkfs();

static void *rufs_init(struct fuse_conn_info *conn);
static void rufs_destroy(void *userdata);
static int rufs_getattr(const char *path, struct stat *stbuf);
static int rufs_opendir(const char *path, struct fuse_file_info *fi);
static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int rufs_mkdir(const char *path, mode_t mode);
static int rufs_rmdir(const char *path);
static int rufs_releasedir(const char *path, struct fuse_file_info *fi);
static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int rufs_open(const char *path, struct fuse_file_info *fi);
static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi);
static int rufs_unlink(const char *path);


static int rufs_truncate(const char *path, off_t size);
static int rufs_release(const char *path, struct fuse_file_info *fi);
static int rufs_flush(const char * path, struct fuse_file_info * fi);
static int rufs_utimens(const char *path, const struct timespec tv[2]);


int run_rufs(int argc, char *argv[]);

#endif
