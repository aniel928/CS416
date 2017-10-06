// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

//might need these?
//#include <string.h>
//#include <ucontext.h>
//#include <signal.h>
//#include <sys/time.h>


//can i replace all parameters with just one x on each side?
#define pthread_create( x ) my_pthread_create( x )
#define pthread_yield( x ) my_pthread_yield( x )
#define pthread_exit( x ) my_pthread_exit( x )
#define pthread_join( x ) my_pthread_join( x )
#define pthread_mutex_init( x ) my_pthread_mutex_init( x )
#define pthread_mutex_lock( x ) my_pthread_mutex_lock( x )
#define pthread_mutex_unlock( x ) my_pthread_mutex_unlock( x )
#define pthread_mutex_destroy( x ) my_pthread_mutex_destroy( x )



//unsigned int is thread identifier
typedef uint my_pthread_t;

typedef struct threadControlBlock {
	/* add something here */
	//Normally includes:
	//threadIdentifier - unique id assigned to every new thread
	//stackPointer - points to thread's stack in the process
	//program counter
	//state of the thread (runnin, ready, waiting, start, done)
	//thread's register values
	//pointer to PCB of process thread lives on.
} tcb; 

/* mutex struct definition */
typedef struct _my_pthread_mutex_t {
	/* add something here */
	//test-and-set instruction?
	
} my_pthread_mutex_t;

/* define your data structures here: */

// Feel free to add your own auxiliary data structures

//enum for states
typedef enum _states{
	ACTIVE, WAITING, DONE
} states;


//doubly linked list for running, waiting, and holding (mpq)
typedef struct _threadQueueNode{
	int priority;
	void* prev;
	void* next;
	//fill with more stuff
} threadQueueNode;

//multi-priority queue
typedef struct multiPriorityQueue{
	//these will be linked lists.
	threadQueueNode* level0ptr;//highest level priority 
	threadQueueNode* level1ptr;
	threadQueueNode* level2ptr;
	threadQueueNode* level3ptr;//lowest level priority
} mpq;




/* Function Declarations: */

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
