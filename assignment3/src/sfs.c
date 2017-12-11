/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h> //for timestamps

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"


/******************************************************************/
//Didn't know if I should put this in a .h file, we can move if needed
//#definitions
#define INODEBLOCKS 289
#define DATABLOCKS 32479
#define BLOCKSPERINODE 112

//enums and structs
typedef enum _bool{
	FALSE, TRUE
}bool;

typedef enum _filetype{
	FILE_NODE, DIR_NODE
}filetype;

//This is probably going to need more data
typedef struct _inode{	
	int size; //size of file
	int numBlocks; //how many blocks it currently takes up total
	int blockNum[BLOCKSPERINODE]; //physical block numbers (remove some of these when adding other attributes)
	int links; //links to file (should always be 1 I'm pretty sure)
 	int indirectionBlock;//which block the inode continues
 	filetype type; //FILE_NODE or DIR_NODE (for extra credit)
 	mode_t filemode; //read/write/execute
 	time_t createtime;//created 
 	time_t modifytime;//last modified
 	time_t accesstime;//last accessed
	int userId; //for permissions
	int groupId; //for permissions
	const char* path; //file path
}inode;

//global vars
int blocks[INODEBLOCKS + DATABLOCKS]; //1 or 0

//more methods

//Finds the first inode set to 0
int findFirstFreeInode(){
	fprintf(stderr,"findFirstFreeInode\n");
	log_msg("findFirstFreeInode\n");
	int block = 0;
	while (block < INODEBLOCKS){
		log_msg("block #: %d\n", block);
		if (blocks[block] == 0){
			break;
		}
		block++;
	}
	if(block == INODEBLOCKS){
		return -1;
	}
	return block;	
}

//Finds the first regular datablock set to 0
int findFirstFreeData(){
	fprintf(stderr,"findFirstFreeData\n");
	log_msg("findFirstFreeData\n");
	int block = INODEBLOCKS;
	while (block < INODEBLOCKS + DATABLOCKS){
		log_msg("block #: %d\n", block);
		if (blocks[block] == 0){
			break;
		}
		block++;
	}
	if(block == INODEBLOCKS + DATABLOCKS){
		return -1;
	}
	return block;	
}


//open every inode and check path for match.
int findInode(const char* path){
	fprintf(stderr,"findInode\n");
	log_msg("findInode\n");
	int block = 0;
	char buffer[BLOCK_SIZE];
	while(block < INODEBLOCKS){
		log_msg("block #: %d\n", block);
		log_msg("blocks[block] = %d\n", blocks[block]);
		if(blocks[block] == 1){
			log_msg("in the 1st if statement\n");
			block_read(block, (void*)buffer);
			log_msg("input path: %s\n", path);
			
			log_msg("inode path: %s\n", ((inode*)buffer)->path);
			if(strcmp(((inode*)buffer)->path, path) == 0){
				log_msg("in the 2nd if statement\n");
				//the file was found/already exists
				break;
			}
			log_msg("out of 2nd\n");
		}
		log_msg("out of 1st\n");
		block++;
	}
	
	
	if(block == INODEBLOCKS){
		return -1; 
	}
	
	return block;
}

int checkPermissions(int block, int type){//type is 0 for open, 1 for read, 2 for write.  Functions will pass this in.
	char buffer[BLOCK_SIZE];
	block_read(block, buffer);
	mode_t mode = ((inode*)buffer)->filemode;
	
	//if user id matches, then check first 3 bits only
	if(getuid() == ((inode*)buffer)->userId){
		//check permissions
	}
	//otherwise if group, check middle 3
	else if(getgid() == ((inode*)buffer)->groupId){
		//check permissions
	}
	//otherwise check last 3 (other)
	else{
		//check permissions
	}
	return 0;
}

/******************************************************************/


///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */

void *sfs_init(struct fuse_conn_info *conn){
	fprintf(stderr, "in bb-init\n");
	log_msg("\nsfs_init()\n");
	disk_open(SFS_DATA->diskfile);
	
	//debugging statement so we can keep track of size while changing inodes.
	fprintf(stderr, "size of inode is: %d\n", sizeof(inode));
		
	//intializing arrays 
	int i = 0;
	while(i < INODEBLOCKS + DATABLOCKS){
		blocks[i] = 0;
		i++;
	}
	
	/* CREATE ROOT DIRECTORY */
	//declare buffer and zero out
	char buffer[BLOCK_SIZE];
	memset((void*)buffer, 0, BLOCK_SIZE);

	//cast buffer as inode and fill in inode with root information
	inode* root = (inode*)buffer;
	root->path = "/";
	root->size = 0;
	root->numBlocks = 0;
	root->links = 2;
	root->indirectionBlock = FALSE;
	root->type = DIR_NODE;
	root->filemode = S_IFDIR | 0755;
	root->createtime = time(NULL);
	root->accesstime = root->createtime;
	root->modifytime = root->createtime;
	root->userId = getuid();
	root->groupId = getgid();

	//now that inode is set, write it into the first inode (block 0 in our file) and update array
	block_write(0, (void*)root);
	blocks[0] = 1;

	log_conn(conn);
	log_fuse_context(fuse_get_context());
	log_msg("leaving init function\n");
	return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata){
  fprintf(stderr,"destroy");
  log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
  //close file
  disk_close(SFS_DATA->diskfile);
  //if we end up having to malloc something in init, free it here.
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int sfs_getattr(const char *path, struct stat *statbuf){
	fprintf(stderr, "in get attr\n");
	int retstat = 0;
	log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",	  path, statbuf);
	int block = findInode(path);
	if (block == -1){
		log_msg("if stmt of getattr\n");
		fprintf(stderr,"File not found\n");
		log_msg("File not found\n");
		return -ENOENT; //TODO: make sure this is right
	}
	else{
		fprintf(stderr,"in else\n");
		char buffer[BLOCK_SIZE];
		block_read(block, (void*)buffer);

		//fill in stat
		statbuf->st_dev = 0;
		statbuf->st_ino = 0;
		statbuf->st_rdev = 0;
		statbuf->st_mode = ((inode*)buffer)->filemode;
		statbuf->st_nlink = ((inode*)buffer)->links;
		statbuf->st_uid = ((inode*)buffer)->userId;
		statbuf->st_gid = ((inode*)buffer)->groupId;
		statbuf->st_size = ((inode*)buffer)->size;
		statbuf->st_atime = ((inode*)buffer)->accesstime;
		statbuf->st_mtime = ((inode*)buffer)->modifytime;
		statbuf->st_ctime = ((inode*)buffer)->createtime;
		statbuf->st_blocks = ((inode*)buffer)->numBlocks;
		fprintf(stderr,"getattr path is: %s\n", ((inode*)buffer)->path);
		log_msg("else stmt of getattr\n");
	}    
	log_msg("leaving getattr\n");
  return retstat;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
  int retstat = 0;
  fprintf(stderr,"create");
  log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);
   
  //check array of inodes for first one that is 0.  Save block number.
 	//write a separate function to do this.
	//ANNE: I made a function called findFirstFreeInode, if you wanna check the logic, all it does it return the first iNode index with 0;

  //create new char buffer: (
	char buffer[BLOCK_SIZE];
    
  //create new inode and cast buffer into it.
	int foundInd = findFirstFreeInode();
	if (foundInd == -1){
		fprintf(stderr, "No free nodes found\n");
		log_msg("no free inodes found\n");
		return -1;
	}
	
	//inode* useNode = (inode*)buffer;
	//ANNE: Definitely check over these declarations cause I'm only like 14% sure bout this
	((inode*)buffer)->size = 0;
	((inode*)buffer)->numBlocks = 0;
	((inode*)buffer)->links = 0;
	((inode*)buffer)->indirectionBlock = FALSE;
	((inode*)buffer)->type = FILE_NODE;//also not sure about this
	((inode*)buffer)->filemode = S_IFREG | mode; 		//S_ISREG 
	((inode*)buffer)->createtime = time(NULL);		
	((inode*)buffer)->modifytime = ((inode*)buffer)->createtime;
	((inode*)buffer)->accesstime = ((inode*)buffer)->createtime;
	((inode*)buffer)->userId = getuid(); 
	((inode*)buffer)->groupId = getgid(); 
	((inode*)buffer)->path = path;

	block_write(foundInd, (void*)buffer);

	fprintf(stderr,"%s %d\n", path, foundInd);
	blocks[foundInd] = 1;
	
	char* buffer2[BLOCK_SIZE];
	block_read(foundInd, buffer2);
	fprintf(stderr,"path is: %s\n", ((inode*)buffer2)->path);
	   
  //fill inode with information passed in and other info (same as init, but for file).
  	//path and mode are given in function call.
  		//along with mode given, use S_ISREG | mode

  //disk_write the buffer to the block number saved above.
    
  //change inode block number to 1 (to signify "used")
    
  return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path){
  fprintf(stderr,"unlink");
  int retstat = 0;
  log_msg("sfs_unlink(path=\"%s\")\n", path);
  int block = findInode(path);
	
	//check to see if any other blocks are linked (indirection)
	
	//clear out all datablocks in inode
		//for each datablock just remove from internal array (in inode) and external array
    
  //when datablocks are cleared, set path == NULL;
   
  //set inodes[] array block index back to 0.
    
  return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi){
  fprintf(stderr,"open");
  int retstat = 0;
  log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
  path, fi);
	
	//see if it exists.
	int block = findInode(path);

	if(block == -1){
		fprintf(stderr,"File does not exist\n");
		log_msg("File does not exist\n");
		retstat = -1; //TODO: check return values
	}
	else{
		int permission = checkPermissions(block, 0); //need to finish this function before it will work.
		if(permission == -1){
			retstat = -2; //TODO; check return values
		}
  }
  return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi){
	fprintf(stderr,"release");
	int retstat = 0;
	log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);
    
	//not really sure we have to add anything here since our open doesn't do much either.

  return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  fprintf(stderr,"read");
  int retstat = 0;
  log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	path, buf, size, offset, fi);
	int block = findInode(path);
	
	if(block == -1){
		fprintf(stderr, "File does not exist\n");
		log_msg("File does not exist\n");
		retstat = -1; //TODO: check return values
	}
	
	else{
		int permission = checkPermissions(block, 1); //need to finish this function before it will work
		if(permission == -1){
			retstat = -2; //TODO: check return values
		}
		else{
			char buffer[BLOCK_SIZE];
			block_read(block, buffer);
			
			//inode stored in buffer
				//calculate block for offset 
				//starting at offset, for each data block, start reading each datablock for until size reached.
		}
	}
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
  fprintf(stderr,"write");
  int retstat = 0;
  log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);
 	int block = findInode(path);
	
	if(block == -1){
		fprintf(stderr, "File does not exist\n");
		log_msg("File does not exist\n");
		retstat = -1; //TODO: check return values
	}
	
	else{
		int permission = checkPermissions(block, 2); //need to finish this function before it will work.
		if(permission == -1){
			retstat = -2; //TODO: check return values
		}
		else{
			char buffer[BLOCK_SIZE];
			block_read(block, buffer);
			
			//if there are existing blocks (internal array), check datablock array for how many bytes are currently written
				//if anything is less than 512, keep writing in that block
				//otherwise, go through data block array and find first one that is -1
			
			//if no existing blocks, go through data block array and firrst find one that is -1
			
			//block_write to that block
				//if writing to existing block, read the block into a buffer and then write to an offset.
			
			//store that block in the array in the inode
					
			//write the inode back in to the file
			
			//change data block array to total bytes written

		}
	}
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode){
  fprintf(stderr,"mkdir");
  int retstat = 0;
  log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
  return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path){
  fprintf(stderr,"rmdir");
  int retstat = 0;
  log_msg("sfs_rmdir(path=\"%s\")\n",	path);
  return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi){
  int retstat = 0;
  fprintf(stderr, "opendir\n");
  log_msg("\nsfs_opendir\n");//(path=\"%s\", fi=0x%08x)\n",path, fi);
  return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
  log_msg("\nsfs_readdir: %s\n", path);
	int retstat = 0;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0); 

	log_msg("in readdir\n");	
	
	char buffer [BLOCK_SIZE];
	int block = findInode(path);
	if(block == -1){
		fprintf(stderr,"Couldn't find file\n");
		log_msg("Couldn't find file\n");
		return -1; //TODO: is this right?
	}
	else{
		fprintf(stderr,"block: %d\n", block);
		block_read(block, buffer);
		if(((inode*)buffer)->type == DIR_NODE){
			char buffer2[BLOCK_SIZE];
			int i = 1;
			while(i < INODEBLOCKS){
				if(blocks[i] == 1){
					fprintf(stderr, "%d\n", i);
					block_read(i,buffer2);
					fprintf(stderr, "now path is: %s\n", ((inode*)buffer2)->path);
/*					//fill in stat
					statbuf->st_dev = 0;
					statbuf->st_ino = 0;
					statbuf->st_rdev = 0;
					statbuf->st_mode = ((inode*)buffer)->filemode;
					statbuf->st_nlink = ((inode*)buffer)->links;
					statbuf->st_uid = ((inode*)buffer)->userId;
					statbuf->st_gid = ((inode*)buffer)->groupId;
					statbuf->st_size = ((inode*)buffer)->size;
					statbuf->st_atime = ((inode*)buffer)->accesstime;
					statbuf->st_mtime = ((inode*)buffer)->modifytime;
					statbuf->st_ctime = ((inode*)buffer)->createtime;
					statbuf->st_blocks = ((inode*)buffer)->numBlocks;
*/					
//					filler(buf, newpath, NULL, 0);
//					log_msg("filled path: %s\n", newpath);
				}
				i++;
			}
			//read each inode in block and get name and stat info
			//filler(buf, "name", &stat, 0);
		}
		else{
			fprintf(stderr, "Not a directory\n");
			log_msg("Not a directory\n");
		}
	}
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi){
  int retstat = 0;
	fprintf(stderr,"releasedir");
  return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage(){
  fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
  abort();
}

int main(int argc, char *argv[]){
	fprintf(stderr, "in main\n");
   int fuse_stat;
   struct sfs_state *sfs_data;
   // sanity checking on the command line
   if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))	sfs_usage();
		sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
			perror("main calloc");
			abort();
    }
    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
