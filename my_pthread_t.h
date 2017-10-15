// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE


#define MAX_THREADS 5 //max threads
#define MAX_MUTEX 50 //max mutexes
#define STACK_SIZE 16384 //size of stack in bytes
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


//can i replace all parameters with just one x on each side?
#define pthread_create( x ) my_pthread_create( x )
#define pthread_yield( x ) my_pthread_yield( x )
#define pthread_exit( x ) my_pthread_exit( x )
#define pthread_join( x ) my_pthread_join( x )
#define pthread_mutex_init( x ) my_pthread_mutex_init( x )
#define pthread_mutex_lock( x ) my_pthread_mutex_lock( x )
#define pthread_mutex_unlock( x ) my_pthread_mutex_unlock( x )
#define pthread_mutex_destroy( x ) my_pthread_mutex_destroy( x )

//level 0 gets 1 quantum, level 1 gets 3, 2 gets 6, 3 gets FIFO (-1 so we can catch)
int quantumPriority[PRIORITY_LEVELS] = {1,3,6,-1};

//enum for states
typedef enum _states{
	ACTIVE, WAITING, DONE
} states;

typedef enum _bool{
	FALSE, TRUE
} bool;

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
	my_pthread_t* next;
} queueNode;

typedef struct threadControlBlock{
	/* add something here */
	
	//threadIdentifier - unique id assigned to every new thread (index in array)
	my_pthread_t tid; //should this be a my_pthread_t type?
		//as it finishes, assign it to waiting queue
	//stackPointer - points to thread's stack in the process
	ucontext_t context;
	//state of the thread (running, ready, waiting, start, done)
	states threadState;
	//current priority level
	uint priority;
	//retval of thread (so people waiting on me get return value)
	void* retval;
	//list of people waiting on me (do it again for mutexes)
	void* waitingThreads;
	
	//program counter - probably not
	//thread's register values - probably not
	//pointer to PCB of process thread lives on. - probably not
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
	//what does mutex have mutual exclusion on
	//thread owner
	//locked yes or no
	//list of people waiting on me
		//don't unlock unless this pointer is null, otherwise hand off lock.

	my_pthread_t owner; //thread owner of this mutex
	int lockState; //1 is locked 0 is unlocked
	basicQueue *waitQueue; //keeps track of the threads waiting on this mutex

} my_pthread_mutex_t;

/* define your data structures here: */

// Feel free to add your own auxiliary data structures

//declare MPQ array (0 - n)
int mpq[PRIORITY_LEVELS] = {0};

//fill array 0 through n
// int i = 0;

// while(i < PRIORITY_LEVELS){
	// mpq[i] = i;
	// i++;
// }

//mpq node
typedef struct _MPQNode{
	my_pthread_t threadId;
	void* next;
	int ctr; //how long has it been on this level?
	//fill with more stuff
} MPQNode;

/* Function Declarations: */

void schedulerInit();
void maintenanceCycle();
void createRunning();
void runThreads();
void time_handle(int signum);
void timer();
void addMPQ(tcb* thread, MPQNode** head, MPQNode** tail);

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
