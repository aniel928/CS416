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

/********* ADDED DEFINITIONS *********/
#define INODEBLOCKS 323
#define DATABLOCKS 32445
#define BLOCKSPERINODE 100
#define PATHSIZE 50


/********* ADDED ENUMS / STRUCTS *********/
typedef enum _bool{
	FALSE, TRUE
}bool;

typedef enum _filetype{
	FILE_NODE, DIR_NODE
}filetype;

typedef struct _inode{	
	int size; //size of file
	int numBlocks; //files: how many blocks it currently takes up total; dir: how many files assigned
	int blockNum[BLOCKSPERINODE]; //physical block numbers (remove some of these when adding other attributes)
	int links; //links to file (should always be 1 I'm pretty sure)
 	int indirectionBlock;//which block the inode continues;
 	filetype type; //FILE_NODE or DIR_NODE (for extra credit)
 	mode_t filemode; //read/write/execute
 	time_t createtime;//created 
 	time_t modifytime;//last modified
 	time_t accesstime;//last accessed
	int userId; //for permissions
	int groupId; //for permissions
	char path[PATHSIZE]; //file path
}inode;

/********* GLOBAL VARAIBLES *********/
char blocks[INODEBLOCKS + DATABLOCKS]; //1 or 0 (used unused)

/********* ADDED FUNCTIONS *********/
//Finds the first inode set to 0
int findFirstFreeInode(){
	fprintf(stderr,"findFirstFreeInode\n");
	log_msg("findFirstFreeInode\n");
	int block = 0;
	while (block < INODEBLOCKS){
		if (blocks[block] == '0'){
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
		if (blocks[block] == '0'){
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
	memset(buffer, 0, BLOCK_SIZE);
	while(block < INODEBLOCKS){
		if(blocks[block] == '1'){
			block_read(block, (void*)buffer);
			log_msg("input path: %s\n", path);			
			log_msg("inode path: %s\n", ((inode*)buffer)->path);
			if(strcmp(((inode*)buffer)->path, path) == 0){
				//the file was found/already exists
				break;
			}
		}
		block++;
	}	
	if(block == INODEBLOCKS){
		return -1; 
	}
	
	return block;
}

//strip current file from path and return path to file (parentDir path)
int getParentDir(char* path){
	log_msg("getParentDir, path: %s\n", path);
	int length = strlen(path);
	//start at end and move back until you find '/'
	int i = length;
	while(i > 0){
		if(path[i] == '/'){
			break;
		}
		i--;
	}
	//if it starts at end and breaks at 0, then the only '/' in this path is root.  So root is the parent.
	if(i==0){
		path[0] = '/';
		path[1] = '\0';	
		log_msg("path: %s\n", path);	
	}
	//otherwise return everything before this index (because there is stuff before the slash if we're not at index 0.
	else{
		memset(path + i, '\0', PATHSIZE - i);
		log_msg("path: %s\n", path);	
	}
	return 0;
}

//strip path to file and return current filename only
int getShortPath(char* path){
	log_msg("getShortPath, path: %s\n", path);
	int length = strlen(path);
	//start at end and move back until you find '/'
	if(length == 1){
		path[1] = '\0';
		return 0;
	}
	int i = length;
	while(i > 0){
		if(path[i] == '/'){
			break;
		}
		i--;
	}
	//buffer to hold filename, then copy the from the index to the end into this new buffer
	char path2[PATHSIZE];
	memset(path2, '\0', PATHSIZE);
	memcpy(path2, path + i + 1, PATHSIZE - (i + 1));
	//Now clear out original path and memcpy path2 back in (b/c we were given a pointer, it's this one that must be updated.)
	memset(path, '\0', PATHSIZE);
	memcpy(path, path2, PATHSIZE);
	log_msg("path: %s\n", path);	
	return 0;
}

/********* FUNCTIONS PROVIDED BY FUSE *********/
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

	/* INITIALIZE ARRAY */
	int i = 0;
	while(i < INODEBLOCKS + DATABLOCKS){
		blocks[i] = '0';
		i++;
	}
	
	/* CREATE ROOT DIRECTORY */
	//declare buffer and zero out
	char buffer[BLOCK_SIZE];
	memset((void*)buffer, 0, BLOCK_SIZE);

	//cast buffer as inode and fill in inode with root information
	inode* root = (inode*)buffer;
	memcpy(root->path, "/",2);
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
	blocks[0] = '1';

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
  fprintf(stderr,"destroy\n");
  log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);

  //close file
  disk_close(SFS_DATA->diskfile);

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

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",	  path, statbuf);

	//take path and find inode
	int block = findInode(path);
	//if not found, return error message.
	if (block == -1){
		fprintf(stderr,"File not found\n");
		log_msg("File not found\n");
		return -ENOENT; 
	}
	//otherwise, it was found, so set the stat that was passed in.
	else{
		//read the inode into a buffer
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, (void*)buffer);

		//fill in stat with info from inode
		statbuf->st_dev = 0; //spec says to initialize to 0 if we don't have a value
		statbuf->st_ino = block; //our inode ID
		statbuf->st_rdev = 0; //spec says to initialize to 0 if we don't have a value.
		statbuf->st_mode = ((inode*)buffer)->filemode;
		statbuf->st_nlink = ((inode*)buffer)->links;
		statbuf->st_uid = ((inode*)buffer)->userId;
		statbuf->st_gid = ((inode*)buffer)->groupId;
		statbuf->st_size = ((inode*)buffer)->size;
		statbuf->st_atime = ((inode*)buffer)->accesstime;
		statbuf->st_mtime = ((inode*)buffer)->modifytime;
		statbuf->st_ctime = ((inode*)buffer)->createtime;
		statbuf->st_blocks = ((inode*)buffer)->numBlocks;
		log_msg("stat filled for %s\n", ((inode*)buffer)->path);
	}    
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
	fprintf(stderr,"create\n");

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n", path, mode, fi);

	//check to make sure path doens't already exist
	int block = findInode(path);
	if(block != -1){
		retstat = EEXIST;
	}
	else{
		//declare a buffer and zero out
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);    

		//check for first free inode, 
		int foundInd = findFirstFreeInode();
		if (foundInd == -1){
			fprintf(stderr, "No free nodes found\n");
			log_msg("no free inodes found\n");
			return ENOSPC;
		}

		//create new inode and cast buffer into it.
		inode* useNode = (inode*)buffer;
		useNode->size = 0;
		useNode->numBlocks = 0;
		useNode->links = 0;
		useNode->indirectionBlock = -1; //start as -1 (some invalid block number)
		useNode->type = FILE_NODE;
		useNode->filemode = S_IFREG | mode;
		useNode->createtime = time(NULL);		
		useNode->modifytime = useNode->createtime;
		useNode->accesstime = useNode->createtime;
		useNode->userId = getuid(); 
		useNode->groupId = getgid(); 

		//copy path passed in into inode->path
		strncpy(useNode->path, path, strlen(path));
		useNode->path[strlen(path)] = '\0';

		//that's it, right it to the file and update blocks array
		block_write(foundInd, (void*)buffer);
		blocks[foundInd] = '1';

		//now figure out who this guy's parent is
		char parentDir[PATHSIZE];
		memcpy(parentDir, path, PATHSIZE);
		getParentDir(&(parentDir[0]));
	
		//go get the parent and update modifytime and numBlocks (for directories numBlocks is the number of files linked).
		int parentBlock = findInode(parentDir);
		char buffer3[BLOCK_SIZE];
		block_read(parentBlock, buffer3);
		((inode*)buffer3)->numBlocks++;
		((inode*)buffer3)->modifytime = time(NULL);
			
		//because it was updated, write it back
		block_write(parentBlock, buffer3);
    }
  return retstat;
}

/** Remove a file */
int sfs_unlink(const char *path){
	fprintf(stderr,"unlink");
	int retstat = 0;

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}


	log_msg("sfs_unlink(path=\"%s\")\n", path);

	//find inode, if doen'st exist return error
	int block = findInode(path);
	if(block == -1){
		return -ENOENT;
	}
	
	//otherwise read inode into new buffer.
	char buffer [BLOCK_SIZE];
	memset(buffer, 0, BLOCK_SIZE);
	block_read(block, buffer);
		
	//declare zeroed out buffer to clear out blocks in file
	char buffer2[BLOCK_SIZE];
	memset(buffer2, 0, BLOCK_SIZE);
	
	//go through each data block that belongs to file
	int i = 0;
	while(i < BLOCKSPERINODE){
		if(((inode*)buffer)->blockNum[i] != 0){
			//zero out a buffer and write that into the block so old data does sneak in			
			block_write(((inode*)buffer)->blockNum[i],buffer2);
			//tell external array this block is free
			blocks[((inode*)buffer)->blockNum[i]] = 0;
			//set internal array to 0 (so when new inode comes in it is initialized properly
			((inode*)buffer)->blockNum[i] = 0;
		}
		i++;
	}
	
	//when datablocks are cleared, set path == NULL;
	memset(((inode*)buffer)->path, '\0',50);

  	//set inodes[] array block index back to 0.
	blocks[block] = 0;
	
	//find this file's parent directory
	char parentDir[PATHSIZE];
	memcpy(parentDir, path, PATHSIZE);
	getParentDir(&(parentDir[0]));
	
	//read in that inode and update modify time and decrement numBlocks (numBlocks in directory is number of files)
	int parentBlock = findInode(parentDir);
	char buffer3[BLOCK_SIZE];
	block_read(parentBlock, buffer3);
	((inode*)buffer3)->numBlocks--;
	((inode*)buffer3)->modifytime = time(NULL);
	//we made a change, so write it back in.	
	block_write(parentBlock, buffer3);

	//and finally, clear out this inode to be super safe
	block_write(block, buffer2);
	
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
	log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n", path, fi);
  
  	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	//see if it exists.
	int block = findInode(path);
	//if doesn't exist, return error
	if(block == -1){
		fprintf(stderr,"File does not exist\n");
		log_msg("File does not exist\n");
		retstat = -ENOENT;
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
	fprintf(stderr,"release\n");
	int retstat = 0;
	
	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n", path, fi);

    //make sure the path exists
	int block = findInode(path);
	if(block == -1){
		retstat = -ENOENT;
	}
	else{
		//if it does, update access time.
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, buffer);
		((inode*)buffer)->accesstime = time(NULL);
		block_write(block, buffer);
	}

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
	fprintf(stderr,"read\n");
	int retstat = 0;
	
	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",path, buf, size, offset, fi);
	
	//get inode
	int block = findInode(path);
	//if doesn't exist, return error.
	if(block == -1){
		fprintf(stderr, "File does not exist\n");
		log_msg("File does not exist\n");
		retstat = -ENOENT; 
	}
	else{
		//read the inode in from file
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, buffer);

		//figure out where to start, how many blocks and how tow write it.
		int startBlock = offset / BLOCK_SIZE; //integer division automatically rounds down.
		int startIndex = offset % BLOCK_SIZE; //this is the index to start at in that block.

		//now create a zeroed out buffer to store data in from file.
		char tempbuff[BLOCK_SIZE];
		memset(tempbuff, 0, BLOCK_SIZE);

		int bytesread = 0;
		while(bytesread < size){
			//first read might have offset, start there
			if(bytesread == 0){
				log_msg("about to read from data block %d\n", ((inode*)buffer)->blockNum[startBlock]);
				block_read(((inode*)buffer)->blockNum[startBlock], tempbuff);
				memcpy(buf + bytesread, tempbuff, BLOCK_SIZE - startIndex);
				bytesread += (BLOCK_SIZE - startIndex);
			}
			//the last thing being read might be less than BLOCK_SIZE, so read that.
			else if (size - bytesread < BLOCK_SIZE){
				block_read(((inode*)buffer)->blockNum[startBlock], tempbuff);
				memcpy(buf + bytesread, tempbuff, size - bytesread);
				bytesread = size;
			}
			//otherwise just read in a full block.
			else{
				block_read(((inode*)buffer)->blockNum[startBlock],tempbuff);
				memcpy(buf + bytesread, tempbuff, BLOCK_SIZE);
				bytesread += BLOCK_SIZE;
			}
			startBlock++;
		}
		log_msg("read returning: %s, strlen: %d\n", buf, strlen(buf));
		retstat = strlen(buf);
	}
	log_msg("return stat: %d, about to leave read\n", retstat);	
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

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n", path, buf, size, offset, fi);

	//find path of file and make sure it exists
 	int block = findInode(path);
	if(block == -1){
		fprintf(stderr, "File does not exist\n");
		log_msg("File does not exist\n");
		retstat = -ENOENT;
	}	
	else{
		//read inode into buffer
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, buffer);

		//you're asking to write more than i can give you
		if((((inode*)buffer)->size + size) > (BLOCK_SIZE * BLOCKSPERINODE)){
			log_msg("Size too big\n");
			fprintf(stderr, "Size too big\n");
			retstat = ENOSPC; //not enough space in this file to finish.
		}
		else{

			int count = 0;//for how many blocks were used.
		
			//store info about where to start and how far to go, etc.
			int firstBlock = offset / BLOCK_SIZE; //this tells you where to go in your internal array.  Integer division rounds down.
			int blockIndex = offset % BLOCK_SIZE; //this tells you where to start in ^^ that block.
			int firstWrite = BLOCK_SIZE - (offset % BLOCK_SIZE); //how many bytes to write
			if(firstWrite == 0){
				firstWrite = size;
				if(firstWrite > BLOCK_SIZE){
					firstWrite = BLOCK_SIZE;
				}
			}
			int remainingBlock = BLOCK_SIZE - (((inode*)buffer)->size % BLOCK_SIZE);//size remaining in last block allocated.
			
			//if offset isn't pointing to end of file, return error.
			if(offset != ((inode*)buffer)->size){
				log_msg("Overwrite not implemented.\n");
				return 0; 
			}
			
			//if offset points to a block we don't have yet. (Like if file is 300 bytes asking for offset 315.
			if(offset > ((inode*)buffer)->size){
				return ESPIPE; 
			}
			
			//keep track of how much you've written with this
			int bytesWritten = 0;
			while(bytesWritten < size ){
				char tempbuffer[BLOCK_SIZE];
				memset(tempbuffer, 0, BLOCK_SIZE);
				
				//will only happen on first read.
				if(!(remainingBlock == 0 || remainingBlock == 512)){
					log_msg("remainingBlock = %d\n", remainingBlock);
					if(remainingBlock > size){
						remainingBlock = size;
					}
					block_read(((inode*)buffer)->blockNum[firstBlock], tempbuffer);
					memcpy(tempbuffer+blockIndex, buf, remainingBlock);
					bytesWritten = remainingBlock;
					block_write(((inode*)buffer)->blockNum[firstBlock], tempbuffer);
					log_msg("wrote: %s\n", tempbuffer);
					remainingBlock = 0;
				}
				else{				
					//get free data block to fill.
					int data = findFirstFreeData(); 
					count++;
			
					//nothing has been written yet
					if (bytesWritten == 0){
						if(size < firstWrite){
							firstWrite = size; 
						}
						memcpy(tempbuffer+blockIndex, buf, firstWrite);
						bytesWritten += firstWrite;
					}
					//amount left to write is less than full block
					else if ((size - bytesWritten) < BLOCK_SIZE){
						memcpy(tempbuffer, buf + bytesWritten, size - bytesWritten);
						bytesWritten = size;
					}	
					//just write the whole block.
					else{
						memcpy(tempbuffer, buf+bytesWritten, BLOCK_SIZE);
						bytesWritten += BLOCK_SIZE;					
					}
					blocks[data] = '1';		

					//find next available index in blockNum;	
					int index = 0;	
					while (((inode*)buffer)->blockNum[index] != 0){
						index++;
					}					
					((inode*)buffer)->blockNum[index] = data;
					
					//	store data block in inode->blockArray				
					block_write(data, tempbuffer);
					log_msg("wrote: %s\n", tempbuffer);
					fprintf(stderr, "this is what was written: %s\n", tempbuffer);
				}
			}	
							
			//change size of inode to be += size and numBlocks + count (how many were filled during write)
			((inode*)buffer)->size += size;
			((inode*)buffer)->numBlocks = count;
		
			//update modify time.
			((inode*)buffer)->modifytime = time(NULL);		
		
			//write the inode back in to the file	
			block_write(block, buffer);
			memset(buffer, 0, sizeof(buffer));
			retstat = size;
		}
	}		
	log_msg("leaving write..\n");
    return retstat;
}

/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode){
	fprintf(stderr,"mkdir");
	int retstat = 0;

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);
  
	//check to see if path already exists and return error
	if(findInode(path)!= -1){
		return EEXIST;	
	}


	//create new zeroed out char buffer: 
	char buffer[BLOCK_SIZE];
	memset(buffer, 0, BLOCK_SIZE);    
	
	//check array of inodes for first one that is 0.  Save block number.
 	int foundInd = findFirstFreeInode();
	if (foundInd == -1){
		fprintf(stderr, "No free nodes found\n");
		log_msg("no free inodes found\n");
		return ENOSPC;
	}

	//create new inode and cast buffer into it.
	inode* useNode = (inode*)buffer;

	//fill in inode
	useNode->size = 0; 
	useNode->numBlocks = 0;//this is number of files in directory
	useNode->links = 2; //at least . and .. to start.  Increases when directories are made inside.
	useNode->indirectionBlock = -1;//this will never need to be updated.
	useNode->type = DIR_NODE;
	useNode->filemode = S_IFDIR | mode;
	useNode->createtime = time(NULL);		
	useNode->modifytime = useNode->createtime;
	useNode->accesstime = useNode->createtime;
	useNode->userId = getuid(); 
	useNode->groupId = getgid(); 

	//write path into node
	strncpy(useNode->path, path, strlen(path));
	useNode->path[strlen(path)] = '\0';

	//write inode to file and update array
	block_write(foundInd, (void*)buffer);
	blocks[foundInd] = '1';

	//find this guy's parent.
	char parentDir[PATHSIZE];
	memcpy(parentDir, path, PATHSIZE);
	getParentDir(&(parentDir[0]));

	//read parent block in and update modifytime, numblocks and links	
	int parentBlock = findInode(parentDir);
	char buffer3[BLOCK_SIZE];
	block_read(parentBlock, buffer3);
	((inode*)buffer3)->numBlocks++;
	((inode*)buffer3)->links++;//again this is because of inode.
	((inode*)buffer3)->modifytime = time(NULL);
	
	//now write it back since we updated it
	block_write(parentBlock, buffer3);  
  
	return retstat;
}

/** Remove a directory */
int sfs_rmdir(const char *path){
	fprintf(stderr,"rmdir\n");
	int retstat = 0;

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("sfs_rmdir(path=\"%s\")\n",	path);
  
  	//find block number of inode and/or make sure inode exists  
	int block = findInode(path);
	if(block == -1){
		retstat = -ENOENT;
	}
	else{
		//read in inode
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, buffer);
		//if this inode is for a file, return error
		if(((inode*)buffer)->type == FILE_NODE){
			log_msg("Not a directory\n");
			fprintf(stderr, "Not a directory\n");
			retstat = ENOTDIR;
		}
		//if this directory is not empty, return error
		else if(((inode*)buffer)->numBlocks != 0){
			log_msg("Directory not empty\n");
			fprintf(stderr, "Directory not empty\n");
			retstat = ENOTEMPTY;
		}
		else{
			//clear the array
			blocks[block] = '0';

			//get parent inode		
			char parentDir[PATHSIZE];
			memcpy(parentDir, path, PATHSIZE);
			getParentDir(&(parentDir[0]));
			
			//read it in and reduce numBlocks and links.
			int parentBlock = findInode(parentDir);
			char buffer3[BLOCK_SIZE];
			block_read(parentBlock, buffer3);
			((inode*)buffer3)->numBlocks--;
			((inode*)buffer3)->links--;			
			((inode*)buffer3)->modifytime = time(NULL);

			//write parent back to file
			block_write(parentBlock, buffer3);	
			
			//now to be safe, write empty buffer back to file for this inode to prevent data leaks.
			char buffer2[BLOCK_SIZE];
			memset(buffer2, 0, BLOCK_SIZE);
			block_write(block, buffer2);
			
			log_msg("Directory deleted\n");
		}
  	}
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

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_opendir\n");//(path=\"%s\", fi=0x%08x)\n",path, fi);

	//check for existence.
	int block = findInode(path);
	if(block == -1){
		retstat = -ENOENT;
	}
	else{
		//if exists, then read in the inode
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, buffer);
	
		//if not a directory, return error
		if(((inode*)buffer)->type == FILE_NODE){
			retstat = ENOTDIR;
		}
	}
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
	
	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	log_msg("\nsfs_readdir: %s\n", path);
	int retstat = 0;

	//pull in inode	
	char buffer [BLOCK_SIZE];
	memset(buffer, 0, BLOCK_SIZE);
	int block = findInode(path);
	//make sure exists
	if(block == -1){
		fprintf(stderr,"Couldn't find directory\n");
		log_msg("Couldn't find file\n");
		return -ENOENT; 
	}
	else{
		//fill . and .. b/c directory
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0); 
		fprintf(stderr,"block: %d\n", block);
		//read in the inode
		block_read(block, buffer);
		if(((inode*)buffer)->type == DIR_NODE){
			char buffer2[BLOCK_SIZE];
			memset(buffer, 0, BLOCK_SIZE);

			int i = 1;
			while(i < INODEBLOCKS){
				if(blocks[i] == '1'){
					//read in every inode one at a time
					block_read(i,(void*)buffer2);

					//get each inode's parent and check to see if is the directory being read.
					char parentDir[PATHSIZE];
					memcpy(parentDir, ((inode*)buffer2)->path, PATHSIZE);
					getParentDir(&(parentDir[0]));				
					//if it is a match, fill a stat
					if(strcmp(parentDir, path) == 0){
						//fill in stat
						struct stat stat;
						stat.st_dev = 0;
						stat.st_ino = i;
						stat.st_rdev = 0;
						stat.st_mode = ((inode*)buffer2)->filemode;
						stat.st_nlink = ((inode*)buffer2)->links;
						stat.st_uid = ((inode*)buffer2)->userId;
						stat.st_gid = ((inode*)buffer2)->groupId;
						stat.st_size = ((inode*)buffer2)->size;
						stat.st_atime = ((inode*)buffer2)->accesstime;
						stat.st_mtime = ((inode*)buffer2)->modifytime;
						stat.st_ctime = ((inode*)buffer2)->createtime;
						stat.st_blocks = ((inode*)buffer2)->numBlocks;

						char* fullpath = ((inode*)buffer2)->path;
						getShortPath(fullpath);
						//put this file into the directory
						filler(buf, fullpath, &stat, 0);
						log_msg("put this file into directory: %s\n", fullpath);
					}
				}
				i++;
			}
		}
		else{//not a directory, return error
			fprintf(stderr, "Not a directory\n");
			log_msg("Not a directory\n");
			retstat = ENOTDIR;
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

	//make sure filename isn't longer than max size (causes segfault)
	if(strlen(path) > PATHSIZE){
		return ENAMETOOLONG; 
	}

	//make sure file exists	
	int block = findInode(path);
	if(block == -1){
		retstat = -ENOENT;
	}
	else{
		//make sure it's a directory
		char buffer[BLOCK_SIZE];
		memset(buffer, 0, BLOCK_SIZE);
		block_read(block, buffer);
		if(((inode*)buffer)->type == FILE_NODE){
			retstat = ENOTDIR;
		}
		else{
			//if it is, update access time
			((inode*)buffer)->accesstime = time(NULL);
			block_write(block, buffer);
		}
	}
	
		
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
