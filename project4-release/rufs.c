/*
 *  Copyright (C) 2023 CS416 Rutgers CS
 *	Tiny File System
 *	File:	rufs.c
 *
 */

#define FUSE_USE_VERSION 26

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
#include "rufs.h"

#define DIR_TYPE 2;

char diskfile_path[PATH_MAX];


// Declare your in-memory data structures here

//Inode Bitmap
bitmap_t inodeBitmap;
//Data Block Bitmap
bitmap_t dataBlockBitmap;
//Super Block
struct superblock* superBlock;
//Starting Numbers of important blocks 
int super_num = 0;
int ino_bit_num = 1;
int db_bit_num = 2;
int ino_start = 3;
int inodes_per_block = BLOCK_SIZE/sizeof(struct inode);


/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
	bitmap_t inodeBit = malloc(BLOCK_SIZE);
	// Step 1: Read inode bitmap from disk
	if(bio_read(ino_bit_num,inodeBit) < 0){
		free(inodeBit);
		printf("Inode Bitmap Read Error");
	}
	int num = -1;
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i< MAX_INUM/8;i++){
		for(int j = 0; j<8;j++){
			if(!(inodeBit[i] & (1 << j))){
				num = i*8 + j;
				set_bitmap(inodeBit,num);
				break;
			}
		}
		if(num != -1){
				break;
			}
	}
	bio_write(ino_bit_num,inodeBit);
	free(inodeBit);
	// Step 3: Update inode bitmap and write to disk 

	return num;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk
	bitmap_t dataBit = malloc(BLOCK_SIZE);
	if(bio_read(db_bit_num,dataBit) < 0){
		free(dataBit);
		printf("Data Bitmap Read Error");
	}
	int num = -1;

	// Step 2: Traverse data block bitmap to find an available slot
	for(int i = 0; i< MAX_DNUM/8;i++){
		for(int j = 0; j<8;j++){
			if(!(dataBit[i] & (1 << j))){
				num = i*8 + j;
				set_bitmap(dataBit,num);
				break;
			}
		}
		if(num != -1){
				break;
			}
	}
	bio_write(db_bit_num,dataBit);
	free(dataBit);
	// Step 3: Update data block bitmap and write to disk 

	return num;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
	int block = ino/inodes_per_block;
	block += ino_start;
  // Step 2: Get offset of the inode in the inode on-disk block
	uint16_t offset = (ino%inodes_per_block)*sizeof(struct inode);
  // Step 3: Read the block from disk and then copy into inode structure
    void* tmp = malloc(BLOCK_SIZE);
	bio_read(block,tmp);
	memcpy(inode, tmp+offset,sizeof(struct inode));
	free(tmp);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int block = ino/inodes_per_block;
	block += ino_start;
	// Step 2: Get the offset in the block where this inode resides on disk
	uint16_t offset = (ino%inodes_per_block)*sizeof(struct inode);
	// Step 3: Write inode to disk 
	void* tmp = malloc(BLOCK_SIZE);
	bio_read(block,tmp);
	memcpy(tmp+offset, inode, sizeof(struct inode));
	bio_write(block,tmp);
	free(tmp);
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
  struct inode *dir_inode = (struct inode*)malloc(sizeof(struct inode));
  readi(ino, dir_inode);
  // Step 2: Get data block of current directory from inode
	int block = dir_inode->direct_ptr[0];
	void* buf = malloc(BLOCK_SIZE);
	bio_read(block,buf);
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
  struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
	for(int i = 0; i< BLOCK_SIZE/sizeof(dirent);i++){
		memcpy(tmp,buf+(i*sizeof(struct dirent)),sizeof(struct dirent));
		if(strcmp(tmp->name,fname)==0 && tmp->valid == 1){
			memcpy(dirent,tmp,sizeof(struct dirent));
			free(tmp);
			free(buf);
			free(dir_inode);
			return 0;
		}
	}
	free(tmp);
	free(buf);
	free(dir_inode);
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	int num_blocks = dir_inode.size/BLOCK_SIZE;
	int block;
	void* buf = malloc(BLOCK_SIZE);
	int free_ent = -1;

	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
	for(int b = 0; b<num_blocks;b++){
		int block = dir_inode.direct_ptr[b];
		bio_read(block,buf);
	for(int i = 0; i< dir_inode.size/sizeof(struct dirent);i++){
		memcpy(tmp,buf+(i*sizeof(struct dirent)),sizeof(struct dirent));
		if(tmp->valid == 0){
			free_ent = i;
			break;
		}
		if(tmp->valid == 1 && strcmp(tmp->name,fname)==0 ){
			perror("dir already exists!");
			free(tmp);
			free(buf);
			return -1;
		}
	}
	}
	// Step 3: Add directory entry in dir_inode's data block and write to disk
	struct dirent *dir_ent = (struct dirent*)malloc(sizeof(struct dirent));
	dir_ent->ino = f_ino;
	strcpy(dir_ent->name,fname);
	dir_ent->len = name_len;
	dir_ent->valid = 1;

	// Allocate a new data block for this directory if it does not exist
   if(free_ent == -1){
		int block = get_avail_blkno();
		if(block <0){
			perror("block allocation failed!");
		}else{
			bio_read(block,buf);
			for(int b = 0; b <16; b++){
				if(dir_inode.direct_ptr[b] == 0){
					dir_inode.direct_ptr[b] = block;
					break;
				}
			}
			free_ent = 0;
		}
   }

	// Update directory inode
	//add something to edit inode modification time
	dir_inode.size += sizeof(struct dirent);
	dir_inode.link+= 1;
	bio_write(dir_inode.ino,&dir_inode);
	
	// Write directory entry
	memcpy(buf+(free_ent*sizeof(struct dirent)),dir_ent,sizeof(struct dirent));
	bio_write(block,buf);
	free(tmp);
	free(dir_ent);
	free(buf);
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	int num_blocks = dir_inode.size/BLOCK_SIZE;

	int block;
	void* buf = malloc(BLOCK_SIZE);
	int entry_num = -1;

	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
	for(int b = 0; b<num_blocks; b++){
		int block = dir_inode.direct_ptr[b];
		bio_read(block,buf);
	for(int i = 0; i< dir_inode.size/sizeof(struct dirent);i++){
		memcpy(tmp,buf+(i*sizeof(struct dirent)),sizeof(struct dirent));
		if(tmp->valid == 1 && strcmp(tmp->name,fname)==0 ){
			entry_num = i;
			break;
		}
	}
	}
	// Step 2: Check if fname exist
	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
	if(entry_num !=-1){
		tmp->valid = 0;
		bio_write(block,buf);
		dir_inode.size -= sizeof(struct dirent);
		dir_inode.link -=1;
		//edit dir_inode mod time
		free(tmp);
		free(buf);
		return 0;
	}
	free(tmp);
	free(buf);
	return -1;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	
	// Step 1: Resolve the path name, walk through path, and finally, find its inode.
	// Note: You could either implement it in a iterative way or recursive way
	

	return 0;
}

/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);

	// write superblock information
	superBlock = malloc(sizeof(struct superblock));
	superBlock->magic_num = MAGIC_NUM;
	superBlock->max_dnum = MAX_DNUM;
	superBlock->max_inum = MAX_INUM;
	superBlock->i_bitmap_blk = 1;
	superBlock->d_bitmap_blk = 2;
	superBlock->i_start_blk = 3;
	superBlock->d_start_blk = (MAX_INUM*sizeof(struct inode))/BLOCK_SIZE + 3;
	if(bio_write(super_num, (void *)superBlock) < 0){
		printf("SuperBlock Write Failed");
	}
	// initialize inode bitmap
	int inoSize = (MAX_INUM/8);
	inodeBitmap = (bitmap_t)malloc(inoSize*sizeof(unsigned char));
	memset(inodeBitmap, 0, inoSize);
	if(bio_write(ino_bit_num,(void *)inodeBitmap) < 0){
		printf("Inode Bitmap Write Failed");
	}
	// initialize data block bitmap
	int dbSize = (MAX_DNUM/8);
	dataBlockBitmap = (bitmap_t)malloc(dbSize*sizeof(unsigned char));
	memset(dataBlockBitmap, 0, dbSize);
	if(bio_write(db_bit_num,(void *)dataBlockBitmap) < 0){
		printf("Data Block Bitmap Write Failed");
	}
	printf("all writes successes!");
	// initialize root directory
	struct inode *root = (struct inode*)malloc(sizeof(struct inode));
	root->ino = 0;
	root->valid = 1;
	root->size = BLOCK_SIZE;
	root->type = DIR_TYPE;
	root->link = 2;

	int block = get_avail_blkno();
	void* dirBlock = malloc(BLOCK_SIZE);
	bio_read(block,dirBlock);
	root->direct_ptr[0] = block;
	// update bitmap information for root directory
	bitmap_t inodeBit = malloc(BLOCK_SIZE);
	bitmap_t dbBit = malloc(BLOCK_SIZE);

	bio_read(ino_bit_num,inodeBit);
	bio_read(db_bit_num,dbBit);

	set_bitmap(inodeBit,0);
	set_bitmap(dbBit,block);

	bio_write(ino_bit_num,inodeBit);
	bio_write(db_bit_num,dbBit);

	free(inodeBit);
	free(dbBit);
	// update inode for root directory
	writei(root->ino,root);
	free(root);
	free(dirBlock);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	int fd = open(diskfile_path, O_RDWR);
	if(fd < 0){
		rufs_mkfs();
	}else{
		//1b
		if(bio_read(0,superBlock) != 0){
			printf("Super Block could not be read!");
		//Data Structures Here
	}
	}
  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int rufs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int rufs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int rufs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int rufs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int rufs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int rufs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int rufs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int rufs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int rufs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations rufs_ope = {
	.init		= rufs_init,
	.destroy	= rufs_destroy,

	.getattr	= rufs_getattr,
	.readdir	= rufs_readdir,
	.opendir	= rufs_opendir,
	.releasedir	= rufs_releasedir,
	.mkdir		= rufs_mkdir,
	.rmdir		= rufs_rmdir,

	.create		= rufs_create,
	.open		= rufs_open,
	.read 		= rufs_read,
	.write		= rufs_write,
	.unlink		= rufs_unlink,

	.truncate   = rufs_truncate,
	.flush      = rufs_flush,
	.utimens    = rufs_utimens,
	.release	= rufs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &rufs_ope, NULL);

	return fuse_stat;
}

/*
make
mkdir -p /tmp/mc2432/mountdir
./rufs -s /tmp/mc2432/mountdir

cd benchark
make
./simple_test

*/