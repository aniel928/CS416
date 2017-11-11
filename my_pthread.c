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

//queueNodes used throughout
queueNode* headRunning = NULL;
queueNode* tailRunning = NULL;
queueNode* currentRunning = NULL; //the one currently running
queueNode* nextRunning = NULL; //the one that's next to run
 
//initialize array of thread pointers to null.
tcb* threads[MAX_THREADS] = {NULL};

//prevent linear search by having a queue of ready numbers.
nextId* headThread = NULL;
nextId* tailThread = NULL;
nextId* headMutex = NULL;
nextId* tailMutex = NULL;

//don't use until array is filled to begin with
int threadCtr = 1;//not 0 because 0 is saved for main thread.
bool useThreadId = FALSE;

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

//count how many nodes are in each level
int levelCtrs[PRIORITY_LEVELS] = {0};

//global variable for when threads don't call exit, need mutex to control
queueNode* manualExit = NULL;
my_pthread_mutex_t* mutexManualExit = NULL;

//declare 8MB chunk of data for malloc
static char* memory = NULL;

//ptrs for OS stuff
metaData* currMD = NULL;
metaData* prevMD = NULL;
metaData* headOS = NULL;
metaData* iterMD = NULL;

//ptrs for USER stuff
memStruct * memHead = NULL;
memStruct * memNew = NULL;
memStruct * memFollow = NULL;
memStruct * memNext = NULL;

//save in case main method calls malloc before making new thread.
PTE* firstPTE = NULL;  
//TODO: add this into Mike's code later

//boolean for malloc init
/***************** Malloc Stuff *********************/

//This function is purely for testing, leaving in now but not necessary when we submit
void showData(){
	memStruct * memTest = memHead;
	printf("----------PRINTING LINKED LIST DATA----------\n");
	int i = 0;
	int pc = 0;
	char* boolVal = "FALSE";
	if (memTest == NULL){
		printf("All empty?\n");
	}
	else{
		while (memTest!=NULL){
	
			if(memTest->inUse == TRUE){
				boolVal = "TRUE";
			}
			else{
				boolVal = "FALSE";
			}
			printf("Page number: %d, Page count: %d, inUse? %s, index: %d\n", i, memTest->pageCount, boolVal, pc*PAGESIZE);
			pc += memTest->pageCount;
			memTest = memTest->next;
			i++;
		}
	}
	printf("Size of memory: %d | Number of pages: %d | Page size: %d\n", MEMORYSIZE, NUMOFPAGES, PAGESIZE);
	printf("Bytes used: %d \t| Pages used: %d (including unused)\n", pc * PAGESIZE, usedPages());
	
}

//Checks to see how many pages have been used, need to switch to page table
int usedPages(){
	memStruct * memUP = memHead;
	int count = 0;
	while (memUP != NULL){
		count += memUP->pageCount;
		memUP = memUP->next;
	}
	return count;
}


void spaceBetween(metaData* currMD, int size, int bytesFree){
	currMD->used = 1;   
	currMD->bytes = size;
	currMD->ptr = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
	iterMD = currMD->ptr;
	iterMD->used = 0;
	iterMD->bytes = bytesFree - size - sizeof(metaData); //bytes reamining are total counted above minus the size we allocated and the size of this metadata
	iterMD->ptr = (metaData*)((long)iterMD + sizeof(metaData) + iterMD->bytes);				
}

int combineFreeStuff(metaData* iterMD){
	int bytesFree = iterMD->bytes;
	while(iterMD->ptr && (iterMD->ptr)->used == 0){
		//if the next one, or two, or several are free, combine them.
		iterMD = iterMD->ptr;
		bytesFree += sizeof(metaData) + iterMD->bytes;
	}
	return bytesFree;
}

void* myallocate(int size, char* file, int line, int threadId){
	//aligning pages
	if(!memory){
		posix_memalign((void*)&memory,PAGESIZE, MEMORYSIZE);
	}
	//error catching in case trying to malloc 0 bytes.
	if(size <= 0){
		printf("No bytes requested, file: %s, line: %d\n", file, line);
		return NULL;
	}
	else{
		printf("%d bytes requested\n", size);
	}

	//library request - don't use paging.
	if(!threadId){
		printf("Coming from library\nUse OS section\n");

		//if no head is set yet
		if(!headOS){
			currMD = (metaData*)memory;
			currMD->used = 1;
			currMD->bytes = size;
			currMD->ptr = NULL;
			headOS = currMD;
		}
		//if head has been used and then freed.
		else if (headOS->used == 0){
			currMD = headOS;
			iterMD = currMD;

			int bytesFree = combineFreeStuff(iterMD);

			if(bytesFree > 0){
				printf("bytes free: %d\n", bytesFree);
			}
			
			//if it's at least as big as space, but less than enough space to put metadata
			if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
				currMD->used = 1;
				printf("Asked for %d, got %d\n",size,currMD->bytes);
			}
			//if it's bigger than the space and there's enough room for metadata
			else if(bytesFree > size){
				spaceBetween(currMD, size, bytesFree);
			}
			//won't fit
			else{
				//set currentMD to head;
				prevMD = headOS;
				currMD = headOS;

				//while ptr != NULL, move forward.
				while(currMD->ptr){ 
					prevMD = currMD;
					currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
					if(currMD->used == 0){
 						iterMD = currMD;
						int bytesFree = combineFreeStuff(iterMD);
 						if(bytesFree > 0){
							printf("bytes free: %d\n", bytesFree);
						}
						//if there are enough bytes free to squeeze it in, but not enough for metadata, give them all
						if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
							currMD->used = 1;
							printf("Asked for %d,  got %d\n",size,currMD->bytes);
							return (void*)((long)currMD + sizeof(metaData));
						}
						//if it's bigger than the space and there's enough room for metadata, put it in and put in metadata for rest
						else if(bytesFree > size){
							spaceBetween(currMD, size, bytesFree);
							printf("ok\n");
							return (void*)((long)currMD + sizeof(metaData));
						}
						//else if it won't fit, keep looping through.
					}
				}
				//do it all one more time forward, loop broke when PTR was null, so get to that free space
				prevMD = currMD;
				currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
				printf("forward one last time, %d/%d bytes used\n", ((long)currMD + sizeof(metaData) + size) - (long)headOS, OSSIZE);

				//Please don't let this happen, 1MB should be plenty
				if(((long)currMD - (long)headOS + sizeof(metaData) + size) > (OSSIZE)){
					printf("Not enough space in OS section.  Make bigger.\n");
					return NULL;
				}
				//if it didn't return ^^up there, then use it!
				currMD->used = 1;
				currMD->bytes = size;
				currMD->ptr = NULL;
				prevMD->ptr = currMD;
			}
			//return the pointer
			return (void*)((long)currMD + sizeof(metaData));
		}
		//head is being used currently
		else{
			//set currentMD to head;
			prevMD = headOS;
			currMD = headOS;
			//while ptr != NULL, move forward.
			while(currMD->ptr){ 
				prevMD = currMD;
				currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
				if(currMD->used == 0){

					iterMD = currMD;
					int bytesFree = combineFreeStuff(iterMD);

					if(bytesFree > 0){
						printf("bytes free: %d\n", bytesFree);
					}				
					if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
						currMD->used = 1;
						printf("Asked for %d, got %d\n",size,currMD->bytes);
						return (void*)((long)currMD + sizeof(metaData));
					}
					//if it's bigger than the space and there's enough room for metadata
					else if(bytesFree > size){
						spaceBetween(currMD, size, bytesFree);
						printf("ok\n");
						return (void*)((long)currMD + sizeof(metaData));
					}
					//else if it won't fit, keep looping through.
				}
			}
			prevMD = currMD;
			currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
			printf("forward one last time, %d/%d bytes used\n", ((long)currMD + sizeof(metaData) + size) - (long)headOS, OSSIZE);

			//Please don't let this happen, 1MB should be plenty
			if(((long)currMD - (long)headOS + sizeof(metaData) + size) > (OSSIZE)){
				printf("Not enough space in OS section.  Make bigger.\n");
				return NULL;
			}
			//if it didn't return ^^up there, then use it!
			currMD->used = 1;
			currMD->bytes = size;
			currMD->ptr = NULL;
			prevMD->ptr = currMD;
		}
		return (void*)((long)currMD + sizeof(metaData));
	}
	//thread request - use paging
	else{
		//get calling thread
		int tid = -1;
		if(!currentRunning){
			tid = 0; //in case main calls malloc before creating thread.
		}
		else{
			tid = currentRunning->tid;
		}
		printf("Coming from macro\nUse User section\n");
		printf("Thread number: %d\n", tid);
		
		if(size <= (PAGESIZE - sizeof(metaData))){
			printf("return a page\n");
			printf("add to page table\n");
		}
		else{
			//Mike's code should work here

/*******************************************************************************/

			int usePgs= usedPages();
			
			//determining how many pages are needed to return
			int pageCount = ((size + sizeof(memStruct))/PAGESIZE) + 1;
			printf("Need %d pages for this Malloc\n", pageCount);
				
			//Check if head has been built
			if (memHead == NULL){	//memHead not built yet
				printf("Head is NULL\n");
				
				if(size > USERSIZE - sizeof(memStruct)){
					printf("Too big.\n");
					return NULL;
				}
				//add if statement ot make sure not greater than USERSIZE - sizeof(memStruct)
		
				//create head
				memHead = (memStruct*)(memory + OSSIZE);
				memHead->pageCount = pageCount;
				memHead->currUsed = size + sizeof(memStruct);
				memHead->inUse = TRUE;
				memHead->next = NULL;
				
				//returning the front of the string
				int retAmt = PAGESIZE * pageCount - sizeof(memStruct);
				printf("returning %d bytes\n", retAmt);			
				void * retStr = (void*)(memHead + sizeof(memStruct)-1);
				return retStr;
			}
			else{//head has been previously set
				printf("Head set, need %d pages\n", pageCount);
				memNew = memHead;
				memFollow = memNew;
				memNew = memNew->next;

				//find out how many pages have been used
				int totalPages = usedPages();
				
				//reset memNew
				memNew = memHead->next;
				int currPages = memHead->pageCount;
				int k = 0;

				while (memNew != NULL){
					if (memNew->inUse == FALSE && memNew->pageCount >= pageCount){ //may need to implement how many bytes are actually left
						break;
					}
					currPages += memNew->pageCount;
					memFollow = memNew;
					memNew = memNew->next;	
					k++;
				}
				if(memNew == NULL){ //found page is NULL
					//need to make sure that the available space can hold the requested size
					if ((usePgs + pageCount) > NUMOFPAGES){
						printf("Can't give this many bytes/pages for this malloc, returning\n");
						return NULL;
					}				
					memNew = (memStruct*)(memory + OSSIZE +(totalPages * PAGESIZE - 1)); //This line is the cause of about an hour worth of segfaulting
					memFollow->next = memNew;
					memNew->inUse = TRUE;
					memNew->pageCount = pageCount;
					memNew->currUsed = sizeof(memStruct)+ size;
					memNew->next = NULL;
									printf("1\n");
				}
				else{//found page that was previously used BUT it was free'd

					printf("found page that was previously used but it was freed %d\n", memNew->pageCount);
					memNew->inUse = TRUE;		//set back to in use
					memNew->currUsed = sizeof(memStruct)+size;
					//If the amount of pages that this previously used page is more than what is needed, create a new memStruct for the leftover pages and set its inUse to false
					if (memNew->pageCount > pageCount){
						//if the freed section has more pages than the pages needed, proceed accordingly
						//determine how many more pages there are
						int index = currPages * PAGESIZE - 1 + pageCount * PAGESIZE;
						memNext = (memStruct*)(memHead + index);
						memNext->inUse = FALSE;
						memNext->pageCount = memNew->pageCount - pageCount;
						memNext->next = memNew->next;
						memNext->currUsed = sizeof(memStruct);
						memNew->next = memNext;
						memNew->pageCount = pageCount;
					}
				}
				void* retStr = (void*)(memNew +sizeof(memStruct));
				return retStr;
			}		
	/****************************************************************************/
		}
	}
	return NULL;
}

void mydeallocate(void* ptr, char* file, int line, int threadId){
	printf("Free stuff\n");
	if(!ptr){
		return;
	}

	

	if(!threadId){
		printf("Free coming from library.\n");
		((metaData*)((long)ptr - sizeof(metaData)))->used = 0;
	}
	else{
		int tid = -1;
		if(currentRunning){
			tid = currentRunning->tid;
		}
		else{
			tid = 0;
		}
		printf("Free coming from thread number %d\n",tid);
	

/**********************************************************************/

		int a = 0;
		bool found = FALSE;		//will flip if found in loop
		int pageNum =  ((ptr +1  - (void*)memHead)/PAGESIZE + 1)-1;
		int search = 0;
		memStruct * memFree = memHead;
		while (search <= pageNum && memFree != NULL){
			search += memFree->pageCount;
			if (search == pageNum){
				memFree->inUse = FALSE;
				found = TRUE;
				break;
			}		
			memFree = memFree->next;
		}
		if (found == FALSE){
			printf("could not find anything to free\n");
		}
		else{
			printf("Great success!\n");
		}
		
		
/************************************************************************/
	}
	return;
}
/**************** Additional Methods ****************/


int schedulerInit(){
//	printf("schedulerInit()\n");
	schedInit = TRUE;

	//create context to call scheduler 
	if(getcontext(&(sched_uctx)) == -1){
		return -1;			
	}
	ucontext_t uc = sched_uctx;
	uc.uc_stack.ss_sp = (char*)myallocate(STACK_SIZE, __FILE__,__LINE__,LIBREQ);
	uc.uc_stack.ss_size = STACK_SIZE;
	uc.uc_link = NULL;
	
	makecontext(&uc, scheduler,1,NULL); 
	sched_uctx = uc;
	//swap to scheduler
	swapcontext(&(threads[0]->context), &sched_uctx);
	return 0;
}

//initializes and runs scheduler until no threads exist
void scheduler(){	
//	printf("scheduler()\n");
	//until no threads are left keep maintaining, then running, maintaining, then running, etc.

	//initialize this mutex for use later.  Want it somewhere where it won't be called repeatedly.	
	my_pthread_mutex_init(mutexManualExit, NULL);

	
	while(schedInit){
		maintenanceCycle();
		if(schedInit){//make sure it wasn't uninitialized in maintenance loop.
			runThreads();
		}
	} 	
	
	//now that we're out of the scheduler, destroy the mutex.
	my_pthread_mutex_destroy(mutexManualExit);

	free_things();
	printf("DONE\n");
}

//do some maintenance between running cycles
void maintenanceCycle(){	
//	printf("maintenanceCycle()\n");

	//this needs to change if more than 4 priority levels
	if(levelCtrs[0] + levelCtrs[1] + levelCtrs[2] + levelCtrs[3] == 0){
		//once there are no more threads left, stop running scheduler
		schedInit = FALSE;	
	}
	//otherwise, do some maintenance
	else{
		queueNode* maintRunning = NULL;	//iterate through MPQ
		queueNode* freeRunning = NULL;	//store node to free.
		queueNode* prevMaint = NULL;
		int i = 0;
		//go through each priority level
		while (i < PRIORITY_LEVELS){	
			maintRunning = (*(mpqHeads[i]));	
			//promote priority of those threads that have been waiting too long (but are not already level 0
			while (maintRunning != NULL){	
				if (maintRunning->ctr > CYCLES && threads[maintRunning->tid]->priority > 0){	
					//reschedule (which creates new node, so store current node to free)
					threads[maintRunning->tid]->priority -=1;	
					//this may be wrong.
					freeRunning = maintRunning;
					//if it's the head, move forward first.
					if (maintRunning == (*(mpqHeads[i]))){
						(*(mpqHeads[i])) = (*(mpqHeads[i]))->next;
					}
					levelCtrs[i]--; //about to add to another queue, so remove from this one.
					addMPQ(threads[maintRunning->tid], mpqHeads[threads[maintRunning->tid]->priority], mpqTails[threads[maintRunning->tid]->priority]);
				}
				//move forward and free current if necessary.
				if(freeRunning == NULL){
				}
				maintRunning = maintRunning->next;
				if(freeRunning != NULL){
					mydeallocate(freeRunning,__FILE__,__LINE__,LIBREQ);
					freeRunning = NULL;
				}
			}
			i++;
		}
		//now that priority maintenance is done, make the running queue
		createRunning(); 
	}
}

//Create running queue for scheduler to run before next maintenance cycle
void createRunning(){
//	printf("In createRunning()\n");
	//Max nodes to be selected from priority queue levels
	//needs to change if there are more than 4 priority levels
	int levelMax[PRIORITY_LEVELS] = {7, 4, 3, 2};//never make last priority level less than 2.
	//set dummy nodes
	headRunning = (queueNode*)myallocate(sizeof(queueNode),__FILE__,__LINE__,LIBREQ);
	headRunning->tid = -1;
	headRunning->next = NULL;
	tailRunning = NULL;
	
	//helper nodes
	queueNode*  tempRunning = NULL;
	queueNode* tempHeadRunning = NULL;  
	queueNode* tempTailRunning = NULL;
	


	int j = 0;

	//go through each priority level
	while(j < (PRIORITY_LEVELS )){
		//if there are more in the list than the maximum to run, take the first x amount (where x is max per level)
		if(levelCtrs[j] > levelMax[j]){
			tempHeadRunning = *(mpqHeads[j]);//set to head of current level
			int i = 0;
			//move forward one less time than max (for total of max)
			while (i < levelMax[j]-1){
				tempRunning = *mpqHeads[j];
				tempRunning = tempRunning->next;
				*(mpqHeads[j]) = (tempRunning);
				tempTailRunning = *(mpqHeads[j]);
				i++;
			}
			//move forward one last time and store back as level head.
			tempRunning = *mpqHeads[j];
			tempRunning = (*mpqHeads[j])->next;
			
			*(mpqHeads[j]) = tempRunning;
			
			//set last node of running list to NULL (b/c it's last)
			tempTailRunning->next = NULL;
			//if head is not already set, set it
			if(headRunning->tid == -1){
//				mydeallocate(headRunning,__FILE__,__LINE__,LIBREQ);
				headRunning = tempHeadRunning;
			}
			else{
				tailRunning->next = tempHeadRunning;
				//if head is set, make sure it's not equal to tail (because that's what happens with one node)
				if(headRunning->tid == tailRunning->tid){
					headRunning->next = tempHeadRunning;
				}
			}
			
			//no matter what, set tail
			tailRunning = tempTailRunning;

			//increment ctrs for those that didn't get put onto the running queue.
			tempHeadRunning = *mpqHeads[j];
			while(tempHeadRunning){

				tempHeadRunning->ctr += 1;
				tempHeadRunning = tempHeadRunning->next;
			}
		
			//subtract maximum from levelCtrs
			levelCtrs[j] -= levelMax[j];
		}
		//otherwise, as log as there's more than zero nodes in list, take the whole list and move it onto running queue.
		else if (levelCtrs[j] > 0){
			//take whole list
			tempHeadRunning = *(mpqHeads[j]);
			tempTailRunning = *(mpqTails[j]);

			//if head is not already set, set it 		
			if(headRunning->tid == -1){
//				mydeallocate(headRunning,__FILE__,__LINE__,LIBREQ);
				headRunning = tempHeadRunning;
			}
			else{
				tailRunning->next = tempHeadRunning;
				//if head is set, make sure it's not equal to tail (because that happens with one node).
				if(headRunning->tid == tailRunning->tid){
					headRunning->next = tempHeadRunning;
				}
			}
			//no matter what, set tail
			tailRunning = tempTailRunning;
			
			//We took everything, so NULL out head/tail and reset counter
			*(mpqHeads[j]) = NULL;
			*(mpqTails[j]) = NULL;
			levelCtrs[j] = 0;
		}
		j++;
	}
}

//Run them between cycles
void runThreads(){
//	printf("start runThreads()\n");
	queueNode* freeRunning = NULL; //store node to free
	nextRunning = headRunning; //iterator
	while(nextRunning){	//until run list is empty
		//call to pthread_yield to swap context to next thread.
		my_pthread_yield();	
		currentRunning = NULL; //so if yield is called now, it knows there's not a thread currently running so it must be the scheduler.
		//if it exited during it's runtime
		if(threads[nextRunning->tid] == NULL){
//			printf("Really done\n");//debugging statement
			freeRunning = nextRunning;  //so free the node
		}
		//was it premempted? Add it back to MPQ (priority is lowered during interrupt handler.
		else if(threads[nextRunning->tid]->threadState == PREEMPTED){	
//			printf("Preempted\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			addMPQ(threads[nextRunning->tid], mpqHeads[threads[nextRunning->tid]->priority], mpqTails[threads[nextRunning->tid]->priority]);
			freeRunning = nextRunning;
		} //can combine these two if we don't care about print statements.
		else if(threads[nextRunning->tid]->threadState == YIELDED){
//			printf("Yielded\n");//debugging statement
			threads[nextRunning->tid]->threadState = ACTIVE;
			addMPQ(threads[nextRunning->tid], mpqHeads[threads[nextRunning->tid]->priority], mpqTails[threads[nextRunning->tid]->priority]);
			freeRunning = nextRunning;
		}
		//if waiting, it got put onto the wait queue, so free this node.
		else if(threads[nextRunning->tid]->threadState == WAITING){
//			printf("Waiting\n");
			freeRunning = nextRunning;
		}//if not premempted check for yield, will not lower priority
		//otherwise it finished on it's own, but never called exit.
		else{
//			printf("Done but not exited.\n");//debugging statement
			threads[nextRunning->tid]->threadState = DONE;
			//need to modify global variable, so do it in mutex.
			my_pthread_mutex_lock(mutexManualExit);
			manualExit = nextRunning;
			exit_thread(manualExit, NULL);   //TODO: what if someone calls exit while this is preempted?
			manualExit = NULL;
			my_pthread_mutex_unlock(mutexManualExit);
			freeRunning = nextRunning;
		}
		//move to next node.
		nextRunning = (queueNode*)nextRunning->next;
		if(freeRunning != NULL){
			mydeallocate(freeRunning,__FILE__,__LINE__,LIBREQ);	//frees unless it's waiting
			freeRunning = NULL;
		}
//		printf("restart loop\n"); //debugging statement.
	}
}

/*signal handler for timer*/
void time_handle(int signum){
//	printf("time_handler\n");
	//change status to PREEMPTED
	threads[currentRunning->tid]->threadState = PREEMPTED;
	//change priority +1 on tcb node(unless bottom level or holding mutex)
	if(threads[nextRunning->tid]->mutexWaiting != TRUE){
		if(threads[currentRunning->tid]->priority != (PRIORITY_LEVELS - 1)){ 
			threads[currentRunning->tid]->priority += 1;
		}
	}
	//swapcontext back to scheduler
	my_pthread_yield();	
}
//
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
	queueNode* newNode = (queueNode*)myallocate(sizeof(queueNode),__FILE__,__LINE__,LIBREQ);
	
	//prep new node 
	newNode->tid = thread->tid;
	newNode->next = NULL;
	newNode->ctr = 0; 
	
	//figure out where to add it.	
	if(*head == NULL){//list is empty
		*head = newNode;
		*tail = newNode;
	}
	else if((*head)->tid == (*tail)->tid){//list only has one element
		(*head)->next = newNode;
		*tail = newNode;
	}
	else{//list has more than 2 or more elements.
		(*tail)->next = newNode;
		*tail = newNode;
	}	
	//wherever we added it, increment the counter.
	levelCtrs[threads[newNode->tid]->priority] += 1;
	return;
}

//pthread_exit calls this as well as manual exit in runthreads
void exit_thread(queueNode* node, void* value_ptr){
//	printf("in exit method\n");
	//before exiting, check to see if anyone else joined.
	if(threads[node->tid]->waitingThreads != NULL){//there are waiting threads
		queueNode* currentNode = threads[node->tid]->waitingThreads;
		queueNode* tempNode = NULL;
		//go through each node and give back the value_ptr
		while(currentNode){
//			char* ptr = (char*)threads[currentNode->tid]->retval;
//			ptr = (char*)value_ptr;
			if(value_ptr != NULL && (threads[currentNode->tid]->retval) != NULL){
				*(threads[currentNode->tid]->retval) = (char*)value_ptr;//is this right?? test it.
			}	
			addMPQ(threads[currentNode->tid], mpqHeads[threads[currentNode->tid]->priority], mpqTails[threads[currentNode->tid]->priority]);		
			tempNode = currentNode;
			currentNode = currentNode->next;
			mydeallocate(tempNode,__FILE__,__LINE__,LIBREQ);
		}
	}
	//Pass ID into tailThread, set curr tail = to this number.	
	if(headThread == NULL || headThread->readyIndex == -1){//none in queue yet
		headThread->readyIndex = node->tid;	
		headThread->next = NULL;
		tailThread = headThread;
	}
	else if(headThread == tailThread){//only one in queue
		nextId* tmpThread = (nextId*)myallocate(sizeof(nextId),__FILE__,__LINE__,LIBREQ);
		tmpThread->readyIndex = node->tid;
		tmpThread->next = NULL;
		headThread->next = tmpThread;
		tailThread = tmpThread;
	}
	else{//two or more in queue
		nextId* tmpThread = (nextId*)myallocate(sizeof(nextId),__FILE__,__LINE__,LIBREQ);
		tmpThread->readyIndex = node->tid;
		tmpThread->next = NULL;
		tailThread->next = tmpThread;
		tailThread = tmpThread;
	}
	mydeallocate(threads[node->tid]->context.uc_stack.ss_sp,__FILE__,__LINE__,LIBREQ);
	threads[node->tid]->context.uc_stack.ss_sp = NULL;
	mydeallocate(threads[node->tid],__FILE__,__LINE__,LIBREQ);
	threads[node->tid] = NULL;
	return;
}

//when scheduler gets uninitialized, free this stuff.
void free_things(){
//	printf("free things\n");
	//dummy node for thread ID linked list.
	nextId* tempThread = NULL;
	while(headThread!= NULL){
		tempThread = headThread;
		headThread = headThread->next;
		mydeallocate(tempThread,__FILE__,__LINE__,LIBREQ);
	}	

	
	
	mydeallocate(sched_uctx.uc_stack.ss_sp,__FILE__,__LINE__,LIBREQ);
	sched_uctx.uc_stack.ss_sp = NULL;
	
}


/**************** Given Methods to Override ****************/

	/**************** Thread Methods ****************/
/* create a new thread */  /* DONE */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	/* Creates a pthread that executes function. Attributes are ignored, arg is not. */
//	printf("my_pthread_create()\n");
	//initialize to some nonzero value
	my_pthread_t ID = -1;
	
	//if headThread is not initialized, initialize it.
	if(!headThread){
		headThread = (nextId*)myallocate(sizeof(nextId),__FILE__,__LINE__,LIBREQ);
		headThread->next = NULL;
		headThread->readyIndex = -1;
	}
	
	//fill up array before moving to linked list of readyIndexes
	if (useThreadId == FALSE){
		ID = threadCtr;
		threadCtr++;
		//thread is full, switch bool to TRUE.
		if(threadCtr == MAX_THREADS){
			useThreadId = TRUE;	
		}
	}
	//array is filled, move over to linked list of readyIndexes
	else{
		if(headThread->readyIndex != -1){//there are indexes available
			ID = headThread->readyIndex;
			nextId* tempThread = headThread;
			if(headThread->next == NULL){ //this way we always have a dummy node
				headThread->readyIndex = -1;
			}
			else{//move forward to next
				headThread = (nextId*)headThread->next;
				mydeallocate(tempThread,__FILE__,__LINE__,LIBREQ); //frees the thread that was previously head.
			}
		}
		else{
			return -1; //couldn't create it, array is full.
		}
	}
	//create a new tcb
	tcb* newNode = (tcb*)myallocate(sizeof(tcb),__FILE__,__LINE__,LIBREQ);
	//assign thread ID
	newNode->tid = ID;
	//if the first thing in the thread array is null, then we have to capture main's context first and store it as a thread.
	if(!threads[0]){
		if(getcontext(&(main_uctx)) == -1){
			return -2;//context issue
		}
		main_uctx.uc_stack.ss_sp = (char*)myallocate(STACK_SIZE,__FILE__,__LINE__,LIBREQ);
		main_uctx.uc_stack.ss_size = STACK_SIZE;
		main_uctx.uc_link = &sched_uctx;
		tcb* newNode = (tcb*)myallocate(sizeof(tcb),__FILE__,__LINE__,LIBREQ);
		newNode->tid = 0;
		newNode->context = main_uctx;
		newNode->threadState = ACTIVE;
		newNode->priority = 0;
		newNode->retval = NULL;
		newNode->waitingThreads = NULL;
		//check to see if pages were allocated before thread was made
		if(firstPTE){
			newNode->pageTable = firstPTE;
		}
		else{
			newNode->pageTable = NULL;
		}
		threads[0] = newNode;
		addMPQ(threads[0], &level0Qhead, &level0Qtail);
	}
	if(getcontext(&(newNode->context)) == -1){
		return -2;//context issue	
	}
	
	//do context stuff
	ucontext_t uc = newNode->context;
	uc.uc_stack.ss_sp = (char*)myallocate(STACK_SIZE,__FILE__,__LINE__,LIBREQ);
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
	newNode->pageTable = NULL;
	threads[ID] = newNode;
	
	//"thread" is a pointer to a "buffer", store ID in that "buffer"
	*thread = ID;		
	
	//add new thread to mpq level 0
	addMPQ(threads[ID], &level0Qhead, &level0Qtail);
	
	//Initialize the scheduler if it's not already initialized
	if(!schedInit){
		if(schedulerInit() != 0){
			return -3;
		}
	}
	return 0;
}	

/* give CPU possession  to other user level threads voluntarily */
int my_pthread_yield() {
	/* Explicit call to the my_pthread_t scheduler requesting that the current context can be swapped out 
	and another can be scheduled if one is waiting. */
//	printf("In my_pthread_yield()\n");//debugging statement
	
	//in case someone calls this before calling create.
	if(nextRunning == NULL){
		return -1;  
	}	

	//if currentRunning is null, then this is being called by the scheduler in order to swap into next thread on running queue
	if(currentRunning == NULL){
		currentRunning = nextRunning;
		timer(threads[currentRunning->tid]->priority);
		if((swapcontext(&(sched_uctx),&((threads[currentRunning->tid])->context))) != 0){
			return -2;
		}
	}
	//otherwise the currently running thread requested to yield, was preempted, or was put onto a waiting queue.
	else{
		//if not preempted or put onto a waiting queue, change status to YIELDED.
		if(threads[currentRunning->tid]->threadState != PREEMPTED && threads[currentRunning->tid]->threadState != WAITING){
			threads[currentRunning->tid]->threadState = YIELDED;		
		}
		//swap back to scheduler.
		if((swapcontext(&((threads[currentRunning->tid])->context),&(sched_uctx))) != 0){
			return -2;
		}
	}
	return 0;
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/* Explicit call to the my_pthread_t library to end the pthread that called it. If the value_ptr isn't 
	NULL, any return value from the thread will be saved. */
//	printf("In my_pthread_exit()\n");
	
	//in case someone calls this before calling create.
	if(currentRunning == NULL){
		return;
	}
	
	exit_thread(currentRunning, value_ptr);	
}

/* wait for thread termination */ 
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	/* Call to the my_pthread_t library ensuring that the calling thread will not continue execution until the one 
	it references exits. If value_ptr is not null, the return value of the exiting thread will be passed back.*/
//	printf("in my_pthread_join()\n");

	//in case someone calls this before calling create.	
	if(currentRunning == NULL){
		return -1;
	}
	//check for valid thread.
	if(threads[thread] == NULL){
		return -2;
	}

	//put thread onto waiting queue in tcb for thread passed in.
	queueNode* newNode = (queueNode*)myallocate(sizeof(queueNode),__FILE__,__LINE__,LIBREQ);
	newNode->tid = currentRunning->tid;
	newNode->next = NULL;

	//set calling thread's "retval" to the address where it wants the value stored.
	threads[currentRunning->tid]->retval = (char**)value_ptr; //is this right? test it.

	//check to see if there's already a waiting thread.	
	if(threads[thread]->waitingThreads == NULL){ //if i'm the first
		threads[thread]->waitingThreads = newNode;
	}
	else{ //if others are already waiting.
		queueNode* currentNode = threads[thread]->waitingThreads;
		while(currentNode->next){
			currentNode = currentNode->next;
		}
		currentNode->next = newNode;
	}
	//move state to WAITING and yield
	threads[currentRunning->tid]->threadState = WAITING;
	my_pthread_yield();

	return 0;
}

	/**************** Mutex Methods ****************/
/* initial the mutex lock */
//returns 0 upon succes or errno value when there is an error
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	/* Initializes a my_pthread_mutex_t created by the calling thread. Attributes are ignored. */
//	printf("my_pthread_mutex_init()\n");
	if(mutex == NULL){
		return EINVAL; //errno for invalid argument
	}
	//create new my_pthread_mutex_t struct.
	mutex->lockState = FALSE;
	mutex->owner = NULL;
	mutex->waitQueue = (basicQueue*)myallocate(sizeof(basicQueue),__FILE__,__LINE__,LIBREQ);

	//init the wait queue
	mutex->waitQueue->head = NULL;
	mutex->waitQueue->tail = NULL;
	mutex->waitQueue->queueSize = 0;	
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
			waitQueueNode *newHead = (waitQueueNode*)myallocate(sizeof(waitQueueNode),__FILE__,__LINE__,LIBREQ);
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
				waitQueueNode *firstNode = (waitQueueNode*)myallocate(sizeof(waitQueueNode),__FILE__,__LINE__,LIBREQ);
				firstNode->thread = threads[currentRunning->tid];
				firstNode->next = NULL;
			
				//set to head and tail			
				mutex->waitQueue->head = firstNode;
				mutex->waitQueue->tail = firstNode;

			}else{

				waitQueueNode *newTail = (waitQueueNode*)myallocate(sizeof(waitQueueNode),__FILE__,__LINE__,LIBREQ);
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
	}else{
		//dequeue node from wait queue

		//grab the nextThread waiting in the mutex's queue
		tcb *nextThread = mutex->waitQueue->head->thread;

		//remove first node in waitQueue
		waitQueueNode *temp = mutex->waitQueue->head->next; //might have a problem here if its null but probably not		
		mydeallocate(mutex->waitQueue->head,__FILE__,__LINE__,LIBREQ);
		mutex->waitQueue->head = temp;
		
		mutex->waitQueue->queueSize--;

		//change status back to active (from waiting)
		nextThread->threadState = ACTIVE;
		
		//set owner
		mutex->owner = nextThread;
		
		//pass the dequeued thread to the scheduler
		addMPQ(nextThread, mpqHeads[nextThread->priority], mpqTails[nextThread->priority]);
		
	}
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

	mydeallocate(mutex->waitQueue,__FILE__,__LINE__,LIBREQ);
	return 0;
}
