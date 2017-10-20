// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

#define USE_MY_PTHREAD 1
#define MAX_THREADS 128 //max threads
//#define MAX_MUTEX 128 //max mutexes
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


#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#endif


//enum for states
typedef enum _states{
	ACTIVE, WAITING, YIELDED, PREEMPTED, DONE
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
 		/* add something here */\
 		
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
