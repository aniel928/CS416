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



/* * * * * * * * * * * * * * *

	define structs/enums

 * * * * * * * * * * * * * * */
 
//enum for states (active, waiting, etc...) --may not be needed with different queues, but might still be nice to have.
//doubly linked list to use as queue
typedef struct _threadQueueNode{
	int priority;
	void* prev;
	void* next;
	//fill with more stuff
} threadQueueNode;


//typedef uint my_pthread_t; //why is this an unsigned int and not struct??


//need a struct, no?
typedef struct _my_pthread_t{
	//context
	//threadID
	//maybe state?
	//fill with stuff
} my_pthread_t;

typedef struct threadControlBlock {
	/* add something here */
} tcb; 

/* mutex struct definition */
typedef struct _my_pthread_mutex_t {
	/* add something here */
} my_pthread_mutex_t;

/* define your data structures here: */

// Feel free to add your own auxiliary data structures


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
