// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include "my_pthread_t.h"

//running threads queue - made up out of MPQ
threadQueueNode headRunQueue;
threadQueueNode tailRunQueue;


//Correction: We DO need waiting queues, but they're going to be off of the threads/mutexes, not on their own.


//need a "maintenance cycle" method that changes around priorities.


//Suggestions:
//	-if in 2 for more than 5 full maintenance cycle quantum, move to 1
//	-if in 1 for more than 5 full maintenance cycle quantum, move to 0
//	-figure out how to avoid priority inversion.
	//if mutex waiting, do not promote?
//	-need to figure out how to bring back in from waiting queue - what should happen with these?  
//		Back to previous level?  Start at high? start at low?
//	To build new queue: level 0 gets 1 quantum, level 1 gets 3 quantum, level 2 gets 6 quantum, level 3 gets FIFO?


//Do this as first step...
//Signal Interrupts and Signal Handler:
//	By setting setitimer, it will send SIGBVTALRM/SIGALRM, which I should be catching with a signal handler?
//		then do either context switch or maintenance cycle?
//


/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	/* Creates a pthread that executes function. Attributes are ignored, arg is not. */
	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	/* Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out 
	and another can be scheduled if one is waiting. */
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/* Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't 
	NULL, any return value from the thread will be saved. */
	//before exiting, check to see if anyone else joined.  If so, have both exit.	
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	/* Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one 
	it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.*/

	//do you join instead of exiting? (i'm sure you do, but just making sure it's coded right)
	//	***NO, you join and then when the other thread exits, this resumes.
	//	pass return value along to everyone that joined it.
	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	/* Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored. */
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	/* Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked. */
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	/* Unlocks a given mutex. */
	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	/* Destroys a given mutex. Mutex should be unlocked before doing so. */
	return 0;
};


int main(int argc, char** argv){
	//need "main" to get rid of error, not sure what we'll put here.
	//Testing crap?
}

