#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

#define USE_MY_PTHREAD 1
#define MAX_THREADS 32 //max threads
#define STACK_SIZE 16384 //size of stack in bytes - make smaller for part2
#define QUANTUM 25000 //predefined in project spec as 25ms - converted to microseconds
#define CYCLES 5 //how many full maintenance cycles before moving up one level of queue
#define PRIORITY_LEVELS 4//how many priority levels


/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_yield my_pthread_yield
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#endif

//MALLOC DECLARATIONS:

//enum for states

typedef enum _states{
	ACTIVE, WAITING, YIELDED, PREEMPTED, DONE
} states;

typedef enum _bool{
	FALSE, TRUE
}bool;

/*typedef struct _metaData{
	int used; //0 if free, 1 if used
	int bytes; //how many bytes were requested.
	struct _metaData* ptr; //point to next - wanted to avoid, but having trouble traversing list without.
}metaData;
*/
typedef struct _metaData{
	int bytes;
	int used; //0(false) or 1(true)
} metaData;


typedef struct _pageTableEntry{
	int pageIndex;
	int maxSize;
	int offset; //offset in swap file; -1 if not swapped out.
	struct _pageTableEntry* next;
} PTE;

typedef struct _memStruct{
	//int sizeLeft;	//initally set = to page size	
	int currUsed;
	int pageCount;
	bool inUse;
	struct _memStruct* next;
}memStruct;

//typedef struct _swapData{
//	int 
//} swapData;

//as we kill off threads/mutexes, push the newly available number (0-49) onto the bottom, 
//pull one off the top when we need a new thread.
typedef struct _nextId{
	int readyIndex;
	void* next;
} nextId;

//unsigned int is thread identifier
typedef uint my_pthread_t;

typedef struct _queueNode{
	my_pthread_t tid;
	void* next;
	int ctr; //how long has it been on this level?
	void* retval;
} queueNode;

typedef struct threadControlBlock{
	/* add something here */
	
	//threadIdentifier - unique id assigned to every new thread (index in array)
	my_pthread_t tid; //should this be a my_pthread_t type?
	//stackPointer - points to thread's stack in the process
	ucontext_t context;
	//state of the thread (active, waiting, preempted, yielded, done)
	states threadState;
	//current priority level
	uint priority;
	//retval of thread (so people waiting on me get return value)
	char** retval;
	//list of people waiting on me (do it again for mutexes)
	void* waitingThreads;
	//for priority inversion
	bool mutexWaiting;
	//page table for each thread
	PTE* pageTable;
} tcb; 

typedef struct _waitQueueNode{

	tcb *thread;
	struct _waitQueueNode *next;

} waitQueueNode;

//used for the wait queue on mutexes
typedef struct _basicQueue{

    waitQueueNode *head;
    waitQueueNode *tail;
    int queueSize;

} basicQueue;

/* mutex struct definition */
typedef struct _my_pthread_mutex_t {
 		/* add something here */
 		
	tcb *owner; //thread owner of this mutex
	int lockState; //1 is locked 0 is unlocked
	basicQueue *waitQueue; //keeps track of the threads waiting on this mutex

} my_pthread_mutex_t;

int schedulerInit();
void scheduler();
void maintenanceCycle();
void createRunning();
void runThreads();
void time_handle(int signum);
void timer(int priority);
void addMPQ(tcb* thread, queueNode** head, queueNode** tail);
void exit_thread(queueNode* node, void* value_ptr);
void free_things();

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

#endif


//malloc stuff
#ifndef _MYALLOCATE_H
#define _MYALLOCATE_H
#define MEMORYSIZE (8*1024*1024) //8MB
#define THREADREQ 1
#define LIBREQ 0
#define malloc(x) myallocate(x,__FILE__,__LINE__, THREADREQ)
#define free(x) mydeallocate(x,__FILE__,__LINE__, THREADREQ)
#define PAGESIZE sysconf( _SC_PAGE_SIZE)
#define OSSIZE (1024*1024) //1MB
#define SHAREDSIZE (PAGESIZE * 4)
#define USERSIZE (MEMORYSIZE - OSSIZE - SHAREDSIZE) //8MB - 1MB - 4 shared pages
#define NUMOFPAGES (USERSIZE / PAGESIZE)
#define SWAPFILESIZE 16*1024*1024
#endif

//helper methods
int findConsecutivePages(int numPages);
int findSwapIndex();
int findEvictIndex(int numPages);
int evictPage(int page);
int restorePage(int page, int offset);
int evictPageIntoBuffer(int page, int offset);
void segment_fault_handler(int signum, siginfo_t *si, void* unused);
void mallocInit();
void * myallocate(int size, char * file, int line, int threadId);
void mydeallocate(void* ptr, char* file, int line, int threadId);
void* shalloc(int size);