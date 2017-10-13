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
#include <math.h>
#include "my_pthread_t.h"

/**************** Global Variables ****************/

int timerCounter = 0;	//Used to stop infinite while loop for timer
bool schedInit = FALSE;
tcb* managerThread = NULL; //this is the main thread
ucontext_t main_uctx;//main context

//running threads queue - made up out of MPQ
queueNode* headRunning = NULL;
queueNode* tailRunning = NULL;
queueNode* currentRunning = NULL; //the one currently running
queueNode* nextRunning = NULL; //the one that's next to run

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


//Initialize head, tail, and counter of each queue assuming only 4 levels.


queueNode* level0Qhead = NULL;
queueNode* level0Qtail = NULL;

queueNode* level1Qhead = NULL;
queueNode* level1Qtail = NULL;

queueNode* level2Qhead = NULL;
queueNode* level2Qtail = NULL;

queueNode* level3Qhead = NULL;
queueNode* level3Qtail = NULL;

//store pointers to head & tail per level.
queueNode** mpqHeads[PRIORITY_LEVELS] = {&level0Qhead, &level1Qhead, &level2Qhead, &level3Qhead};
queueNode** mpqTails[PRIORITY_LEVELS] = {&level0Qtail, &level1Qtail, &level2Qtail, &level3Qtail};

int levelCtrs[4] = {0};


/**************** Additional Methods ****************/

void testThreads(){
	printf("Printing\n");
	my_pthread_yield();
	return;
}

void testThreadsWithExit(){
	printf("Printing\n");
	my_pthread_exit(NULL);
	return;
}

//initializes and runs scheduler until no threads exist
void schedulerInit(){	

	schedInit = TRUE;
	//until no threads are left keep maintaining, then running, maintaining, then running.
	while(schedInit){
		maintenanceCycle();
		if(schedInit){
			runThreads();
		}
	} 
	

}

//do some maintenance between running cycles
void maintenanceCycle(){
	//To build new queue: level 0 gets 2^0 = 1 quantum, level 1 gets 2^1 = 2 quantum, level 2 gets 2^2 quantum, level 3 2^3 quantum, level 4 gets FIFO?
	//how many of each before maintenance cycle?
	
	//need a "maintenance cycle" method that changes around priorities:
	//Suggestions:
	//	-if in 2 for more than 5 full maintenance cycle quantum, move to 1
	//	-if in 1 for more than 5 full maintenance cycle quantum, move to 0
	//	-figure out how to avoid priority inversion.
		//if mutex waiting, do not promote?
	//	-need to figure out how to bring back in from waiting queue - what should happen with these?  
	//	Back to previous level?  Start at high? start at low?
	
	//once there are no more threads left change scheduler to 
	if(levelCtrs[0] + levelCtrs[1] + levelCtrs[2] + levelCtrs[3] == 0){
			schedInit = FALSE;	
	}
	else{
		//move up in priority
		//mostly pseudocode:
		//i = 0
		//while i < PRIORITY_LEVELS:
		//	current = MPQheads[i]
		//	while current:
		//		if current->ctr > CYCLES:
		//		threads[current->tid]->priority += 1
		//		current = current->next;
		
		
		//pull a list of threads from MPQ to run.
		createRunning();
	}
}

//create list of threads to run between maintenance cycles
void createRunning(){
	//TODO: all pseudocode.
	//if more than 8 in level 0:
		//move first 8 into running
		//make 9th new head of 0.
	//else:
		//move entire queue into running
		//make head of 0 = NULL
	//if more than 5 in level 1:
		//move first 5 into running
		//make 6th new head of 1.
	//else:
		//move entire queue into running
		//make head of 1 = NULL
	//if more than 3 in level 2:
		//move first 3 into running
		//make 4th new head of 2.
	//else:
		//move entire queue into running
		//make head of 2 = NULL
	//if more than 1 in level 3:
		//move first into running
		//make 2nd new head of 3.
	//else:
		//move entire queue into running
		//make head of 3 = NULL
	
	//don't bother filling with more nodes from other levels, this round will just be shorter.
	
	
	//fix this to be a better running list, I'm just putting all threads in level0 in here for now
	printf("Move level0Qhead over to headRunning\n");
	headRunning = level0Qhead;
	level0Qhead = NULL;
	levelCtrs[0] = 0;
	return;
}

//Run them between cycles
void runThreads(){

	printf("Assign head running to current running\n");
	nextRunning = headRunning;
	while(nextRunning){
		printf("made it to while loop\n");
		//call to pthread_yield to swap contexts.
		//timer(threads[nextRunning->tid]->priority);
		my_pthread_yield();	
		//logic needed for yield.  If NULL, then we know there's not currently a thread running, it's the "manager thread"
		currentRunning = NULL;
		//if my_pthread_exit is called, then this will be null, check that first.
		if(threads[nextRunning->tid] == NULL){
			printf("Done\n");//debugging statement
		}//if not null, check if status is preempted so we can lower priority
		else if(threads[nextRunning->tid]->threadState == PREEMPTED){
			printf("Preempted\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			levelCtrs[threads[(nextRunning->tid)]->priority] -= 1;//does not belong here.. why is it working?
		}
		//if not premempted check for yield, will not lower priority
		else if(threads[nextRunning->tid]->threadState == YIELDED){
			printf("Yielded\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			levelCtrs[threads[(nextRunning->tid)]->priority] -= 1;//does not belong here.. why is it working?
		}
		//otherwise it finished on it's own, but never called exit.
		else{
			printf("Done\n");//debugging statement
			threads[nextRunning->tid]->threadState = DONE;
			levelCtrs[threads[(nextRunning->tid)]->priority] -= 1;//does not belong here.. why is it working?
			//will have to remove it manually.  Either put that logic here, or in maintenance.  I vote for here. (faster)
		}
		//move to next node.
		nextRunning = (queueNode*)nextRunning->next;
		printf("restart loop\n"); //debugging statement.
	}
}

/*signal handler for timer*/
void time_handle(int signum){
	printf("time_handler\n");
	/*//change status to PREEMPTED
	threads[currentRunning->tid]->threadState = PREEMPTED;
	//change priority +1 on tcb node(unless bottom level)
	if(threads[currentRunning->tid]->priority != (PRIORITY_LEVELS -1)){ //if we do more than 4 levels, change this number!
		threads[currentRunning->tid]->priority =+ 1;}*/
	//put back on tail of that new priority queue. (example in my_pthread_create())
	//careful to keep it here for when it swaps back in to main.
	//swapcontext back to main
	//my_pthread_yield();
	
	int count = 0;
	printf("finished %d %d\n", signum,timerCounter);
	timerCounter++;
}

//add parameter for priority then take 2^priority and * QUANTUM
void timer(){//int priority){
	struct sigaction sigact;
	struct itimerval timer;

	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = &time_handle;
	sigaction (SIGVTALRM, &sigact, NULL);
	
	/*next value*/		//current timer interval? why next value?
	timer.it_interval.tv_sec = 0;		//0 seconds
	timer.it_interval.tv_usec = QUANTUM;// * (1 << priority) ;	//25 milliseconds / 1 quantum

	/*current value*/	//current timer value?
	timer.it_value.tv_sec = 0;		//0 seconds
	timer.it_value.tv_usec = QUANTUM;// * (1 << priority) ;	//25 milliseconds / 1 quantum
	
	
	setitimer (ITIMER_VIRTUAL, &timer, NULL);

	while (timerCounter<40){
	
	}
}

//add to MPQ (new thread, or back in from waiting/running), must pass in level queues.
void addMPQ(tcb* thread, queueNode** head, queueNode** tail){
	queueNode* newNode = (queueNode*)malloc(sizeof(queueNode));
	
	//prep new node 
	newNode->tid = thread->tid;
	newNode->next = NULL;
	newNode->ctr = 0; 
	
	//figure out where to add it.	
	if(*head == NULL){//list is empty
		printf("added to head of MPQ\n");
		*head = newNode;
		*tail = newNode;
	}
	else if((*head)->tid == (*tail)->tid){//list only has one element
		printf("added as 2nd element of MPQ\n");
		(*head)->next = newNode;
		*tail = newNode;
	}
	else{//list has more than 2 or more elements.
		printf("added to end of MPQ\n");
		(*tail)->next = newNode;
		*tail = newNode;
	}	
	return;
}

/**************** Given Methods to Override ****************/

	/**************** Thread Methods ****************/
/* create a new thread */  /* DONE */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	/* Creates a pthread that executes function. Attributes are ignored, arg is not. */

	//initialize to some fake value
	my_pthread_t ID = -1;

	//fill up array before moving to linked list of readyIndexes
	if (useThreadId == FALSE){
		printf("use array\n");
		ID = threadCtr;
		threadCtr++;
		//thread is full, switch bool to TRUE.
		if(threadCtr == MAX_THREADS){
			useThreadId = TRUE;	
			headThread = (nextId*)malloc(sizeof(nextId));
			headThread->next = NULL;
			headThread->readyIndex = -1;
		}
	}
	//array is filled, move over to linked list of readyIndexes
	else{
		printf("don't use array!\n");
		if(headThread->readyIndex != -1){//there are indexes available
			ID = headThread->readyIndex;
			nextId* tempThread = headThread;
			if(headThread->next == NULL){ //this way we always have a dummy node
				headThread->readyIndex = -1;
			}
			else{//there are no indexes available
				headThread = (nextId*)headThread->next;
				free(tempThread); //frees the thread that was previously head.
			}
		}
		else{
			printf("No threads available.\n");
			return -1; //couldn't create it, array is full.
		}
	}
	//create a new tcb
	tcb* newNode = (tcb*)malloc(sizeof(tcb));
	//assign thread ID
	newNode->tid = ID;
	if(getcontext(&(newNode->context)) == -1){
		printf("Throw error\n");
		return -1;			
	}

	//do context stuff
	ucontext_t uc = newNode->context;
	uc.uc_stack.ss_sp = (char*)malloc(STACK_SIZE);
	uc.uc_stack.ss_size = STACK_SIZE;
	uc.uc_link = &main_uctx;//should be manager thread (scheduler? maintenance?)

	//arg is the (one) argument that gets passed into function ^^
	makecontext(&uc, (void(*)(void))*function,1,arg); 
	newNode->context = uc; 

	//assign other tcb stuff
	newNode->threadState = ACTIVE;
	newNode->priority = 0;
	newNode->retval = NULL;
	newNode->waitingThreads = NULL;
	threads[ID] = newNode;
	
	//"thread" is a pointer to a "buffer", store ID in that "buffer"
	*thread = ID;		
	
	//add new thread to mpq level 0
	addMPQ(threads[ID], &level0Qhead, &level0Qtail);
	levelCtrs[0] += 1;
	
	//Initialize the scheduler if it's not already initialized
	if(!schedInit){
		schedulerInit();
	}
	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	/* Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out 
	and another can be scheduled if one is waiting. */
	printf("In yield\n");//debugging statement
	
	//if currentRunning is null, then this is being called by the main program on purpose
	if(currentRunning == NULL){
		currentRunning = nextRunning;
		swapcontext(&(main_uctx),&((threads[currentRunning->tid])->context));
	}
	//otherwise the currently running thrad rquested to yield.
	else{
		threads[currentRunning->tid]->threadState = YIELDED;
		swapcontext(&((threads[currentRunning->tid])->context),&(main_uctx));
	}

	//swapcontext(&(where to save),&(new one to run))
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/* Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't 
	NULL, any return value from the thread will be saved. */
	printf("In my pthread exit\n");

	//before exiting, check to see if anyone else joined.
	/*Unable to test this part until we figure out how to ref library in other file. */
	queueNode* tempNode = NULL;
	if(threads[currentRunning->tid]->waitingThreads != NULL){//there are waiting threads
		queueNode* currentNode = threads[currentRunning->tid]->waitingThreads;
		while(currentNode){
			currentNode->retval = value_ptr;
			
			//TODO: put back on mpq!  I think this code will work, but cannot test it yet, and might really blow up.
			//mpqTails[threads[currentNode->tid]->priority]->next = currentNode;
			//mpqTails[threads[currentNode->tid]->priority] = currentNode;
			
			tempNode = currentNode;
			currentNode = currentNode->next;
		}
	}
	else{//no waiting threads - no one joined.
		printf("No threads waiting.\n");		
	}
		
	//Pass ID into tailThread, set curr tail = to this number.	
	if(headThread->readyIndex == -1){//none in queue yet
		printf("First thread ID\n");
		headThread->readyIndex = currentRunning->tid;	
		headThread->next = NULL;
		tailThread = headThread;
	}
	else if(headThread == tailThread){//only one in queue
		printf("Second thread ID\n");
		nextId* tmpThread = (nextId*)malloc(sizeof(nextId));
		tmpThread->readyIndex = currentRunning->tid;
		tmpThread->next = NULL;
		headThread->next = tmpThread;
		tailThread = tmpThread;
	}
	else{//two or more in queue
		printf("Lots in\n");
		nextId* tmpThread = (nextId*)malloc(sizeof(nextId));
		tmpThread->readyIndex = currentRunning->tid;
		tmpThread->next = NULL;
		tailThread->next = tmpThread;
		tailThread = tmpThread;
	}
	free((threads[currentRunning->tid]));
	threads[currentRunning->tid] = NULL;
	
	return;
	
};

/* wait for thread termination */ 
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	/* Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one 
	it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.*/

	//check for valid thread.
	if(threads[thread] = NULL){
		printf("Thread does not exist, cannot join.\n");
		return -1;
	}
	//put thread onto waiting queue in tcb for thread passed in.
	queueNode* newNode = (queueNode*)malloc(sizeof(queueNode));
	newNode->tid = thread;
	newNode->next = NULL;
	newNode->retval = value_ptr;
	
	if(threads[thread]->waitingThreads = NULL){ //if i'm the first
		threads[thread]->waitingThreads = newNode;
	}
	else{ //if others are already waiting.
		queueNode* currentNode = threads[thread]->waitingThreads;
		while(currentNode->next){
			currentNode = currentNode->next;
		}
		currentNode->next = newNode;
	}
	//**value_ptr is a buffer waiting for ret val. (unless not null)

	return 0;
};

	/**************** Mutex Methods ****************/
/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	/* Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored. */
	
	//create new my_pthread_mutex_t struct.
	
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	/* Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked. */
	//if mutex already locked, add to wait queue
	
	//right before returning yield - is this correct (from Sakai)
	my_pthread_yield();
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	/* Unlocks a given mutex. */
	//check to see if anyone is waiting, if so, do not unlock, just "pass" the mutex on.  
	//Only unlock when no one else is waiting.
	
	
	//right before returning yield - is this correct (from Sakai)
	my_pthread_yield();
	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	/* Destroys a given mutex. Mutex should be unlocked before doing so. */
	//check to make sure mutex is not locked first.
	return 0;
};



//need main to keep errors from occurring - use for trivial testing.
int main(int argc, char** argv){


	/* TESTING */

	printf("Calling function normally:\n");
	timer();//Still for testing purpose, obviously move to wherever needed and remove this whenever
	printf("Now trying to use a thread\n");
	timerCounter=0;
	my_pthread_t mythread1, mythread2, mythread3, mythread4, mythread5, mythread6;
	printf("%d\n",threadCtr);
	my_pthread_create(&mythread1, NULL, (void*)&testThreads, NULL);
	printf("%d\n",threadCtr);
	my_pthread_create(&mythread2, NULL, (void*)&testThreads, NULL);
	printf("%d\n",threadCtr);
	my_pthread_create(&mythread3, NULL, (void*)&testThreads, NULL);
	printf("%d\n",threadCtr);
	my_pthread_create(&mythread4, NULL, (void*)&testThreads, NULL);
	printf("%d\n",threadCtr);
	my_pthread_create(&mythread5, NULL, (void*)&testThreadsWithExit, NULL);
	printf("%d\n",threadCtr);
	my_pthread_create(&mythread6, NULL, (void*)&testThreads, NULL);
	printf("Should NOT be 6: %d\n",threadCtr);

	queueNode* tempQ = level0Qhead;
	int count = 1;
	while(tempQ != NULL){
		printf("%d threads in queue\n", count);
		count++;
		tempQ = tempQ->next;
	}
}
