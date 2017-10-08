//Henry's comment
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
//To build new queue: level 0 gets 1 quantum, level 1 gets 3 quantum, level 2 gets 6 quantum, level 3 gets FIFO?
//how many of each before maintenance cycle?

threadQueueNode headRunQueue;
threadQueueNode tailRunQueue;

//initialize array of thread pointers to null.
tcb* threads[MAX_THREADS] = {NULL};

//initialize array of mutex pointers to null.
my_pthread_mutex_t* mutexes[MAX_MUTEX] = {NULL};

//prevent linear search by having a queue of ready numbers.
nextId* headThread = NULL;
nextId* tailThread = NULL;
nextId* headMutex = NULL;
nextId* tailMutex = NULL;

//don't use until array is filled to begin with
int threadCtr = 0;
bool useThreadId = FALSE;
int mutexCtr = 0;
bool useMutexId = FALSE;

//Correction: We DO need waiting queues, but they're going to be off of the threads/mutexes, not on their own.

//need a "maintenance cycle" method that changes around priorities:
//Suggestions:
//	-if in 2 for more than 5 full maintenance cycle quantum, move to 1
//	-if in 1 for more than 5 full maintenance cycle quantum, move to 0
//	-figure out how to avoid priority inversion.
	//if mutex waiting, do not promote?
//	-need to figure out how to bring back in from waiting queue - what should happen with these?  
//		Back to previous level?  Start at high? start at low?


//Do this as first step...
//Signal Interrupts and Signal Handler:
//	By setting setitimer, it will send SIGBVTALRM/SIGALRM, which I should be catching with a signal handler?
//		then do either context switch or maintenance cycle?
//


/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	/* Creates a pthread that executes function. Attributes are ignored, arg is not. */
	//DESIGN PLAN:
	//if (useThreadId == FALSE):
	//	ID = threadCtr
	//	threadCtr++;
	//	if(threadCtr == 50):
	//		useThreadId = TRUE;		
	//else:
	//	ID = pop ID off of head
	//	set head to head->next
	//
	//threads[ID] = new tcb node;
		//create a new tcb
		//"thread" is a pointer to a "buffer", store ID in that "buffer"
			//basically call thread* = threadID (index)
		//ignore attributes
		//get and make contexts (this is where function argument comes in)
			//malloc context.uc_stack->ss_sp
			//set context.uc_stack.ss_size = STACK_SIZE
		//arg is the (one) argument that gets passed into function ^^

	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	/* Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out 
	and another can be scheduled if one is waiting. */
	
	//call this function during context switch in running queue
	//is this where we call swap_context?
	
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/* Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't 
	NULL, any return value from the thread will be saved. */
	
	//before exiting, check to see if anyone else joined.
	//If so, pass return value on to each and put them back in ready queue.
	//Pass ID into tailThread, set curr tail = to this number.
	
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	/* Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one 
	it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.*/

	// put thread onto waiting queue in tcb for thread passed in.
	//**value_ptr is a buffer waiting for ret val. (unless not null)

	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	/* Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored. */
	
	//create new my_pthread_mutex_t struct.
	
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

//is this just for testing purposes?  It's global if it's declared here.
int i = 0;

/*signal handler for timer*/
void time_handle (int signum){
	int count = 0;
	printf("finished %d\n", signum);
	i++;
}

int main(int argc, char** argv){
	struct sigaction sigact;
	struct itimerval timer;

	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = &time_handle;
	sigaction (SIGVTALRM, &sigact, NULL);
	
	/*next value*/
	timer.it_interval.tv_sec = 0;		//0 seconds
	timer.it_interval.tv_usec = 25000;	//25 milliseconds / 1 quantum

	/*current value*/	
	timer.it_value.tv_sec = 0;		//0 seconds
	timer.it_value.tv_usec = 25000; 	//25 milliseconds / 1 quantum
	
	
	setitimer (ITIMER_VIRTUAL, &timer, NULL);

	while (i<40){
	
	}

}
