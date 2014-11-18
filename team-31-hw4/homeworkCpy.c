/*
 * file:        homework.c
 * description: skeleton file for CS 5600 homework 3
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, updated April 2012
 * $Id: homework.c 452 2011-11-28 22:25:31Z pjd $
 */

#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "cs5600fs.h"
#include "blkdev.h"

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 512-byte SECTORS, while the
 * file system uses 1024-byte BLOCKS. Remember to multiply everything
 * by 2.
 */

extern struct blkdev *disk;

// rename some of the structs
typedef struct cs5600fs_dirent dir_ent;
/* Global Variables */
struct cs5600fs_super *super = NULL;
struct cs5600fs_entry *fat = NULL;
unsigned int ROOT_START_BLK = 0;
const int MAX_NAME_LEN = 44;
const int DIR_ENT_NUM = 16;
#define IS_FILE 0
#define IS_DIR 1

/* Helper Functions */

int parse_path(char *path, char *buf[])
{
	//char *path1 = (char *)malloc(strlen(path) + 1);
	//strcpy(path1, path);
    int d = 0;
    //printf("DEBUG: path1 in parse path:%s\n", path1);
    buf[d] = strtok(path, "/");
    while(buf[d] != NULL){
    	//printf("bufffffddddddd:%d:%s\n", d, buf[d]);
    	d ++;
        buf[d] = strtok(NULL, "/");
    }
    return d;
}

int lookup(char *path, dir_ent *dir)
{
    char *names[DIR_ENT_NUM];
    //printf("DEBUG: before parse path:%s\n", path);
    int depth = parse_path(path, names);
    //printf("DEBUG: after parse path:%s\n", path);
    //printf("parse_path ok\n");
    //printf("depth = %d\n", depth);
    if(depth == 0)
    {
        *dir = super->root_dirent;
        //printf("lookup done\n");
        return SUCCESS;
    }

    int i, j, found;
    int start_blk = ROOT_START_BLK;
    dir_ent directory[DIR_ENT_NUM];
    for(i = 0; i < depth; i ++)
    {
    	//printf("%s\n", names[0]);
        found = -1;
        disk->ops->read(disk, start_blk * 2, 2, (void *) directory);
        for(j = 0; j < DIR_ENT_NUM; j ++)
        {
            if(directory[j].valid && strcmp(directory[j].name, names[i]) == 0)
            {
                found = j;
                //printf("DEBUG: found j: %d\n", j);
                start_blk = directory[j].start;
                break;
            }
        }
        if(found == -1){
        	//printf("DEBUG: oops%d\n", found);
            return -ENOENT;
        }
        if(i != depth - 1 && !directory[j].isDir)
            return -ENOTDIR;
                //*dir = directory[found];
    }
    *dir = directory[found];
    //printf("DEBUG: found: %d\n", found);
   	//printf("lookup done\n");
    return SUCCESS;
}

void convert(dir_ent *target, struct stat *sb)
{
	sb->st_dev = 0;
	sb->st_ino = 0;
	sb->st_mode = target->mode | (target->isDir ? S_IFDIR : S_IFREG);
	sb->st_nlink = 1;
	sb->st_uid = target->uid;
	sb->st_gid = target->gid;
	sb->st_rdev = 0;
	sb->st_size = target->length;
	sb->st_blksize = FS_BLOCK_SIZE;
	sb->st_blocks = (target->length / FS_BLOCK_SIZE) + (((target->length % FS_BLOCK_SIZE) != 0) ? 1 : 0);
	sb->st_atime = target->mtime;
	sb->st_mtime = target->mtime;
	sb->st_ctime = target->mtime;
}


// helper function
void update_dirs(char *path, int isValid, int new_len, 
	char *new_name, mode_t mode, struct utimbuf *ut)
{
	
	char *names[DIR_ENT_NUM];
    int depth = parse_path(path, names);
	int i, j, found;
	int curr_blk, start_blk = ROOT_START_BLK;
    dir_ent directory[DIR_ENT_NUM];
    //printf("DEBUG: set_file_length begin\n");
    //printf("DEBUG: depth: %d\n", depth);
    
	for(i = 0; i < depth; i ++)
	{
		found = -1;
        disk->ops->read(disk, start_blk * 2, 2, (void *) directory);
        //printf("DEBUG: new start:%s\n", directory[0].name);
        curr_blk = start_blk;
		for(j = 0; j < DIR_ENT_NUM; j ++)
		{
			if(directory[j].valid && strcmp(directory[j].name, names[i]) == 0)
            {
            	found = j;
                start_blk = directory[j].start;
                
                break;
            }
		}
	}

	// for unlink a file
	if(!isValid)
		directory[found].valid = isValid;
	// for set new length for the file
	else{
		if(new_len >= 0)
			directory[found].length = new_len;
		if(new_name != NULL)
			strcpy(directory[found].name, new_name);
		if(mode != -1)
			directory[found].mode = mode;

	//printf("DEBUG: curr_blk:%d\n", curr_blk);
	//printf("DEBUG: dir start:%d\n", directory[found].start);
		directory[found].mtime = time(NULL);
		if(ut != NULL)
			directory[found].mtime = ut->modtime;
	}
	disk->ops->write(disk, curr_blk*2, 2, (void *) directory);
	//printf("%d\n", result);
	//printf("DEBUG: set_file_length end\n");
}

/* init - this is called once by the FUSE framework at startup.
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void* hw3_init(struct fuse_conn_info *conn)
{
	super = (struct cs5600fs_super *)malloc(FS_BLOCK_SIZE);
    disk->ops->read(disk, 0, 2, (void*)super);

    ROOT_START_BLK = super->root_dirent.start;

    int fat_len = super->fat_len;
    fat = (struct cs5600fs_entry *) malloc(fat_len * FS_BLOCK_SIZE);
    disk->ops->read(disk, 2, fat_len * 2, (void*)fat);
    //printf("init ok\n");
    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in CS5600fs are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int hw3_getattr(const char *path, struct stat *sb)
{
	/*
	char *buf[50];
	memset(buf, 0, 50 * (sizeof(char *)));
	//parse the path first
	parse_path(path, buf);
	*/
	//printf("getattr start\n");
	
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);

	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	
	int result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	//printf("lookup success\n");
	// convert cs5600 direntry to struct stat
	convert(direntry, sb);
	
	/*
	sb->st_dev = 0;
	sb->st_ino = 0;
	sb->st_mode = direntry->mode | (direntry->isDir ? S_IFDIR : S_IFREG);
	sb->st_nlink = 1;
	sb->st_uid = direntry->uid;
	sb->st_gid = direntry->gid;
	sb->st_rdev = 0;
	sb->st_size = direntry->length;
	sb->st_blksize = 0;
	sb->st_blocks = (direntry->length / 1024) + (((direntry->length % 1024) != 0) ? 1 : 0);
	sb->st_atime = direntry->mtime;
	sb->st_mtime = direntry->mtime;
	sb->st_ctime = direntry->mtime;
	*/
	//printf("getattr ok\n");
	return SUCCESS;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int hw3_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
    char *name = NULL;
    struct stat sb;
	int i;

	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	//printf("readdir copy path done\n");
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	int result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	//read the block
	int next_start = direntry->start;
	dir_ent dir[DIR_ENT_NUM];
	result = disk->ops->read(disk, next_start * 2, 2, (void *)dir);
	if (result != SUCCESS)
		return result;

	memset(&sb, 0, sizeof(sb));

	for (i = 0; i < DIR_ENT_NUM; i++)
	{
		if (dir[i].valid)
		{
			/*
			sb.st_mode = dir[i].mode | (dir[i].isDir ? S_IFDIR : S_IFREG);
			sb.st_nlink = 1;
			sb.st_uid = direntry->uid;
			sb.st_gid = direntry->gid;
			sb.st_size = dir[i].length;
			sb.st_atime = sb.st_ctime = sb.st_mtime = dir[i].mtime;
			sb.st_blocks = (dir[i].length / 1024) + (((dir[i].length % 1024) != 0) ? 1 : 0);
			*/
			convert(&dir[i], &sb);
			name = dir[i].name;
			filler(buf, name, &sb, 0); // invoke callback function 
		}
	}
	//printf("readdir ok\n");
	return SUCCESS;
}

/* create - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *
 * If a file or directory of this name already exists, return -EEXIST.
 */
 // helper function
 // spilt the given path into the father path and the new file or dir name
void split_path(const char *path, char *f_path, char *new_name)
{
	// extract father path
	int i;
	//printf("DEBUG: split path:%s\n", path);
	for(i = strlen(path) - 1; i > 0; i --)
	{
		if(path[i] == '/')
			break;
	}
	//printf("i: %d\n", i);
	//f_path = (char *)malloc(i + 1);
	strncpy(f_path, path, i);
	f_path[i] = '\0';
	//printf("DEBUG: father path right after split:%s\n", f_path);
	// extract the new file or dir name
	//new_name = (char *)malloc(MAX_NAME_LEN);
	//i ++;
	int j = 0;
	for(i = i + 1; i < strlen(path); i ++)
	{
		new_name[j] = path[i];
		j ++;
	}
	new_name[j] = '\0';
	//printf("%s\n", new_name);
}

int allocate_new_from_fat()
{

	int free_blk_no = -1;
	int nentries_in_fat = (FS_BLOCK_SIZE / 4)*super->fat_len;
	int i = 0;
	int result = 0;

	for (i = 0; i < nentries_in_fat; i++)
	{
		if (!fat[i].inUse)
		{	
			// the first entry not inused
			fat[i].inUse = 1;
			fat[i].eof = 1;
			fat[i].next = 0;
			int blk_no = i / (nentries_in_fat / super->fat_len);
			int fat_blk_no = blk_no + 1;
			result = disk->ops->write(disk, fat_blk_no * 2, 2, (void*)fat + (blk_no*(FS_BLOCK_SIZE / 4)));
			if (result != SUCCESS)
				return result;
			free_blk_no = i;
			break;
		}
	}
	if (i == nentries_in_fat)
		return -ENOSPC;

	return free_blk_no;

}

int create_ent(const char *path, mode_t mode, int isDir)
{

	int i, result = 0;
	char *father_path = (char *)malloc(strlen(path));
	char *new_name = (char *)malloc(MAX_NAME_LEN);
	split_path(path, father_path, new_name);
	/*
	if(isDir == IS_FILE){
		printf("DEBUG: create file path:%s\n", path);
		printf("DEBUG: spilt good?\n");
		printf("DEBUG: father name:%s\n", father_path);
		printf("DEBUG: new name:%s\n", new_name);
	}*/
	//read father dir
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	result = lookup(father_path, direntry);
	int next_start = direntry->start;
	dir_ent father_dir[DIR_ENT_NUM];
	result = disk->ops->read(disk, next_start * 2, 2, (void *)father_dir);
	if (result != SUCCESS)
		return result;
	//if(isDir)
	//	printf("read father path good?\n");
	
	for (i = 0; i < DIR_ENT_NUM; i++)
	{

		//find the first invalid entry
		if (!father_dir[i].valid)
		{
			//printf("begin allocate\n");
			//find the first not inused entry in FAT and allocate it
			int new_start_number = allocate_new_from_fat();
			/*
			if(isDir){
				printf("allocate good?\n");
				printf("allocate #: %d\n", new_start_number);
			}
			*/
			//allocation the finding entry in the father dir
			memset(&father_dir[i], 0, sizeof(dir_ent));

			father_dir[i].valid = 1;
			father_dir[i].isDir = isDir; 
			father_dir[i].uid = getuid();
			father_dir[i].gid = getgid();
			father_dir[i].mode = mode;
			father_dir[i].mtime = time(NULL); //get creat time
			father_dir[i].start = new_start_number;
			father_dir[i].length = 0;  //initially as 0

			strcpy(father_dir[i].name, new_name);
			
			// write the block back 
			result = disk->ops->write(disk, next_start * 2, 2, (void *)father_dir);
			if (result != SUCCESS)
				return result;
			
			break;
			
		}
		
	}
	if (i == DIR_ENT_NUM)
		return -ENOSPC;
	
	return SUCCESS;
}

static int hw3_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
	//printf("DEBUG: come in?\n");
	int result = 0;
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	//printf("DEBUG: create file path: %s\n", path1);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//check if the dir name does not exist.
	result = lookup(path1, direntry);
	if (result == SUCCESS) 
		return -EEXIST;
	if (result != -ENOENT)
		return result;
	strcpy(path1, path);
	//printf("DEBUG: lookup good?\n");
	return create_ent(path, mode, IS_FILE);
	//printf("DEBUG: create good?\n");
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */ 
static int hw3_mkdir(const char *path, mode_t mode)
{
	//printf("DEBUG: come in?\n");
	int result = 0;
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//check if the dir name does not exist.
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	
	result = lookup(path1, direntry);
	//printf("DEBUG: lookup good?result: %d\n", result);
	if (result != -ENOENT)
		return result;
	if (result == SUCCESS)
		return -EEXIST;
	//printf("DEBUG: good?\n");
	strcpy(path1, path);
	return create_ent(path, mode, IS_DIR);
    //printf("DEBUG: mkdir good?\n");
    
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 */
static int hw3_unlink(const char *path)
{
	int result = 0;
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	// if it's a dir, return error
	if(direntry->isDir)
		return -EISDIR;

	// unlink the fat
	int start = direntry->start;
	while(1)
	{
		fat[start].inUse = 0;
		if(fat[start].eof)
		{
			fat[start].eof = 0;
			break;
		}
		start = fat[start].next;
	}
	// write fat back
	disk->ops->write(disk, 2, super->fat_len * 2, (void *)fat);
	strcpy(path1, path);
	update_dirs(path1, 0, -1, NULL, -1, NULL);
    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int hw3_rmdir(const char *path)
{
	int i, result = 0;
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;

	// if it's a dir, return error
	if(!direntry->isDir)
		return -ENOTDIR;

	dir_ent sub_ent[DIR_ENT_NUM];
	disk->ops->read(disk, direntry->start * 2, 2, (void *)sub_ent);
	for(i = 0; i < DIR_ENT_NUM; i ++)
	{
		if(sub_ent[i].valid)
			return -ENOTEMPTY;
	}
	fat[direntry->start].inUse = 0;
	// write fat back
	disk->ops->write(disk, 2, super->fat_len * 2, (void *)fat);
	strcpy(path1, path);
	update_dirs(path1, 0, -1, NULL, -1, NULL);

    return 0;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int hw3_rename(const char *src_path, const char *dst_path)
{
	//printf("DEBUG: come in:\n");
	int result = 0;
	char *src_path1 = (char *)malloc(strlen(src_path) + 1);
	strcpy(src_path1, src_path);
	char *dst_path1 = (char *)malloc(strlen(dst_path) + 1);
	strcpy(dst_path1, dst_path);
	dir_ent *src_ent = (dir_ent *)malloc(sizeof(dir_ent));
	dir_ent *dst_ent = (dir_ent *)malloc(sizeof(dir_ent));
	//check source file or directory
	result = lookup(src_path1, src_ent);
	if(result != SUCCESS)
		return result;
	// checkout if destination already exists
	result = lookup(dst_path1, dst_ent);
	if(result == SUCCESS)
		return -EEXIST;
	if(result == -ENOTDIR)
		return result;
	
	// check if src and dst are in the same dir
	char *src_father = (char *)malloc(strlen(src_path));
	char *src_name = (char *)malloc(MAX_NAME_LEN);
	split_path(src_path, src_father, src_name);

	char *dst_father = (char *)malloc(strlen(dst_path));
	char *dst_name = (char *)malloc(MAX_NAME_LEN);
	split_path(dst_path, dst_father, dst_name);
	//printf("DEBUG: dest name:%s\n", dst_name);

	if(strcmp(src_father, dst_father))
		return -EINVAL;

	strcpy(src_path1, src_path);
	update_dirs(src_path1, src_ent->valid, -1, dst_name, -1, NULL);
	
    return 0;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int hw3_chmod(const char *path, mode_t mode)
{
	int result = 0;
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	
	if(!direntry->isDir)
		mode &= 01777;
	strcpy(path1, path);
	update_dirs(path1, direntry->valid, -1, NULL, mode, NULL);

    return 0;
}
int hw3_utime(const char *path, struct utimbuf *ut)
{
	int result = 0;
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;

	strcpy(path1, path);
	update_dirs(path1, direntry->valid, -1, NULL, -1, ut);
    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int hw3_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
     
    if (len != 0)
	return -EINVAL;		/* invalid argument */
    
    int result = 0;
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	if(direntry->isDir)
		return -EISDIR;

	int start = direntry->start;
	if(!fat[start].eof)
	{
		fat[start].eof = 1;
		int next = fat[start].next;
		// unlink others
		while(fat[next].inUse)
		{
			fat[next].inUse = 0;
			if(fat[next].eof)
				break;
			next = fat[next].next;
		}
	}
	// write back fat
	disk->ops->write(disk, 2, super->fat_len * 2, (void *)fat);
	// write back direntry
	strcpy(path1, path);
	update_dirs(path1, direntry->valid, 0, NULL, -1, NULL);
    return 0;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= len, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int hw3_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
	int i, result = 0;
	// try to find the file first
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	//printf("DEBUG: read path%s\n", path1);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	//printf("DEBUG: found file!\n");
	//printf("DEBUG: found file length:%d\n", direntry->length);
	// if it's a dir, return error
	if(direntry->isDir)
		return -EISDIR;
	int file_len = direntry->length;
	// if offset >= len
	if(offset >= file_len)
		return 0;
	if(len + offset > file_len)
		len = file_len - offset;

	int blk_num = (file_len % FS_BLOCK_SIZE == 0)? 
					(file_len / FS_BLOCK_SIZE) : 
					(file_len / FS_BLOCK_SIZE + 1);
	int start_blk = offset / FS_BLOCK_SIZE;
	int start_offset = offset % FS_BLOCK_SIZE;
	int left = len;
	// find the first block to read
	int read_start = direntry->start;
	//int read_pre;
	for(i = 0; i < start_blk; i ++)
	{
		//read_pre = read_start;
		read_start = fat[read_start].next;
	}
	//printf("DEBUG: readStart first:%d\n", read_start);

	char dataBuf[FS_BLOCK_SIZE];
	int buf_offset = 0;
	for(i = start_blk; i < blk_num; i ++)
	{
		disk->ops->read(disk, read_start * 2, 2, dataBuf);
		//printf("readStart first:%d\n", read_start);
		//printf("dataBuf aaa: %s\n", dataBuf);
		//printf("start_offset:%d\n", start_offset);
		int read_length = FS_BLOCK_SIZE - start_offset;
		if(left < read_length)
		{
			memcpy(buf + buf_offset, dataBuf + start_offset, left);
			//printf("left: %d\n", left);
			buf_offset += left;
			left = 0;
			break;
		} else
		{
			memcpy(buf + buf_offset, dataBuf + start_offset, read_length);
			//printf("dataBuf bbb: %s\n", dataBuf);
			left -= read_length;
			read_start = fat[read_start].next;
			//printf("read_start later: %d\n", read_start);
			buf_offset += read_length;
			start_offset = 0;
		}
	}
	//printf("buf: %s\n\n", buf);
    return len - left;
}


/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 */
static int hw3_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
	//printf("DEBUG :WRITEEEEEEE:%s\n", path);
	int i, result = 0;
	// try to find the file first
	char *path1 = (char *)malloc(strlen(path) + 1);
	strcpy(path1, path);
	dir_ent *direntry = (dir_ent *)malloc(sizeof(dir_ent));
	//convert path into cs5600 direntry
	result = lookup(path1, direntry);
	if (result != SUCCESS)
		return result;
	//printf("DEBUG :WRITEEEEEEE:%s\n", path1);
	// if it's a dir, return error
	if(direntry->isDir)
		return -EISDIR;
	int file_len = direntry->length;
	// if offset >= len
	if(offset > file_len)
		return -EINVAL;
	if(len + offset > file_len){
		direntry->length = len + offset;
		strcpy(path1, path);
		//printf("DEBUG: new length : %d\n", direntry->length);
		update_dirs(path1, direntry->valid, direntry->length, NULL, -1, NULL);
	}
	
	//printf("DEBUG: write file length:%d\n", direntry->length);
	
	int blk_num = ((len + offset) % FS_BLOCK_SIZE == 0)? 
					((len + offset) / FS_BLOCK_SIZE) : 
					((len + offset) / FS_BLOCK_SIZE + 1);
	int start_blk = offset / FS_BLOCK_SIZE;
	int start_offset = offset % FS_BLOCK_SIZE;
	int left = len;
	//printf("DEBUG: start_blk:%d\n", start_blk);
	//printf("DEBUG: blk_num:%d\n", blk_num);
	// find the first block to read
	int write_start = direntry->start;
	for(i = 0; i < start_blk; i ++)
		write_start = fat[write_start].next;

	int write_blk = write_start;
	for(i = start_blk + 1; i < blk_num; i ++)
	{
		if(fat[write_blk].eof)
		{
			fat[write_blk].eof = 0;
			int new_blk = allocate_new_from_fat();
			if(new_blk < 0)
				return new_blk;
			fat[write_blk].next = new_blk;
		}
		write_blk = fat[write_blk].next;
	}

	char dataBuf[FS_BLOCK_SIZE];
	int buf_offset = 0;
	for(i = start_blk; i < blk_num; i ++)
	{
		disk->ops->read(disk, write_start * 2, 2, dataBuf);
		int space_length = FS_BLOCK_SIZE - start_offset;
		if(left < space_length)
		{
			memcpy(dataBuf + start_offset, buf + buf_offset, left);
			disk->ops->write(disk, write_start * 2, 2, dataBuf);
			buf_offset += left;
			left = 0;
			break;
		}else{
			memcpy(dataBuf + start_offset, buf + buf_offset, space_length);
			disk->ops->write(disk, write_start * 2, 2, dataBuf);
			left -= space_length;
			write_start = fat[write_start].next;
			buf_offset += space_length;
			start_offset = 0;
		}
	}

    return len - left;
}

//helper function
int cal_used_blocks()
{
	int i, num = 0;
	for(i = 0; i < super->fat_len; i ++)
	{
		if(fat[i].inUse)
			num ++;
	}
	return num;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
static int hw3_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */
    st->f_bsize = FS_BLOCK_SIZE;
    st->f_blocks = 1024 - (1+ super->fat_len);
    st->f_bfree = st->f_blocks - cal_used_blocks();
    st->f_bavail = st->f_bfree;
    st->f_namemax = MAX_NAME_LEN;

    st->f_frsize = 0;
    st->f_files = 0;
    st->f_ffree = 0;
    st->f_favail = 0;
    st->f_fsid = 0;
    st->f_flag = 0;
    return 0;
}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw3_ops'.
 */
struct fuse_operations hw3_ops = {
    .init = hw3_init,
    .getattr = hw3_getattr,
    .readdir = hw3_readdir,
    .create = hw3_create,
    .mkdir = hw3_mkdir,
    .unlink = hw3_unlink,
    .rmdir = hw3_rmdir,
    .rename = hw3_rename,
    .chmod = hw3_chmod,
    .utime = hw3_utime,
    .truncate = hw3_truncate,
    .read = hw3_read,
    .write = hw3_write,
    .statfs = hw3_statfs,
};

