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
queueNode* maintRunning;	//thread iterator used for maintenenance cycle
queueNode* notUsedRunning = NULL;
 

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

int levelCtrs[PRIORITY_LEVELS] = {0};

//needs to change if there are more than 4 priority levels
int levelMax[PRIORITY_LEVELS] = {38, 17, 7, 2};//never make last priority level less than 2.

//global variable for when threads don't exit.
queueNode* manualExit = NULL;

/*
//DEBUGGING FUNCTION
void printWaitQueueMutex(basicQueue *waitQueue){

    printf("print Queue:\n");
    waitQueueNode *temp = waitQueue->head;
    int i;
    for(i = 0; i < waitQueue->queueSize; i++){

        if(temp == NULL)
            printf("NULL\n");
        else{
            printf("ThreadID: %d\n", temp->thread->tid);
            temp = temp->next;
        }
    }

    printf("EndPrint\n\n");

    return;
}*/

/**************** Additional Methods ****************/

int schedulerInit(){
	schedInit = TRUE;

	//create context to call scheduler 
	if(getcontext(&(sched_uctx)) == -1){
		printf("Couldn't get context.\n");
		return -1;			
	}
	ucontext_t uc = sched_uctx;
	uc.uc_stack.ss_sp = (char*)malloc(STACK_SIZE);
	uc.uc_stack.ss_size = STACK_SIZE;
	uc.uc_link = NULL;
	
	makecontext(&uc, scheduler,1,NULL); 
	sched_uctx = uc;
	swapcontext(&(threads[0]->context), &sched_uctx);
	return 0;
}

//initializes and runs scheduler until no threads exist
void scheduler(){	
//	printf("scheduler()\n");
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
//	printf("maintenanceCycle()\n");
	//once there are no more threads left change scheduler to 
	if(levelCtrs[0] + levelCtrs[1] + levelCtrs[2] + levelCtrs[3] == 0){
			schedInit = FALSE;	
	}
	else{
		int i = 0;
		while (i < PRIORITY_LEVELS){	//while i < PRIORITY_LEVELS: //right now this is 4
			maintRunning = (*(mpqHeads[i]));	//	current = MPQheads[i]
//			printf("1\n");
			while (maintRunning != NULL){	//	while current:
//				printf("2\n");
				if (maintRunning->ctr > CYCLES && threads[maintRunning->tid]->priority > 0){	//		if current->ctr > CYCLES:  
//					printf("3\n");
					threads[maintRunning->tid]->priority -=1;		//		threads[current->tid]->priority -= 1
				}
				maintRunning = maintRunning->next;		//		current = current->next;		
//				printf("4\n");
			}
			i++;
		}
		createRunning();
	}
}

//create list of threads to run between maintenance cycles
/*void createRunning(){
//	printf("In createRunning\nCurrent Counts: \n level0: %d\n level1: %d\n level2: %d\n level3: %d\n", levelCtrs[0], levelCtrs[1], levelCtrs[2], levelCtrs[3]);
	//Max nodes to be selected from priorty queue levels
	headRunning = (queueNode*)malloc(sizeof(queueNode));
	headRunning->tid = -1;
	headRunning->next = NULL;
	tailRunning = (queueNode*)malloc(sizeof(queueNode));
	tailRunning->tid = -1;
	tailRunning->next = NULL;
	queueNode*  tempRunningHead = (queueNode*)malloc(sizeof(queueNode));
	queueNode*  tempRunningTail = (queueNode*)malloc(sizeof(queueNode));

	int levelMax[4] = {7,5,3,1};
	
	bool headSet = FALSE;
	int i = 0;

	//-********Adding level 0***********
	if (levelCtrs[0] > levelMax[0]){
		
				printf("In if\n");
		tempRunningHead = level0Qhead;
		while(i < levelMax[0]-1 ){
			level0Qhead = level0Qhead->next;
			tempRunningTail = level0Qhead;
			i++;			
		}
		tempRunningTail->next = NULL;
		headRunning = tempRunningHead;
		
		printf("%d\n", headRunning->tid);
		tailRunning = tempRunningTail;
	
		level0Qhead = level0Qhead->next;	

		headSet = TRUE;
//			printf("In if\n");
		//-*decrement counter for level 0 by max*
		levelCtrs[0] -= levelMax[0];
//		printf("level 0 max = %d, level counter for 0: %d\n", levelMax[0], levelCtrs[0]);
//		printf("In createRunning\nCurrent Counts: \n level0: %d\n level1: %d\n level2: %d\n level3: %d\n", levelCtrs[0], levelCtrs[1], levelCtrs[2], levelCtrs[3]);
		//-*increment counter for nodes not used*
		tempRunningHead = level0Qhead;
		tempRunningTail = level0Qtail;
		tempRunningTail->ctr += 1;
			
		while(tempRunningHead != tempRunningTail && tempRunningHead != NULL){
			tempRunningHead->ctr +=1;
			tempRunningHead = tempRunningHead->next;			
		}
				
	}
	else if (levelCtrs[0] > 0){	// at least one, move entire level0 into running 
//		printf("In else if, %d <= %d\n", levelCtrs[0], L0Max);
		headRunning = level0Qhead;	//headRunning is the beginning of level 0
		tailRunning = level0Qtail;	//tailRunning is the end of level0
		level0Qhead = NULL;					//clear head of level 0
		level0Qtail = NULL;					//clear tail of level 0
		headSet = TRUE;							//head is set
		levelCtrs[0] = 0;						//counter is empty
//		printf("headRunning tid: %d \n tailRunning tid %d\n", headRunning->tid, tailRunning->tid);

	}
//printf("Finished adding level 0\n");
	
//-********Adding level 1************
//still need to decrement count for each level and add counters for nodes not used
	i = 0;
	if (levelCtrs[1] > levelMax[1]){
	
		tempRunningHead = level1Qhead;
		while(i < levelMax[1]){
			level1Qhead = level1Qhead->next;
			tempRunningTail = level1Qhead;
			i++;			
		}
		if (headSet == FALSE){ //head has not been set
			tempRunningTail->next = NULL;
			headRunning = tempRunningHead;
			tailRunning = tempRunningTail;
			level1Qhead = level1Qhead->next;
			headSet == TRUE;
		}
		else{//head has already been set
			tailRunning->next = tempRunningHead;
			tailRunning = tempRunningTail;
			level1Qhead = level1Qhead->next;
			
		}
		//-*decrement counter for level 0 by max*
		levelCtrs[1] -= levelMax[1];
		
		//-*increment counter for nodes not used*
		tempRunningHead = level1Qhead;
		tempRunningTail = level1Qtail;
		tempRunningTail->ctr += 1;
		while(tempRunningHead != tempRunningTail){
			tempRunningHead->ctr +=1;
			tempRunningHead = tempRunningHead->next;			
		}
				
	}
	else if (levelCtrs[1] > 0){
		if (headSet == FALSE){
			headRunning = level1Qhead;
//			tailRunning = level1Qhead;
			tailRunning = level1Qtail;
			level1Qhead = NULL;
			level1Qtail = NULL;
			headSet = TRUE;
			levelCtrs[1] = 0;	
//			printf("finished else if ->if\n");			
		}
		else{
			tailRunning->next = level1Qhead;
			tailRunning = level1Qtail;
			level1Qhead = NULL;
			level1Qtail = NULL;
			levelCtrs[1]= 0;
//			printf("finished else if ->else\n");
		}
		tailRunning->next = NULL;				
	}
i=0;
//printf("Finished adding level 1\n");
//-********Adding level 2************
if (levelCtrs[2] > levelMax[2]){
		tempRunningHead = level2Qhead;
		while(i < levelMax[2]){
			level2Qhead = level2Qhead->next;
			tempRunningTail = level2Qhead;
			i++;			
		}
		if (headSet == FALSE){ //head has not been set
			tempRunningTail->next = NULL;
			headRunning = tempRunningHead;
			tailRunning = tempRunningTail;
			level2Qhead = level2Qhead->next;
			headSet = TRUE;
		}
		else{//head has already been set
			tailRunning->next = tempRunningHead;
			tailRunning = tempRunningTail;
			level2Qhead = level2Qhead->next;
			
		}
		
		//-*decrement counter for level 0 by max*
		levelCtrs[2] -= levelMax[2];
		
		//-*increment counter for nodes not used*
		tempRunningHead = level2Qhead;
		tempRunningTail = level2Qtail;
		tempRunningTail->ctr += 1;
		while(tempRunningHead != tempRunningTail){
			tempRunningHead->ctr +=1;
			tempRunningHead = tempRunningHead->next;			
		}				
	}
	else if (levelCtrs[2] > 0){
		if (headSet == FALSE){					//checking if head was set
			headRunning = level2Qhead;
//			tailRunning = level2Qhead;
			tailRunning = level2Qtail;
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
			levelCtrs[2] = 0;							//all nodes were used so count is empty
		}
		tailRunning->next = NULL;		
	}

//	printf("Finished adding level 2\n");
//-*******Adding level 3************
//if l3Max becomes greater than 1 this code needs to change
	i = 0;
	if (levelCtrs[3] > levelMax[3]){
		tempRunningHead = level3Qhead;
		while(i < levelMax[3]){
			level3Qhead = level3Qhead->next;
			tempRunningTail = level3Qhead;
			i++;			
		}
		if (headSet == FALSE){ //head has not been set
			tempRunningTail->next = NULL;
			headRunning = tempRunningHead;
			tailRunning = tempRunningTail;
			level3Qhead = level3Qhead->next;
			headSet == TRUE;
		}
		else{//head has already been set
			tailRunning->next = tempRunningHead;
			tailRunning = tempRunningTail;
			level3Qhead = level3Qhead->next;
			
		}
		
		//-*decrement counter for level 0 by max*
		levelCtrs[3] -= levelMax[3];
		
		//-*increment counter for nodes not used*
		tempRunningHead = level3Qhead;
		tempRunningTail = level3Qtail;
		tempRunningTail->ctr += 1;
		while(tempRunningHead != tempRunningTail){
			tempRunningHead->ctr +=1;
			tempRunningHead = tempRunningHead->next;			
		}				
	}
	else if (levelCtrs[3] > 0){
		if (headSet == FALSE){					//checking if head was set
			headRunning = level3Qhead;
			tailRunning = level3Qtail;
			level3Qhead = NULL;
			level3Qtail = NULL;
			headSet = TRUE;						//headset = true
			levelCtrs[3] = 0;		
		}
		else{														//head wasn't set
			tailRunning->next = level3Qhead;	//next value is set to level 2 head
			tailRunning = level3Qtail;		//tail running is the end of level 2
			level3Qhead = NULL;
			level3Qtail = NULL;
			levelCtrs[3] = 0;							//all nodes were used so count is empty
		}
		tailRunning->next = NULL;			
		
	}
//	printf("Finished adding level 3\n");
	
	i = 0;
		
	tempRunningHead = headRunning;
	printf("--PRINTING OUT RUNNING QUEUE TID's (for debugging)\n");
	while (tempRunningHead != NULL){
		printf("%d : %d \n", i, tempRunningHead->tid);
		tempRunningHead = tempRunningHead->next;		
		i++;
	}
	
	printf("FINISHED PRINTING RUNNING QUEUE TID'S\n");
	return;
}
*/
void createRunning(){
//	printf("In createRunning\nCurrent Counts: \nleve0: %d\n Level1: %d\n level2: %d\n level3: %d\n", levelCtrs[0], levelCtrs[1], levelCtrs[2], levelCtrs[3]);
	//Max nodes to be selected from priorty queue levels
	headRunning = (queueNode*)malloc(sizeof(queueNode));
	headRunning->tid = -1;
	headRunning->next = NULL;
	tailRunning = (queueNode*)malloc(sizeof(queueNode));
	tailRunning->tid = -1;
	tailRunning->next = NULL;
	queueNode*  tempRunning = NULL;//(queueNode*)malloc(sizeof(queueNode));
	//tempRunning->tid = -1;
	//tempRunning->next = NULL;
	queueNode* tempHeadRunning = NULL;
	queueNode* tempTailRunning = NULL;


	int j = 0;
	while(j < (PRIORITY_LEVELS )){
//		printf("priority %d\n", j);
		if(levelCtrs[j] > levelMax[j]){
			//take max
//			printf("greater\n");
			tempHeadRunning = *(mpqHeads[j]);//level0Qhead; //set to head
			int i = 0;
			//move forward 6 times (for total of 7)
			while (i < levelMax[j]-1){
//				printf("%d\n", i);
				tempRunning = *mpqHeads[j];
				tempRunning = tempRunning->next;
				*(mpqHeads[j]) = (tempRunning);
				tempTailRunning = *(mpqHeads[j]);
				i++;
			}
//			printf("out of while\n");
//			printf("tid in greater: %d\n", (*(mpqHeads[j]))->tid);
			tempRunning = *mpqHeads[j];
			tempRunning = tempRunning->next;
			*(mpqHeads[j]) = (tempRunning);
			tempTailRunning->next = NULL;
			if(headRunning->tid == -1){
				headRunning = tempHeadRunning;
			}
			else{
				tailRunning->next = tempHeadRunning;
				if(headRunning->tid == tailRunning->tid){
					headRunning->next = tempHeadRunning;
				}
			}
			tailRunning = tempTailRunning;
			//mpqHeads[j] = &(tempRunning);
//			printf("tid: %d\n", (*(mpqHeads[j]))->tid);
		
			//increment other ctrs
			tempHeadRunning = *mpqHeads[j];
			while(tempHeadRunning){
				tempHeadRunning->ctr += 1;
				tempHeadRunning = tempHeadRunning->next;
			}
		
			//subtract 7 from levelCtrs
			levelCtrs[j] -= levelMax[j];
		}
		else if (levelCtrs[j] > 0){
//			printf("not greater\n");
			//take whole list
			tempHeadRunning = *(mpqHeads[j]);
			tempTailRunning = *(mpqTails[j]);
		
			if(headRunning->tid == -1){
				headRunning = tempHeadRunning;
			}
			else{
				tailRunning->next = tempHeadRunning;
				if(headRunning->tid == tailRunning->tid){
					headRunning->next = tempHeadRunning;
				}
			}
			tailRunning = tempTailRunning;
			*(mpqHeads[j]) = NULL;
			*(mpqTails[j]) = NULL;
			levelCtrs[j] = 0;
		}
		j++;
	}

	//tempRunning = headRunning;
	//printf("start\n");
	//while(tempRunning)
	//	printf("tid: %d\n", tempRunning->tid);	
	//	tempRunning = tempRunning->next;
	//
	//printf("end\n");

	j =0;
	while (j < PRIORITY_LEVELS){
		tempRunning = *(mpqHeads[j]);
		int i = 0;
		while(tempRunning){
			tempRunning->ctr += 1;
			tempRunning = tempRunning->next;
		}
		j++;
	}
}

//Run them between cycles
void runThreads(){
//	printf("start runThreads()\n");
	queueNode* tempRunning = NULL;
//	printf("Assign head running to current running\n");
	nextRunning = headRunning;
	while(nextRunning){
		//call to pthread_yield to swap contexts.
//		printf("id: %d priority: %d\n",nextRunning->tid, threads[nextRunning->tid]->priority);
		my_pthread_yield();	
//		printf("back in run\n");
		currentRunning = NULL; //(so if yield is called now, there's not a thread currently running).
		//if my_pthread_exit is called, then this will be null, check that first.
		if(threads[nextRunning->tid] == NULL){
//			printf("Really done\n");//debugging statement
			tempRunning = nextRunning;
			//do not put back on MPQ
		}//if not null, check if status is preempted so we can lower priority
		else if(threads[nextRunning->tid]->threadState == PREEMPTED){
//			printf("Preempted\n");//debugging statement
//			printf("%d\n", threads[nextRunning->tid]->priority);
			threads[nextRunning->tid]->threadState = ACTIVE;
			//put back on MPQ
//			printf("add\n");
			addMPQ(threads[nextRunning->tid], mpqHeads[threads[nextRunning->tid]->priority], mpqTails[threads[nextRunning->tid]->priority]);
//			printf("done adding\n");
			tempRunning = nextRunning;
		}
		else if(threads[nextRunning->tid]->threadState == WAITING){
//			printf("Waiting\n");
			tempRunning = nextRunning;
			//don't put back on MPQ.
		}//if not premempted check for yield, will not lower priority
		else if(threads[nextRunning->tid]->threadState == YIELDED){
//			printf("Yielded\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			//put back on MPQ
			addMPQ(threads[nextRunning->tid], mpqHeads[threads[nextRunning->tid]->priority], mpqTails[threads[nextRunning->tid]->priority]);
			tempRunning = nextRunning;
		}
		//otherwise it finished on it's own, but never called exit.
		else{
//			printf("Done but not exited.\n");//debugging statement
			threads[nextRunning->tid]->threadState = DONE;
			//do not put back on MPQ
			//will have to remove it manually.  Either put that logic here, or in maintenance.  I vote for here. (faster)
			manualExit = nextRunning;
			my_pthread_exit(NULL);
			tempRunning = nextRunning;
		}
		//move to next node.
//		levelCtrs[threads[nextRunning->tid]->priority] -= 1;
		nextRunning = (queueNode*)nextRunning->next;
		if(tempRunning != NULL){
			free(tempRunning);	//frees unless it's waiting
		}
//		printf("restart loop\n"); //debugging statement.
	}
}

/*signal handler for timer*/
void time_handle(int signum){
//	printf("time_handler\n");
	//change status to PREEMPTED
	threads[currentRunning->tid]->threadState = PREEMPTED;
	//change priority +1 on tcb node(unless bottom level)
	if(threads[nextRunning->tid]->mutexWaiting != TRUE){
		if(threads[currentRunning->tid]->priority != (PRIORITY_LEVELS - 1)){ 
			threads[currentRunning->tid]->priority += 1;
		}
	}
	//swapcontext back to main
	my_pthread_yield();
	
}

//add parameter for priority then take 2^priority and * QUANTUM
void timer(int priority){
//	printf("set timer()\n");
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
//	printf("addMPQ()\n");
	queueNode* newNode = (queueNode*)malloc(sizeof(queueNode));
	
	//prep new node 
	newNode->tid = thread->tid;
	newNode->next = NULL;
	newNode->ctr = 0; 
	
	//figure out where to add it.	
	if(*head == NULL){//list is empty
//		printf("added to head of MPQ\n");
		*head = newNode;
		*tail = newNode;
	}
	else if((*head)->tid == (*tail)->tid){//list only has one element
//		printf("added as 2nd element of MPQ\n");
		(*head)->next = newNode;
		*tail = newNode;
	}
	else{//list has more than 2 or more elements.
//		printf("added to end of MPQ\n");
		(*tail)->next = newNode;
		*tail = newNode;
	}	
//	printf("out\n");
	levelCtrs[threads[newNode->tid]->priority] += 1;
//	printf("exiting\n");
	return;
}

/**************** Given Methods to Override ****************/

	/**************** Thread Methods ****************/
/* create a new thread */  /* DONE */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	/* Creates a pthread that executes function. Attributes are ignored, arg is not. */
//	printf("my_pthread_create()\n");
	//initialize to some fake value
	my_pthread_t ID = -1;
	
	if(!headThread){
		headThread = (nextId*)malloc(sizeof(nextId));
		headThread->next = NULL;
		headThread->readyIndex = -1;
	}
	
	//fill up array before moving to linked list of readyIndexes
	if (useThreadId == FALSE){
//		printf("use array\n");
		ID = threadCtr;
		threadCtr++;
		//thread is full, switch bool to TRUE.
		if(threadCtr == MAX_THREADS){
			useThreadId = TRUE;	
		}
	}
	//array is filled, move over to linked list of readyIndexes
	else{
//		printf("don't use array!\n");
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
//			printf("No threads available.\n");
			return -1; //couldn't create it, array is full.
		}
	}
	//create a new tcb
	tcb* newNode = (tcb*)malloc(sizeof(tcb));
	//assign thread ID
	newNode->tid = ID;
	if(!threads[0]){
//		printf("Create main\n");
		if(getcontext(&(main_uctx)) == -1){
//			printf("Couldn't get main context.\n");
			return -2;			
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
//		printf("Couldn't get new context.\n");
		return -2;			
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
//	printf("OTHER IN MPQ!\n");
	
	//Initialize the scheduler if it's not already initialized
	if(!schedInit){
		if(schedulerInit() != 0){
			return -3;
		}
//	}else{
//		my_pthread_yield();
	}
	return 0;
}	

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	/* Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out 
	and another can be scheduled if one is waiting. */
//	printf("In my_pthread_yield()\n");//debugging statement
//	printf("Current running id: %d\n",nextRunning->tid);
	//in case someone calls this before calling create.
	if(nextRunning == NULL){
//		printf("Error. Nothing running, cannot yield.\n");
		return -1;  
	}	
//	printf("nextRunning not null\n");
	
	//TODO: Add more logic for main thread vs scheduler thread
	
	
	//if currentRunning is null, then this is being called by the main program on purpose
	if(currentRunning == NULL){
		currentRunning = nextRunning;
//		printf("it was null about to swap %d\n", threads[nextRunning->tid]->priority);
		timer(threads[nextRunning->tid]->priority);
		if((swapcontext(&(sched_uctx),&((threads[currentRunning->tid])->context))) != 0){
//			printf("couldn't swap contexts\n");
			return -2;
		}
	}
	//otherwise the currently running thread rquested to yield.
	else{
//		printf("currentRunning not null %d\n",currentRunning->tid);
		if(threads[currentRunning->tid]->threadState != PREEMPTED && threads[currentRunning->tid]->threadState != WAITING){
//			printf("not preempted or waiting\n");
			threads[currentRunning->tid]->threadState = YIELDED;
		
		}
//		else{
//			printf("prempted or waiting\n");
//		}
		if((swapcontext(&((threads[currentRunning->tid])->context),&(sched_uctx))) != 0){
//			printf("couldn't swap context\n");
			return -2;
		}
//		printf("What about now?\n");
	}
	//swapcontext(&(where to save),&(new one to run))
	return 0;
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/* Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't 
	NULL, any return value from the thread will be saved. */
//	printf("In my_pthread_exit()\n");
	
	//in case someone calls this before calling create.
	if(currentRunning == NULL  && manualExit == NULL){
//		printf("Nothing running.\n");
		return;
	}
	else if(currentRunning == NULL && manualExit != NULL){
//		printf("manual\n");
		currentRunning = manualExit;
		manualExit = NULL;
	}

	//before exiting, check to see if anyone else joined.
	/*Unable to test this part until we figure out how to ref library in other file. */
	queueNode* tempNode = NULL;
	if(threads[currentRunning->tid]->waitingThreads != NULL){//there are waiting threads
		queueNode* currentNode = threads[currentRunning->tid]->waitingThreads;
		while(currentNode){
			(threads[currentNode->tid]->retval) = value_ptr;
			
			//TODO: put back on mpq!  I think this code will work, but cannot test it yet, and might really blow up.
			//mpqTails[threads[currentNode->tid]->priority]->next = currentNode;
			//mpqTails[threads[currentNode->tid]->priority] = currentNode;
			
			tempNode = currentNode;
//			printf("%d %d\n",currentRunning->tid, currentNode->tid);
			addMPQ(threads[currentNode->tid], mpqHeads[threads[currentNode->tid]->priority], mpqTails[threads[currentNode->tid]->priority]);
			
			currentNode = currentNode->next;
		}
	}
	//Pass ID into tailThread, set curr tail = to this number.	
	if(headThread == NULL || headThread->readyIndex == -1){//none in queue yet
//		printf("First thread ID\n");
		headThread->readyIndex = currentRunning->tid;	
		headThread->next = NULL;
		tailThread = headThread;
	}
	else if(headThread == tailThread){//only one in queue
//		printf("Second thread ID\n");
		nextId* tmpThread = (nextId*)malloc(sizeof(nextId));
		tmpThread->readyIndex = currentRunning->tid;
		tmpThread->next = NULL;
		headThread->next = tmpThread;
		tailThread = tmpThread;
	}
	else{//two or more in queue
//		printf("Lots in\n");
		nextId* tmpThread = (nextId*)malloc(sizeof(nextId));
		tmpThread->readyIndex = currentRunning->tid;
		tmpThread->next = NULL;
		tailThread->next = tmpThread;
		tailThread = tmpThread;
	}
	free((threads[currentRunning->tid]));
	threads[currentRunning->tid] = NULL;
//	printf("leaving exit\n");
	currentRunning = NULL;
	
	return;
	
}

/* wait for thread termination */ 
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	/* Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one 
	it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.*/
//	printf("in my_pthread_join()\n");
	//in case someone calls this before calling create.	
	if(currentRunning == NULL){
//		printf("Nothing running.\n");
		return -1;
	}
	//check for valid thread.
	if(threads[thread] == NULL){
//		printf("Thread does not exist, cannot join.\n");
		return -2;
	}
	//put thread onto waiting queue in tcb for thread passed in.
	queueNode* newNode = (queueNode*)malloc(sizeof(queueNode));
	newNode->tid = currentRunning->tid;
	newNode->next = NULL;
//	printf("Node stuff done\n");

	threads[currentRunning->tid]->retval = value_ptr;
	
	if(threads[thread]->waitingThreads == NULL){ //if i'm the first
//		printf("NULL\n");
		threads[thread]->waitingThreads = newNode;
	}
	else{ //if others are already waiting.
//		printf("WAITING\n");
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
}

	/**************** Mutex Methods ****************/
/* initial the mutex lock */
//returns 0 upon succes or errno value when there is an error
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	/* Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored. */
//	printf("my_pthread_mutex_init()\n");
	if(mutex == NULL){
//		printf("NULL\n");
		return EINVAL; //errno for invalid argument
	}
	//create new my_pthread_mutex_t struct.
	mutex->lockState = FALSE;
	mutex->owner = (tcb*)malloc(sizeof(tcb));
	mutex->waitQueue = (basicQueue*)malloc(sizeof(basicQueue));

	//init the wait queue
	mutex->waitQueue->head = NULL;
	mutex->waitQueue->tail = NULL;
	mutex->waitQueue->queueSize = 0;	
//	printf("initialized\n");
	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	/* Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked. */
//	printf("my_pthread_mutex_lock()\n");
	if(mutex == NULL)
		return EINVAL;  
	
	while (__sync_lock_test_and_set(&(mutex->lockState), TRUE)){
		printf("in the while\n");  //this never prints

		//if owner enters here, pass it through
		if(mutex->owner->tid == currentRunning->tid)
			return 0;

		//make the thread wait
		threads[currentRunning->tid]->threadState = WAITING;

		//dealing with priority inversion with priority inheritance
		if((threads[currentRunning->tid]->priority) > (mutex->owner->priority)){

			//put new thread at head of wait queue
			waitQueueNode *newHead = (waitQueueNode*)malloc(sizeof(waitQueueNode));
			newHead->thread = threads[currentRunning->tid];

			newHead->next = mutex->waitQueue->head;
			mutex->waitQueue->head = newHead;

			mutex->owner->priority = 0;
			mutex->owner->mutexWaiting = TRUE;

//			printWaitQueueMutex(mutex->waitQueue); //DEBUGGING

			//run owner and then the new high priority thread
//			if(swapcontext(&(newHead->thread->context),&(mutex->owner->context)) != 0){
//				//print errno if it did not go through
//				errno = ENOMEM;
//				perror("Error with swapcontext in pthread_mutex_lock ");
//				return ENOMEM;
//			}
			

		}else{

			//put new thread at end of wait queue
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
//			printWaitQueueMutex(mutex->waitQueue); //DEBUGGING
			//call scheduler
//			printf("%d\n", currentRunning->tid);
			my_pthread_yield();

		}

	}
	
	//fetch owner of the lock
	mutex->owner = threads[currentRunning->tid];
//	printf("owner: %d\n",mutex->owner->tid);
	return 0;

}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	/* Unlocks a give n mutex. */
	//check to see if anyone is waiting, if so, do not unlock, just "pass" the mutex on.  
	//Only unlock when no one else is waiting.
//	printf("mutex_unlock\n");
	if(mutex == NULL)
		return EINVAL;  
	
	if(mutex->waitQueue->queueSize == 0){
		mutex->lockState = 0;
//		printf("no one waiting\n");
	}else{
//		printf("someone waiting\n");
		//dequeue node from wait queue

		//grab the nextThread waiting in the mutex's queue
		tcb *nextThread = mutex->waitQueue->head->thread;

		//remove first node in waitQueue
		waitQueueNode *temp = mutex->waitQueue->head->next; //might have a problem here if its null but probably not		
		free(mutex->waitQueue->head);
		mutex->waitQueue->head = temp;
		
		mutex->waitQueue->queueSize--;

		//change status back to active (from waiting)
		nextThread->threadState = ACTIVE;
		
		//set owner
		mutex->owner = nextThread;
		
//		printWaitQueueMutex(mutex->waitQueue); //DEBUGGING

		//pass the dequeued thread to the scheduler
		addMPQ(nextThread, mpqHeads[nextThread->priority], mpqTails[nextThread->priority]);
		
		//give them the lock.

	}
//	printf("current: %d\n", currentRunning->tid);
	return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	/* Destroys a given mutex. Mutex should be unlocked before doing so. */
	//check to make sure mutex is not locked first.
//	printf("mutex_destroy\n");
	//throw error if mutex does not exist
	if(mutex == NULL)
		return EINVAL;  

	//throw error if mutex is lock (currently in use)
	if(mutex->lockState == 1)
		return EBUSY;

	free(mutex->waitQueue);

	return 0;
}
