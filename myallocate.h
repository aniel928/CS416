#include <unistd.h>

#ifndef _MYALLOCATE_H
#define _MYALLOCATE_H
#define THREADREQ 0
#define malloc(x) myallocate(x,__FILE__,__LINE__,THREADREQ)
#define free(x) mydeallocate(x,__FILE__,__LINE__,THREADREQ)
#define MEMORYSIZE 8388608
#define PAGESIZE sysconf( _SC_PAGE_SIZE)
#endif

char * myallocate(int size, char * file, int line, int type);
void mydeallocate(char* ptr, char* file, int line, int type);
