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
int inodes_per_block = BLOCK_SIZE/sizeof(struct inode);
int root_ino = 0;


/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {
	// Step 1: Read inode bitmap from disk
	if(bio_read(superBlock->i_bitmap_blk,inodeBitmap) < 0){
		printf("Inode Bitmap Read Error");
	}
	int num = -1;
	// Step 2: Traverse inode bitmap to find an available slot
	for(int i = 0; i< MAX_INUM;i++){
			if(get_bitmap(inodeBitmap,i) == 0){
				num = i;
				set_bitmap(inodeBitmap,num);
				break;
			}
			if(num != -1){
				break;
			}
		
	}

	bio_write(superBlock->i_bitmap_blk,inodeBitmap);
	// Step 3: Update inode bitmap and write to disk 

	return num;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

	// Step 1: Read data block bitmap from disk

	if(bio_read(superBlock->d_start_blk,dataBlockBitmap) < 0){

		printf("Data Bitmap Read Error");
	}
	int num = -1;

	// Step 2: Traverse data block bitmap to find an available slot
	for(int i = 0; i< MAX_DNUM;i++){
			if(get_bitmap(dataBlockBitmap,i) == 0){
				num = i;
				set_bitmap(dataBlockBitmap,num);
				break;
			}
			if(num != -1){
				break;
			}
		}

	bio_write(superBlock->d_start_blk,dataBlockBitmap);

	// Step 3: Update data block bitmap and write to disk 

	return num;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
  if(ino > MAX_INUM){
	printf("ERROR: Inode out of range");
	return -1;
  }
	int block = ino/inodes_per_block;
	block += superBlock->i_start_blk;
  // Step 2: Get offset of the inode in the inode on-disk block
	uint16_t offset = (ino%inodes_per_block);
  // Step 3: Read the block from disk and then copy into inode structure
    void* tmp = malloc(BLOCK_SIZE);
	bio_read(block,tmp);
	memcpy(inode, tmp+offset,sizeof(struct inode));
	free(tmp);
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	if(ino > MAX_INUM){
	printf("ERROR: Inode out of range");
	return -1;
  }
	int block = ino/inodes_per_block;
	block += superBlock->i_start_blk;
	// Step 2: Get the offset in the block where this inode resides on disk
	uint16_t offset = (ino%inodes_per_block);
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
	
	void* buf = malloc(BLOCK_SIZE);
	struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

  for(int b = 0; b<16; b++){
	if(dir_inode->direct_ptr[b] == 0){
		printf("ERROR: End of directory or empty dir");
		free(buf);
		free(tmp);
		return -1;
	}

	bio_read(dir_inode->direct_ptr[b],buf);
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
  }
	free(tmp);
	free(buf);
	free(dir_inode);
	return -1;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode

	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
	for(int b = 0; b<16;b++){
		if(dir_inode.direct_ptr[b] == 0){
		break;
	}

		bio_read(dir_inode.direct_ptr[b],tmp);
	for(int i = 0; i< BLOCK_SIZE/sizeof(struct dirent);i++){
		
		if(tmp->valid == 1 && strcmp(tmp->name,fname)==0 ){
			perror("dir already exists!");
			free(tmp);
			return -1;
		}
		tmp++;
	}

	}

	// Step 3: Add directory entry in dir_inode's data block and write to disk
	int location = -1;
	struct dirent *dir_ent = (struct dirent*)malloc(sizeof(struct dirent));
	for(int b = 0; b < 16; b++){
		if(dir_inode.direct_ptr[b] == 0){//allocate new block
			dir_inode.direct_ptr[b] = get_avail_blkno();
			struct inode* new = (struct inode*)malloc(BLOCK_SIZE);
			bio_write(dir_inode.direct_ptr[b], new);
			free(new);
			dir_inode.vstat.st_blocks++ ;
		}
		bio_read(dir_inode.direct_ptr[b],tmp);
		dir_ent = tmp;
		for(int i = 0; i< BLOCK_SIZE/sizeof(struct dirent);i++){
		if(dir_ent->valid == 0){
			location = b;
			dir_ent->ino = f_ino;
			strcpy(dir_ent->name,fname);
			dir_ent->len = name_len;
			dir_ent->valid = 1;
			break;
		}
		dir_ent++;
	}
	if(location != -1){
		break;
	}
	}

	// Update directory inode
	//add something to edit inode modification time
	dir_inode.size += sizeof(struct dirent);
	dir_inode.link+= 1;
	writei(dir_inode.ino,&dir_inode);
	
	// Write directory entry
	bio_write(dir_inode.direct_ptr[location],tmp);
	free(tmp);
	free(dir_ent);
	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode

	int block;
	void* buf = malloc(BLOCK_SIZE);
	int entry_num = -1;

	// Step 2: Check if fname (directory name) is already used in other entries
	struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
	for(int b = 0; b<16; b++){
		if(dir_inode.direct_ptr[b] == 0){
		break;
	}
		block = dir_inode.direct_ptr[b];
		bio_read(block,buf);
	for(int i = 0; i< dir_inode.size/sizeof(struct dirent);i++){
		memcpy(tmp,buf+(i*sizeof(struct dirent)),sizeof(struct dirent));
		if(tmp->valid == 1 && strcmp(tmp->name,fname)==0 ){
			entry_num = i;
			break;
		}
	}
	if(entry_num != -1){
		break;
	}
	}
	// Step 2: Check if fname exist
	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
	if(entry_num !=-1){
		tmp->valid = 0;
		memcpy(buf+(entry_num*sizeof(struct dirent)),tmp,sizeof(struct dirent));
		bio_write(block,buf);
		dir_inode.size -= sizeof(struct dirent);
		dir_inode.link -=1;
		//edit dir_inode mod time
		writei(dir_inode.ino,&dir_inode);
		free(tmp);
		free(buf);
		return 0;
	}
	printf("Entry Not Found!");
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
	if(strcmp(path,"\0")){
		printf("ERROR: empty path!!");
		return -1;
	}
   
   char *delim = (char *)malloc(2*sizeof(char)); // Delimiter to split the path
   delim[0] = '/';
   delim[1] = '\0';
	char *paths = (char *)malloc(sizeof(path));
	char *saveptr;
    char *token = strtok_r(paths, delim,&saveptr);

	if(strcmp(path,delim) == 0){
		paths =NULL;
	}

	//temporary to read directory entries from data blocks
	struct dirent *tmp = (struct dirent*)malloc(sizeof(struct dirent));
	tmp->ino = 0; //root
    while (token != NULL) {
		//CODE TO SEARCH THROUGH ALL DATA BLOCKS OS THE INODE

		if(dir_find(tmp->ino,(const char *)token,(size_t)strlen(token),tmp) == -1){
			printf("Error: Path does mot exist");
			return -1;
		}
		//int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

		token = strtok_r(NULL, delim,&saveptr);
	}
	readi(tmp->ino,inode);
	
	return 0;
}
/* 
 * Make file system
 */
int rufs_mkfs() {

	// Call dev_init() to initialize (Create) Diskfile
	dev_init("/common/home/mc2432/os-project4/project4-release/DISKFILE");
	printf("here1");
	// write superblock information
	superBlock = (struct superblock*)malloc(BLOCK_SIZE);
	superBlock->magic_num = MAGIC_NUM;
	superBlock->max_dnum = MAX_DNUM;
	superBlock->max_inum = MAX_INUM;
	superBlock->i_bitmap_blk = 1;
	superBlock->d_bitmap_blk = 2;
	superBlock->i_start_blk = 3;
	superBlock->d_start_blk = (MAX_INUM*sizeof(struct inode))/BLOCK_SIZE + 3;

	if(bio_write(super_num, superBlock) < 0){

		printf("SuperBlock Write Failed");
	}

	// initialize inode bitmap
	inodeBitmap = (bitmap_t)malloc(BLOCK_SIZE);
	set_bitmap(inodeBitmap,0);
	if(bio_write(superBlock->i_bitmap_blk,inodeBitmap) < 0){

		printf("Inode Bitmap Write Failed");
	}
	// initialize data block bitmap
	dataBlockBitmap = (bitmap_t)malloc(BLOCK_SIZE);
	set_bitmap(dataBlockBitmap,0);
	if(bio_write(superBlock->d_bitmap_blk,dataBlockBitmap) < 0){
		printf("Data Block Bitmap Write Failed");
	}

	printf("all writes successes!");
	// initialize root directory
	struct inode *root = (struct inode*)malloc(sizeof(struct inode));
	bio_read(superBlock->i_start_blk, root) ;
	root->ino = root_ino;
	root->valid = 1;
	root->size = BLOCK_SIZE;
	root->type = DIR_TYPE;
	root->link = 0;

	root->direct_ptr[0] = superBlock->d_start_blk;
	root->direct_ptr[1] = 0;

	struct stat *rootStat = (struct stat *)malloc(sizeof(struct stat)) ;
	rootStat->st_mode = S_IFDIR | 0755 ;
	rootStat->st_nlink = 2 ;
	rootStat->st_ino = root->ino ;
	time(&rootStat->st_mtime) ;
	rootStat->st_blocks = 1 ;
	rootStat->st_blksize = BLOCK_SIZE ;
	rootStat->st_size = 2*sizeof(struct dirent) ;

	root->vstat = *rootStat ;
	//bio_write(superBlock->i_start_block,root);
	free(rootStat) ;

//SET UP ROOT DIRECTORY ENTRIES NEEDED HERE

	struct dirent* rootEnt = (struct dirent*)malloc(sizeof(struct dirent));
	rootEnt->ino = root->ino;					
	rootEnt->valid = 1;					
	strncpy(rootEnt->name, ".",2);					
	rootEnt->len = 2;

	struct dirent* parent = rootEnt+1;
	parent->ino = root->ino;					
	parent->valid = 1;					
	strncpy(parent->name, "..",3);							
	parent->len = 3;	

	bio_write(superBlock->d_start_blk, rootEnt) ;
	free(rootEnt) ;


	// update bitmap information for root directory
	inodeBitmap = (bitmap_t)malloc(BLOCK_SIZE);
	dataBlockBitmap = (bitmap_t)malloc(BLOCK_SIZE);

	bio_read(superBlock->i_bitmap_blk,inodeBitmap);
	bio_read(superBlock->d_bitmap_blk,dataBlockBitmap);

	set_bitmap(inodeBitmap,0);
	set_bitmap(dataBlockBitmap,0);

	bio_write(superBlock->i_bitmap_blk,inodeBitmap);
	bio_write(superBlock->d_bitmap_blk,dataBlockBitmap);

	// update inode for root directory
	writei(root->ino,root);
	free(root);
	return 0;
}


/* 
 * FUSE file operations
 */
static void *rufs_init(struct fuse_conn_info *conn) {

	// Step 1a: If disk file is not found, call mkfs
	int fd = dev_open("/common/home/mc2432/os-project4/project4-release/DISKFILE");
	if(fd == -1){
		printf("in mkfs");
		rufs_mkfs();
		return NULL;
	}


  // Step 1b: If disk file is found, just initialize in-memory data structures
  // and read superblock from disk
  //1b: read in all necessary globals
  printf("not in mkfs");
  superBlock = (struct superblock*)malloc(BLOCK_SIZE);
		if(bio_read(0,superBlock) != 0){
			printf("Super Block could not be read!");
	}

  inodeBitmap = (bitmap_t)malloc(BLOCK_SIZE);
  bio_read(superBlock->i_bitmap_blk,inodeBitmap);
  dataBlockBitmap = (bitmap_t)malloc(BLOCK_SIZE);
  bio_read(superBlock->d_bitmap_blk,dataBlockBitmap);

	return NULL;
}

static void rufs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures

	// Step 2: Close diskfile

}

static int rufs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path
	struct inode *inode = (struct inode*)malloc(sizeof(struct inode));
	if(get_node_by_path(path, 0,inode) < 0){
		printf("Error: getting path");
	}
	// Step 2: fill attribute of file into stbuf from inode
		*stbuf = inode->vstat ;
		//stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = inode->link;
		stbuf->st_size   = inode->size;
		time(&stbuf->st_mtime);

	return 0;
}

static int rufs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode *inode =(struct inode*) malloc(BLOCK_SIZE);
	inode->ino = 0;
	if(get_node_by_path(path,inode->ino,inode) == -1){
		printf("Path not found");
		free(inode);
		return -1;
	}
	if(inode->valid == 1){
		free(inode);
		return 0;
	}

	// Step 2: If not find, return -1

    return -1;
}

static int rufs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* inode = (struct inode*)malloc(BLOCK_SIZE);

	struct dirent* entry = (struct dirent*)malloc(BLOCK_SIZE);
	get_node_by_path(path,0,inode);
	// Step 2: Read directory entries from its data blocks, and copy them to filler
	for(int b = 0; b<16; b++){
	if(inode->direct_ptr[b] == 0){
		break;
	}

	bio_read(inode->direct_ptr[b],entry);
	for(int i = 0; i< BLOCK_SIZE/sizeof(struct dirent);i++){
		if(entry->valid == 1){
			struct inode* ent_inode = (struct inode*)malloc(BLOCK_SIZE);
			readi(entry->ino,ent_inode);
			filler(buffer,entry->name,&ent_inode->vstat,0);
		}
		entry ++;
		}

	}
	free(inode);
	free(entry);
	return 0;
}


static int rufs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	char *dirPath = (char*)malloc(strlen(path)+1);
	strcpy(dirPath,path);
	dirname(dirPath);

	//safely seperate baseName
	char *baseName = (char*)malloc(strlen(path)+1);;
	strcpy(baseName,path);
	basename(baseName);

	// Step 2: Call get_node_by_path() to get inode of parent directory
	struct inode *par_inode = (struct inode*)malloc(sizeof(struct inode));
	if(get_node_by_path(dirPath,par_inode->ino, par_inode)<0){
		printf("Path Find Error!!");
		return -1;
	}
	// Step 3: Call get_avail_ino() to get an available inode number
	int new_ino = get_avail_ino();									

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	dir_add(*par_inode, new_ino,baseName,sizeof(baseName));
	// Step 5: Update inode for target directory
	free(par_inode);
	//update inode info
	struct inode *dir_inode = (struct inode*)malloc(sizeof(struct inode));
	dir_inode-> ino = new_ino;			
	dir_inode-> valid = 1;			
	dir_inode-> size = BLOCK_SIZE;				
	dir_inode-> type = DIR_TYPE;	
	dir_inode->direct_ptr[0]= get_avail_blkno();	
	struct inode* blk = (struct inode*)malloc(BLOCK_SIZE);
	bio_write(dir_inode->direct_ptr[0],blk);
	free(blk);		
	dir_inode->direct_ptr[1]= 0;
	dir_inode->link = 2;

	//update vstat info 
	struct stat * statInfo = (struct stat *)malloc(sizeof(struct stat)) ;
	statInfo->st_mode = mode ; // Directory
	statInfo->st_nlink = 1 ;
	statInfo->st_ino = dir_inode->ino ;
	time(&statInfo->st_mtime) ;
	statInfo->st_blocks = 1 ;
	statInfo->st_blksize = BLOCK_SIZE ;
	statInfo->st_size = dir_inode->size ;

	dir_inode->vstat = *statInfo ;
	free(statInfo) ;

	// Step 6: Call writei() to write inode to disk
	writei(new_ino,dir_inode);

	//add . and ..
	struct dirent* root = (struct dirent*)malloc(sizeof(struct dirent));
	root->ino = new_ino;					
	root->valid = 1;					
	strncpy(root->name, ".",2);					
	root->len = 2;

	struct dirent* parent = root+1;
	parent->ino = new_ino;					
	parent->valid = 1;					
	strncpy(parent->name, "..",3);							
	parent->len = 3;	

	bio_write(dir_inode->direct_ptr[0],(const void*)root);			
	free(root);
	free(dir_inode);

	return 0;
}

//CAN SKIP

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

//CAN SKIP

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

TO START

make
mkdir -p /tmp/mc2432/mountdir
./rufs -s /tmp/mc2432/mountdir

cd benchmark
make
./simple_test

TO UNMOUNT AND BEGIN AGAIN
fusermount -u /tmp/mc2432/mountdir

NEW NEW
mkdir /tmp/mc2432/mountdir
make
./rufs -s /tmp/mc2432/mountdir
or
./rufs -d /tmp/mc2432/mountdir
cd /tmp/mc2432/mountdir

*/
/*
Current issu: when debugging, running into malloc issue at rootStat in rufs_mkfs

for every run

Detach with:
fusermount -u /tmp/mc2432/mountdir

delete DISKFILE

MAKE DIR
mkdir /tmp/mc2432/mountdir

ENTER DEBUG
gdb --args ./rufs -d /tmp/mc2432/mountdir

*/