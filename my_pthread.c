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
ucontext_t main_uctx;
ucontext_t sched_uctx; //scheduler context

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
int threadCtr = 1;//not 0 because 0 is saved for main thread.
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

//global variable for when threads don't exit.
queueNode* manualExit = NULL;

/**************** Additional Methods ****************/

void schedulerInit(){
	schedInit = TRUE;
	//create context to call scheduler (do not recursively call create)
	if(getcontext(&(sched_uctx)) == -1){
		printf("Couldn't get context.\n");
		return;			
	}
	ucontext_t uc = sched_uctx;
	uc.uc_stack.ss_sp = (char*)malloc(STACK_SIZE);
	uc.uc_stack.ss_size = STACK_SIZE;
	uc.uc_link = NULL;
	
	makecontext(&uc, scheduler,1,NULL); 
	sched_uctx = uc;
	swapcontext(&(threads[0]->context), &sched_uctx);
}

//initializes and runs scheduler until no threads exist
void scheduler(){	

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
	//once there are no more threads left change scheduler to 
	if(levelCtrs[0] + levelCtrs[1] + levelCtrs[2] + levelCtrs[3] == 0){
			schedInit = FALSE;	
	}
	else{
		int i = 0;
		while (i < PRIORITY_LEVELS){	//while i < PRIORITY_LEVELS: //right now this is 4
			currentRunning = *mpqHeads[i];	//	current = MPQheads[i]
			while (currentRunning != NULL){	//	while current:
				if (currentRunning->ctr > CYCLES){	//		if current->ctr > CYCLES:  
					threads[currentRunning->tid]->priority +=1;		//		threads[current->tid]->priority += 1
					}
				currentRunning = currentRunning->next;		//		current = current->next;		
			}
			i++;
		}
		createRunning();
	}
}

//create list of threads to run between maintenance cycles
void createRunning(){
	printf("In createRunning\nCurrent Counts: \nleve0: %d\n Level1: %d\n level2: %d\n level3: %d\n", levelCtrs[0], levelCtrs[1], levelCtrs[2], levelCtrs[3]);
	//Max nodes to be selected from priorty queue levels
	headRunning = (queueNode*)malloc(sizeof(queueNode));
	headRunning->tid = -1;
	headRunning->next = NULL;
	tailRunning = (queueNode*)malloc(sizeof(queueNode));
	tailRunning->tid = -1;
	tailRunning->next = NULL;
	queueNode*  tempRunning = (queueNode*)malloc(sizeof(queueNode));
	tempRunning->tid = -1;
	tempRunning->next = NULL;
	int L0Max = 7;
	int L1Max = 5;
	int L2Max = 3;
	int L3Max = 1;
	bool headSet = FALSE;
	int iterator = 0;

//********Adding level 0************/
	if (levelCtrs[0] > L0Max ){ //more than max in level 0
		printf("In if, %d > %d\n", levelCtrs[0], L0Max);			
			headRunning = level0Qhead;	//head is the beginning of level 0 head;
			tailRunning = NULL;	//tail is the same as head
			headRunning->next = tailRunning;
			headSet = TRUE;							//head is set
			level0Qhead= level0Qhead->next;		
		//move first 7 into running
			while(iterator < L0Max ){
				printf("in loop0 --%d\n", iterator);
				tailRunning = level0Qhead;
				level0Qhead = level0Qhead->next;
				iterator++;
			}
		tailRunning->next = NULL;//make 9th new head of 0.
		//make 8th node->next = null;
		levelCtrs[0] -= L0Max;
		}
	else if (levelCtrs[0] > 0){	// at least one, move entire level0 into running 
		printf("In else if, %d <= %d\n", levelCtrs[0], L0Max);
		headRunning = level0Qhead;	//headRunning is the beginning of level 0
		tailRunning = level0Qtail;	//tailRunning is the end of level0
		level0Qhead = NULL;					//clear head of level 0
		level0Qtail = NULL;					//clear tail of level 0
		headSet = TRUE;							//head is set
		levelCtrs[0] = 0;						//counter is empty
		printf("headRunning tid: %d \n tailRunning tid %d\n", headRunning->tid, tailRunning->tid);

	}
/**debugging**/
	tempRunning = headRunning;
	iterator = 0;
	printf("about to go in while loop 0\n");
	while (tempRunning != NULL){
		printf("%d : %d \n", iterator, tempRunning->tid);
		tempRunning = tempRunning->next;	
		iterator++;		
	}	
/**debugging*/
/********Adding level 1************/
	iterator = 0;
	if (levelCtrs[1] > L1Max){
		printf("in if\n");
		if (headSet == FALSE){			
			headRunning = level1Qhead;
			tailRunning = level1Qhead;
			headSet = TRUE;
			level1Qhead = level1Qhead->next;			
		}
		tailRunning = tailRunning->next;
		while (iterator < L1Max){
			tailRunning = level1Qhead;
			level1Qhead = level1Qhead->next;
			iterator++;
		}
		tailRunning->next = NULL;
		levelCtrs[1]-=L1Max;		
	}
	else if (levelCtrs[1]>0){
		if (headSet == FALSE){
			headRunning = level1Qhead;
			tailRunning = level1Qhead;
			level1Qhead = NULL;
			level1Qtail = NULL;
			headSet = TRUE;
			levelCtrs[1] = 0;	
			printf("finished else if ->if\n");			
		}
		else{
			tailRunning->next = level1Qhead;
			tailRunning = level1Qtail;
			level1Qhead = NULL;
			level1Qtail = NULL;
			levelCtrs[1]= 0;
			printf("finished else if ->else\n");
		}
		tailRunning->next = NULL;				
	}
		/**debugging**/
		tempRunning = headRunning;
		iterator = 0;
		printf("about to go in while loop 1\n");
		while (tempRunning != NULL){
			printf("%d : %d \n", iterator, tempRunning->tid);
			tempRunning = tempRunning->next;		
			iterator++;		
		}
		/**debugging**/		
		/********Adding level 2************/
		iterator = 0;
		if (levelCtrs[2] > L2Max){
			if (headSet == FALSE){
				headRunning = level2Qhead;
				tailRunning = level2Qhead;
				headSet = TRUE;
				level2Qhead = level2Qhead->next;			
			}
			tailRunning = tailRunning->next;
			while (iterator < L2Max){
				tailRunning = level2Qhead;
				level2Qhead = level2Qhead->next;
				iterator++;
			}
			tailRunning->next = NULL;
			levelCtrs[2]-=L2Max;
		}
		else if (levelCtrs[2] > 0){
			if (headSet == FALSE){					//checking if head was set
				headRunning = level2Qhead;
				tailRunning = level2Qhead;
				level2Qhead = NULL;
				level2Qtail = NULL;
				headSet = TRUE;						//headset = true
				levelCtrs[2] = 0;		
			}
			else{														//head wasn't set
				tailRunning->next = level2Qhead;	//next value is set to level 2 head
				tailRunning = level2Qtail;		//tail running is the end of level 2
				level2Qhead = NULL;
				level2Qtail = NULL;
				levelCtrs[2]= 0;							//all nodes were used so count is empty
			}
			tailRunning->next = NULL;		
		}
		/**debugging**/
		tempRunning = headRunning;
		iterator = 0;
		printf("about to go in while loop 2\n");
		while (tempRunning != NULL){
			printf("%d : %d \n", iterator, tempRunning->tid);
			tempRunning = tempRunning->next;	
			iterator++;		
		}
		/**debugging**/
		/********Adding level 3************/
		iterator = 0;
		if (levelCtrs[3] > L3Max){	//more than max 
			if (headSet == FALSE){					//head hasn't been set
				headRunning = level3Qhead;		//head is set to beginning of level 3 q
				tailRunning = level3Qhead;		//tail is set to head also??
				headSet = TRUE;								//head has been set
				level3Qhead = level3Qhead->next;			//move head over
			}
			tailRunning = tailRunning->next;			// move tail running to next for while loop
			while (iterator < L3Max){					//add first L3Max to running queue
				tailRunning = level3Qhead;
				level3Qhead = level3Qhead->next;
				iterator++;
			}
			tailRunning->next = NULL;
			levelCtrs[3]-=L3Max;		
		}
		else if (levelCtrs[3] > 0){					//there is at least 1
			if (headSet == FALSE){						//head has not been set previously
				headRunning = level3Qhead;			//head of running is head of level 3
				tailRunning = level3Qhead;			//tail of running is tail of level 3
				level3Qhead = NULL;
				level3Qtail = NULL;
				headSet = TRUE;									//head was set
				levelCtrs[3] = 0;								//all nodes have been taken so count is 0
			}	
			else{				//head has already been set
				tailRunning->next = level3Qhead;	//set next to the beginning of the third level
				tailRunning = level3Qtail;				//set the end to the end of the third level
				level3Qhead = NULL;
				level3Qtail = NULL;
				levelCtrs[3]= 0;									//taken all nodes so count is 0
			}			
			tailRunning->next = NULL;
		}
		/**debugging**/
		iterator = 0;
		printf("about to go in while loop 3\n");
		tempRunning = headRunning;
		while (tempRunning != NULL){
			printf("%d : %d \n", iterator, tempRunning->tid);
			tempRunning = tempRunning->next;	
			iterator++;		
		}
		//**debugging**/
		iterator = 0;
		tempRunning = headRunning;
		printf("PRINTING OUT RUNNING QUEUE TID's\n");
		while (tempRunning != NULL){
			printf("%d : %d \n", iterator, tempRunning->tid);
			tempRunning = tempRunning->next;		
		}
		printf("FINISHED PRINTING RUNNING QUEUE TID'S\n");
	return;
}

//Run them between cycles
void runThreads(){
	printf("Assign head running to current running\n");
	nextRunning = headRunning;
	while(nextRunning){
		//call to pthread_yield to swap contexts.
		timer(threads[nextRunning->tid]->priority);
		my_pthread_yield();	
		currentRunning = NULL; //(so if yield is called now, there's not a thread currently running).
		//if my_pthread_exit is called, then this will be null, check that first.
		if(threads[nextRunning->tid] == NULL){
			printf("Really done\n");//debugging statement
			//do not put back on MPQ
		}//if not null, check if status is preempted so we can lower priority
		else if(threads[nextRunning->tid]->threadState == PREEMPTED){
			printf("Preempted\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			//put back on MPQ
			addMPQ(threads[nextRunning->tid], mpqHeads[threads[nextRunning->tid]->priority], mpqTails[threads[nextRunning->tid]->priority]);
			levelCtrs[threads[nextRunning->tid]->priority] += 1;
		}
		else if(threads[nextRunning->tid]->threadState == WAITING){
			printf("Waiting\n");
			//don't put back on MPQ.
		}//if not premempted check for yield, will not lower priority
		else if(threads[nextRunning->tid]->threadState == YIELDED){
			printf("Yielded\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			//put back on MPQ
		}
		//otherwise it finished on it's own, but never called exit.
		else{
			printf("Done but not exited.\n");//debugging statement
			threads[nextRunning->tid]->threadState = DONE;
			//do not put back on MPQ
			//will have to remove it manually.  Either put that logic here, or in maintenance.  I vote for here. (faster)
			manualExit = nextRunning;
		}
		//move to next node.
//		levelCtrs[threads[nextRunning->tid]->priority] -= 1;
		nextRunning = (queueNode*)nextRunning->next;
		printf("restart loop\n"); //debugging statement.
	}
//	free(headRunning);
//	headRunning = NULL;	
//	free(tailRunning);
//	tailRunning = NULL;
}

/*signal handler for timer*/
void time_handle(int signum){
	printf("time_handler\n");
	//change status to PREEMPTED
	threads[currentRunning->tid]->threadState = PREEMPTED;
	//change priority +1 on tcb node(unless bottom level)
	if(threads[currentRunning->tid]->priority != (PRIORITY_LEVELS -1)){ //if we do more than 4 levels, change this number!
		threads[currentRunning->tid]->priority += 1;}
	//put back on tail of that new priority queue. (example in my_pthread_create())
	//careful to keep it here for when it swaps back in to main.
	//swapcontext back to main
	my_pthread_yield();
	
// 	int count = 0;
// 	printf("finished %d %d\n", signum,timerCounter);
// 	timerCounter++;
}

//add parameter for priority then take 2^priority and * QUANTUM
void timer(int priority){
	struct sigaction sigact;
	struct itimerval timer;

	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = &time_handle;
	sigaction (SIGVTALRM, &sigact, NULL);
	
	/*next value*/		//current timer interval? why next value?
	timer.it_interval.tv_sec = 0;		//0 seconds
	timer.it_interval.tv_usec = QUANTUM * (1 << priority) ;	//25 milliseconds / 1 quantum

	/*current value*/	//current timer value?
	timer.it_value.tv_sec = 0;		//0 seconds
	timer.it_value.tv_usec = QUANTUM * (1 << priority) ;	//25 milliseconds / 1 quantum
	
	
	setitimer (ITIMER_VIRTUAL, &timer, NULL);


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
	printf("Creating thread\n");
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
	if(!threads[0]){
		printf("Create main\n");
		if(getcontext(&(main_uctx)) == -1){
			printf("Couldn't get main context.\n");
			return -1;			
		}
		main_uctx.uc_stack.ss_sp = (char*)malloc(STACK_SIZE);
		main_uctx.uc_stack.ss_size = STACK_SIZE;
		main_uctx.uc_link = &sched_uctx;
		tcb* newNode = (tcb*)malloc(sizeof(tcb));
		newNode->tid = 0;
		newNode->context = main_uctx;
		newNode->threadState = ACTIVE;
		newNode->priority = 0;
		newNode->retval = NULL;
		newNode->waitingThreads = NULL;
		threads[0] = newNode;
		
		addMPQ(threads[0], &level0Qhead, &level0Qtail);
	}
	if(getcontext(&(newNode->context)) == -1){
		printf("Couldn't get new context.\n");
		return -1;			
	}
	
	//do context stuff
	ucontext_t uc = newNode->context;
	uc.uc_stack.ss_sp = (char*)malloc(STACK_SIZE);
	uc.uc_stack.ss_size = STACK_SIZE;
	uc.uc_link = &sched_uctx;//should be manager thread (scheduler? maintenance?)

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
	printf("OTHER IN MPQ!\n");
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

	//in case someone calls this before calling create.
	if(nextRunning == NULL){
		printf("Nothing running.\n");
		return -1;
	}	
	
	//TODO: Add more logic for main thread vs scheduler thread
	
	
	//if currentRunning is null, then this is being called by the main program on purpose
	if(currentRunning == NULL){
		currentRunning = nextRunning;
		if((swapcontext(&(sched_uctx),&((threads[currentRunning->tid])->context))) != 0){
			printf("Fuck\n");
		}
		printf("Back in yield??\n");	
	}
	//otherwise the currently running thread rquested to yield.
	else{
		if(threads[currentRunning->tid]->threadState != PREEMPTED){
			threads[currentRunning->tid]->threadState = YIELDED;
		}
		if((swapcontext(&((threads[currentRunning->tid])->context),&(sched_uctx))) != 0){
			printf("Fuck\n");
		}
		printf("What about now?\n");
	}
	printf("Back in yield\n");
	//swapcontext(&(where to save),&(new one to run))
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/* Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't 
	NULL, any return value from the thread will be saved. */
	printf("In my pthread exit\n");
	
	//in case someone calls this before calling create.
	if(currentRunning == NULL  && manualExit == NULL){
		printf("Nothing running.\n");
		return;
	}
	else if(currentRunning == NULL && manualExit != NULL){
		currentRunning = manualExit;
		return;
	}

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
	printf("in Join\n");
	//in case someone calls this before calling create.	
	if(currentRunning == NULL){
		printf("Nothing running.\n");
		return -1;
	}
	
	//check for valid thread.
	if(threads[thread] == NULL){
		printf("Thread does not exist, cannot join.\n");
		return -1;
	}
	//put thread onto waiting queue in tcb for thread passed in.
	queueNode* newNode = (queueNode*)malloc(sizeof(queueNode));
	newNode->tid = thread;
	newNode->next = NULL;
	newNode->retval = value_ptr; //TODO: *value_ptr??
	printf("Node stuff done\n");
	
	if(threads[thread]->waitingThreads == NULL){ //if i'm the first
		printf("NULL\n");
		threads[thread]->waitingThreads = newNode;
	}
	else{ //if others are already waiting.
		printf("WAITING\n");
		queueNode* currentNode = threads[thread]->waitingThreads;
		while(currentNode->next){
			currentNode = currentNode->next;
		}
		currentNode->next = newNode;
	}
	//move state to WAITING and yield
	threads[currentRunning->tid]->threadState = WAITING;
	my_pthread_yield();
	//**value_ptr is a buffer waiting for ret val. (unless not null)

	return 0;
};

	/**************** Mutex Methods ****************/
/* initial the mutex lock */
//returns 0 upon succes or errno value when there is an error
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	/* Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored. */
	
	if(mutex == NULL)
		return EINVAL; //errno for invalid argument

	//create new my_pthread_mutex_t struct.
	mutex->lockState = 0;
	mutex->waitQueue = (basicQueue*)malloc(sizeof(basicQueue));

	//init the wait queue
	mutex->waitQueue->head = NULL;
	mutex->waitQueue->tail = NULL;
	mutex->waitQueue->queueSize = 0;	
	
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	/* Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked. */
	/*if mutex already locked, add to wait queue
	if(mutex->lockState == 0){
		mutex->lockState == 1;

	}else{*/

	while (test_and_set(mutex->lockState) == TRUE){
	
		//make the thread wait
		threads[currentRunning->tid]->threadState = WAITING;

		//and add it to wait queue
		if(mutex->waitQueue->queueSize == 0){
			
			//create node
			waitQueueNode *firstNode = (waitQueueNode*)malloc(sizeof(waitQueueNode));
			firstNode->thread = threads[currentRunning->tid];
			firstNode->next = NULL;
			
			//set to head and tail			
			mutex->waitQueue->head = firstNode;
			mutex->waitQueue->tail = firstNode;

		}else{

			waitQueueNode *newTail = (waitQueueNode*)malloc(sizeof(waitQueueNode));
			newTail->thread = threads[currentRunning->tid];
			newTail->next = NULL;

			mutex->waitQueue->tail->next = newTail;
			mutex->waitQueue->tail = newTail;

		}

		mutex->waitQueue->queueSize++;

		//call scheduler
		scheduler();

	}

};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	/* Unlocks a give n mutex. */
	//check to see if anyone is waiting, if so, do not unlock, just "pass" the mutex on.  
	//Only unlock when no one else is waiting.

	if(mutex->waitQueue->queueSize == 0){
		mutex->lockState = 0;

	}else{

		//dequeue node from wait queue

		//grab the nextThread waiting in the mutex's queue
		tcb *nextThread = mutex->waitQueue->head->thread;

		//remove first node in waitQueue
		waitQueueNode *temp = mutex->waitQueue->head->next; //might have a problem here if its null but probably not
		free(mutex->waitQueue->head);
		mutex->waitQueue->head = temp;
		
		mutex->waitQueue->queueSize--;

		//pass the dequeued thread to the scheduler
		addMPQ(nextThread, mpqHeads[nextThread->priority], mpqTails[nextThread->priority]);

	}

};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	/* Destroys a given mutex. Mutex should be unlocked before doing so. */
	//check to make sure mutex is not locked first.
	
	//throw error if mutex does not exist
	if(mutex == NULL)
		return EINVAL;

	//throw error if mutex is lock (currently in use)
	if(mutex->lockState == 1)
		return EBUSY;

	free(mutex->waitQueue);

	return 0;
};

/*void testThreads(){
	return;
}*/

/*int main(int argc, char** argv){
	my_pthread_t mythread1, mythread2, mythread3, mythread4 = 0;
	my_pthread_create(&mythread1, NULL, (void*)&testThreads, NULL);
	my_pthread_create(&mythread2, NULL, (void*)&testThreads, NULL);
	my_pthread_create(&mythread3, NULL, (void*)&testThreads, NULL);
	my_pthread_create(&mythread4, NULL, (void*)&testThreads, NULL);

	createRunning();
	queueNode* temp = NULL;
	temp = headRunning;
	int i = 0;
	while(headRunning){
		i++;
		printf("%d\n",headRunning->tid);
		headRunning = headRunning->next;
	}
	printf("%d\n", i);
	runThreads();
	printf("Did it once\n");
	createRunning();
	temp = headRunning;
	i = 0;
	while(headRunning){
		i++;
		printf("%d\n",headRunning->tid);
		headRunning = headRunning->next;
	}
	printf("%d\n", i);
	runThreads();

}*/
