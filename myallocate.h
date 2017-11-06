#include <unistd.h>

#ifndef _MYALLOCATE_H
#define _MYALLOCATE_H
#define THREADREQ 0
#define malloc(x) myallocate(x,__FILE__,__LINE__,THREADREQ)
#define free(x) mydeallocate(x,__FILE__,__LINE__,THREADREQ)
#define MEMORYSIZE 8388608
#define PAGESIZE sysconf( _SC_PAGE_SIZE)
#define NUMOFPAGES (MEMORYSIZE / PAGESIZE)
#endif



typedef enum _bool{
	FALSE, TRUE
} bool;

typedef struct _memStruct{
	//int sizeLeft;	//initally set = to page size	
	int currUsed;
	int pageCount;
	bool inUse;
	struct _memStruct* next;
	struct _memStruct* prev;
}memStruct;




char * myallocate(int size, char * file, int line, int type);
void mydeallocate(char* ptr, char* file, int line, int type);
