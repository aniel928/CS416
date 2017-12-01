
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
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
tcb* tempTCB = NULL;
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
my_pthread_mutex_t mutexManualExit;

//declare 8MB chunk of data for malloc
static char* memory = NULL;

//ptrs for OS stuff
metaData* currMD = NULL;
metaData* prevMD = NULL;
metaData* headOS = NULL;
metaData* iterMD = NULL;
metaData* tempMD = NULL;

//ptrs for USER stuff
memStruct * memHead = NULL;
memStruct * memNew = NULL;
memStruct * memFollow = NULL;
memStruct * memNext = NULL;

//ptrs for SHALLOC stuff
metaData* shcurrMD = NULL;
metaData* shprevMD = NULL;
metaData* headSh = NULL;
metaData* shiterMD = NULL;

//external page table and initialization
int EPT[(MEMORYSIZE - OSSIZE -(4*4096))/ 4096][2];//need to somehow malloc size
bool eptSet = FALSE;
//int OSbytes = 0;

//boolean for swap file, used vs unused
bool swapIndex[SWAPFILESIZE/4096] = {FALSE};
bool memoryInit = FALSE;
bool headInit = FALSE;
bool shallocInit = FALSE;

//save in case main method calls malloc before making new thread.
PTE* firstPTE = NULL; 

//global file descriptor
int fd = -1;

my_pthread_mutex_t mutexMalloc;//so that two threads don't grab same page
bool mallocInitialized = FALSE;
//boolean for malloc init
/***************** Malloc Stuff *********************/

//take number of consecutive pages needed, returns start index from EPT to be used
int findConsecutivePages(int numPages){
//	printf("find consecutive pages\n");
	int i=0;
	int j=0;
	//go through EPT
	while(i < NUMOFPAGES){
		//if it's less than 0, we can take it (-1 means never been used, -2 means used previously, so potentially something in swap file
		if(EPT[i][0] < 0){
			if(EPT[i][0] == -2){
				mprotect((void*)((long)memory + OSSIZE + PAGESIZE*j), PAGESIZE, PROT_READ | PROT_WRITE);
			}
			j++;
			if(j == numPages){
				break;
			}
		}
		//otherwise, reset counter
		else{
			j = 0;
		}
		i++;
	}
	//this means none were available
	if(j==0 || j < numPages){
		return -1;
	}
	return(i - (j - 1));
}

//returns first free index in swap file
int findSwapIndex(){
//	printf("find swap index\n");
	//go through entire array and find first available index
	int i = 0;
	while(i < sizeof(swapIndex)/sizeof(int)){
		if(swapIndex[i] == FALSE){
			//set to TRUE b/c we're about to use it
			swapIndex[i] = TRUE;
			break;
		}
		i++;
	}
	if(i==sizeof(swapIndex)/sizeof(int)){
		//this means none of them were available
		return -1;
	}
	return i;
}

//almost identical to findConsecutivePages() but with slightly different logic
//take number of consecutive pages needed, returns index from EPT that can be evicted
int findEvictIndex(int numPages){
//	printf("find evict index\n");
	int i = 0;
	int j = 0;
	int tid = 0;
	if(currentRunning){
		tid = currentRunning->tid;
	}
//	printf("%d\n", tid);
//	printf("tid %d, numPages %d\n", tid, numPages);
	//go through entire array
	while(i < NUMOFPAGES){
//		printf("EPT[%d][0] = %d\n", i, EPT[i][0]);
		if(EPT[i][0] != tid){
			j++;
//			printf("i: %d, j: %d\n",i, j);
//			printf("inside: %d\n", j);
			//if we found them all consecutively, break
			if(j == numPages){
				return (i - (j-1)); //returns first page
			}
		}
		else{
			//otherwise, once we find one that belongs to self, need to reset counter
//			printf("%d\n", i);
			j = 0;
		}
		i++;
	}
	//this happens if it gets to end of array without finding room
	return -1;
	
}

//takes index and number of pages and evicts them all - returns offset in file on success, -1 on failure
int evictPage(int page){
//	printf("evictPage\n");
	int swapInd = findSwapIndex();
	if(swapInd == -1){
		return -1;
	}
	else{
		mprotect((void*)((long)memory + OSSIZE + PAGESIZE*page), PAGESIZE, PROT_READ | PROT_WRITE); 
	}
	//write current page to memory
	char buffer[4096];
	memcpy(buffer, (void*)((long)memory + OSSIZE + PAGESIZE*page), PAGESIZE);
	int status = 0;
	//move to that index in file
	lseek(fd, (swapInd * PAGESIZE), SEEK_SET);
	while(status != PAGESIZE){
		status += write(fd, (void*)buffer + status, PAGESIZE - status);
	}
	return swapInd;
}

//takes page to be swapped and offset in file for the one we're looking for.  Returns offset on success, -1 on failure
int restorePage(int page, int offset){
//	printf("restore page\n");
	//first evict
	int status = evictPage(page);
	if(status == -1){
		printf("file full\n");
		return -1;
	}
	//then read into memory
	lseek(fd, offset * PAGESIZE, SEEK_SET);
	char buffer[4096];
	int bytesread = 0;
//	while(bytesread < PAGESIZE){
		read(fd, (void*)buffer + bytesread, PAGESIZE - bytesread); 
//	}
	memcpy((void*)((long)memory + OSSIZE + PAGESIZE*page), buffer, PAGESIZE);
	swapIndex[page] = FALSE;
//	printf("return restore page\n");
	return status;
}

//for when file is full but need to bring something else back in.
//something is definitely broken here
int evictPageIntoBuffer(int page, int offset){
//	printf("evict page into buffer\n");
	mprotect((void*)((long)memory + OSSIZE + PAGESIZE*page), PAGESIZE, PROT_READ | PROT_WRITE); 
	//buffers to save from file temporarily
	char buffer[4096]; //store from file
	char buffer2[4096]; //write to file
	
	//lseek into file and read back into buffer
	lseek(fd, offset * PAGESIZE, SEEK_SET);
	read(fd, (void*)buffer, PAGESIZE);
	
	//now copy out of array into buffer, lseek back and write to file
	memcpy(buffer2, (void*)((long)memory + OSSIZE + PAGESIZE*page), PAGESIZE);
	lseek(fd, offset * PAGESIZE, SEEK_SET);
	int status = 0;
	while(status != PAGESIZE){
		status += write(fd, (void*)buffer2 + status, PAGESIZE - status);
	}
	
	//now memcpy buffer into memory
	memcpy((void*)((long)memory + OSSIZE + PAGESIZE*page), buffer, PAGESIZE);
	return offset;
}

//handle segmentation faults
void segment_fault_handler(int signum, siginfo_t *si, void* unused){
//	printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
//	printf("memory: %p\tUSER: %p\tend: %p\n", memory, (void*)((long)memory + OSSIZE), (void*)((long)memory + OSSIZE + USERSIZE));
	
	//if address is in our array, deal with swap file

	if ((long)si->si_addr >= (long)memory + OSSIZE && (long)si->si_addr < (long)memory + OSSIZE + USERSIZE){
//		printf("segfaulting inside\n");
		//calculate page
		long page = ((long)si->si_addr - (long)memory - OSSIZE)/PAGESIZE;
		//allow read/write access to page (since we're about to switch it in anyway)				
		mprotect((void*)((long)memory + OSSIZE + (page * PAGESIZE)), PAGESIZE, PROT_READ | PROT_WRITE);
		int offset = -1;
		//grab current owner so we can set their file offset later.
		int owner = EPT[page][0];
		//circle through pageTable and find current page for offest
		PTE* pageTable = firstPTE;
		if(currentRunning){
			pageTable = threads[currentRunning->tid]->pageTable;
		}
		while(pageTable){
			if(pageTable->pageIndex == page){
				break;
			}
			pageTable = pageTable->next;					
		}
		if(pageTable){
			offset = pageTable->offset;
		}
		//if we didn't find it, this is a real segfault because they don't own that page
		if(offset == -1){
			printf("real internal segmentation fault\n");
			exit(EXIT_FAILURE);
		}
		//otherwise restore the page (this function also calls evict for us on the current one.
		int newOffset = restorePage(page, offset);
		//if restorePage fails
		if(newOffset == -1){
			//this means file is full
			newOffset = evictPageIntoBuffer(page, offset);
			printf("newOffset: %d\n", newOffset);
		}
		//now update EPT, currentThreads offset and old thread's offset.
		if(currentRunning){
			EPT[page][0] = currentRunning->tid;
		}
		else{
			EPT[page][0] = 0;
		}
		pageTable->offset = -1;
		if(threads[owner]){
			pageTable = threads[owner]->pageTable; ///TODO:  THIS IS THE SEGFAULTING LINE
			while(pageTable){
				if(pageTable->pageIndex == page){
					pageTable->offset = newOffset;
					break;
				}
				pageTable = pageTable->next;
			}
		}
		return;
	//if address is not in our array, then it's a real segmentation fault
	}
	else if((long)si->si_addr >= (long)memory && (long)si->si_addr < (long)memory + OSSIZE){
		printf("segfault in OS space\n");
	}
	else{
		printf("real external segmentation fault\n");
		exit(EXIT_FAILURE);
	}
}

//If first malloc call, initialize/align memory and set up some important stuff
void mallocInit(){
//	printf("Aligning...\n");
	//aligning pages
	if(!memoryInit){
		posix_memalign((void*)&memory,PAGESIZE, MEMORYSIZE);
		memoryInit = TRUE;
		
	}
	int i =0;
	//creating swapfile
	fd = open("swapfile", O_CREAT | O_RDWR| O_TRUNC, 0666);
	lseek(fd, 16*1024*1024, SEEK_CUR);
	write(fd, "", 1);
	
	//catching segfaults	
	struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segment_fault_handler;

    if(sigaction(SIGSEGV, &sa, NULL) == -1){
    	printf("Fatal error setting up signal handler\n");
		exit(EXIT_FAILURE);    //explode!
	}
	//initialization of EPT, setting all buckets to -1
	if (eptSet != TRUE){
		int i = 0;
			while (i < NUMOFPAGES){//EPT is now a multi dimensional array so size is different
			EPT[i][0] = -1;
			i++;
		}
		eptSet = TRUE;				
	}
	//set variable to TRUE so we don't get back into this function again
	mallocInitialized = TRUE;
	return;
}

//replace malloc
void* myallocate(int size, char* file, int line, int threadId){
//	printf("in myallocate\n");
	if(!mallocInitialized){
		mallocInit();
		my_pthread_mutex_init(&mutexMalloc, NULL);
		if(!threads[0]){
			tempTCB = (tcb*)myallocate(sizeof(tcb),__FILE__,__LINE__,LIBREQ);
			tempTCB->tid = 0;
		}

	}
	
	//error catching in case trying to malloc 0 bytes.
	if(size <= 0){
		return NULL;
	}

	//library request - don't use paging.
	if(!threadId){
		//if not initialized yet, set first metadata
		if(!headInit){
			headOS = (metaData*)memory;
			headOS->used = 0;
			headOS->bytes = OSSIZE - sizeof(metaData*);
			headInit = TRUE;
		}
		//now that it's initialized, start at headstart at head and keep going until space fits
		currMD = headOS;
		while((long)currMD - (long)memory < OSSIZE){
			if(!currMD->used && currMD->bytes >= size){
				break;
			}
			else{	
				currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
			}
			if((long)currMD >= (long)memory + OSSIZE){
				return NULL;	
			}
		}
		//if there's not enough room for metadata
		if(currMD->bytes < (sizeof(metaData*) + size)){
			currMD->used = 1;		
		}
		//otherwise, put metdata and let that point to new size
		else{
			currMD->used = 1;
			metaData* tempMD = (metaData*)((long)currMD + sizeof(metaData) + size); 
			tempMD->used = 0;

			tempMD->bytes = currMD->bytes - size - sizeof(metaData);
			currMD->bytes = size;
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
		my_pthread_mutex_lock(&mutexMalloc);
		//if smaller than a page is requested
		if(size <= (PAGESIZE - sizeof(metaData))){
			//check to see if pageTable exists for thread, if it doesn't, add it
			PTE* table = NULL;
			if(tid == 0 && !threads[0]){
				table = firstPTE;	//called by  main method before pthread_create
			}
			else{
				table = threads[tid]->pageTable;
			}		
			//find an entry that has enough room for small malloc
			while(table){
				if(table->maxSize >= (size + sizeof(memStruct))){
					break;
				} 
				table = table->next;
			}
			
			//as long as it's not NULL (end of list) that means that we found an entry that could fit request
			if(table){
				//grab the index and get to that page table in memory
				int index = table->pageIndex;
				memStruct* prevMem = NULL;
				memStruct* newMem = (memStruct*)((long)memory + OSSIZE + (index * PAGESIZE));

				//move forward until we find a place that fits request
				while(newMem->inUse != 0 || newMem->currUsed < size){
					//if it's NULL, we've reached the end and need to add to end of cell
					if(!newMem->next){
						prevMem = newMem;
						newMem = (memStruct*)((long)newMem + newMem->currUsed + sizeof(memStruct));
						table->maxSize -= (size + sizeof(memStruct));
						break;
					}
					
					prevMem = newMem;
					newMem = (memStruct*)((long)newMem + newMem->currUsed + sizeof(memStruct));
					//index++;
					if(table->maxSize == newMem->currUsed){
						table->maxSize -= (size - sizeof(memStruct)); //if the current size equals what was in the file, reduce max size
					}
				}

				//set metadata for new malloc
				newMem->inUse = 1;
				newMem->pageCount = 0;
				if(prevMem != NULL){
					newMem->next = prevMem->next;
					prevMem->next = newMem;
				}
				else{
					newMem->next = NULL;
				}
				
				//set metadata if there is enough room and left and it's between two metadatas, set a new metadata for unused
				if(newMem->currUsed > (size + sizeof(memStruct))){
					int newSize = newMem->currUsed - size - sizeof(memStruct);
					newMem->currUsed = size;
	
					memStruct* nextMem = (memStruct*)((long)newMem + sizeof(memStruct));
					nextMem->inUse = 0;
					nextMem->currUsed = newSize;
				}
				EPT[index][0] = tid;
				EPT[index][1]++;
				my_pthread_mutex_unlock(&mutexMalloc);
				return (void*)((long)newMem + sizeof(memStruct));
			}
			//if it didn't go into if (and therefore didn't return), it needs a page allocated, so continue on
		}
		//determining how many pages are needed to return
		int pageCount = ((size + sizeof(memStruct))/PAGESIZE) + 1;//ANNE: I may change this later because this adds one if its like 1.4 or 0.5 cause it's an int, so it'd be 1 or 0 respectively, but what if it's exactly 1.0 or 2.0, I think there's code that makes this irrelevant either way but I just wanted to point it out

		int firstPage = findConsecutivePages(pageCount);
		// if it failed, we need to evict, by end  of this if firstPage will be the firstPage of where we're assigning the new value
		if(firstPage == -1){
			firstPage = findEvictIndex(pageCount);
			int status = -1;
			//if it still failed, we can't do anything, return NULL pointer
 			if(firstPage == -1){
 				my_pthread_mutex_unlock(&mutexMalloc);
 				return NULL;
 			}
 			else{
				//otherwise, start evicting.
 				int i = 0;
 				int j = firstPage;
 				while(i < pageCount){
 					status = evictPage(j);
					//if for some reason we run out of room, return NULL;
 					if(status == -1){
 						printf("File really full\n");
 						my_pthread_mutex_unlock(&mutexMalloc);
 						return NULL;
 					}
 					else{
 						//otherwise the space is ours.
  						PTE* pageTable = threads[EPT[j][0]]->pageTable;
 						while(pageTable){
 							if(pageTable->pageIndex == j){
 								pageTable->offset = status; //stores offset in pageTable
 								break;
 							}
 							pageTable = pageTable->next;
 						}
						EPT[j][0] = -2;	
						EPT[j][1] = 0;
 					}
 					j++;
 					i++;
 				}
 			}	
		}	

		//set memstruct stuff		
		memNew = (memStruct*)((long)memory + OSSIZE +(firstPage * PAGESIZE )); //This line is the cause of about an hour worth of segfaulting		
		memNew->pageCount = pageCount;
		memNew->currUsed = size + sizeof(memStruct); //MIKE: why plus sizeof(memStruct)?  What are you doing with it? ANNE, It's just saying how many bytes that this specific malloc has used in the page, if you're not using it in other places I think it'd be best to just leave it, I see what you're saying but I believe I have some logic that uses it and it's working, let me know if you're using it and boy this is long, huh?
		memNew->inUse = TRUE;
		memNew->next = NULL; //i think only important for internal mallocs

		//will only happen on first call
		if (memHead == NULL){
			memHead = memNew;
		}
		
		//external page table stuff
		int i = firstPage;
		while (i < (long)(pageCount + firstPage)){
			EPT[i][0] = tid;
			EPT[i][1]++;
			i++;					
		}
		
		//internal page table stuff
		PTE * threadPTE = (PTE*)myallocate(sizeof(PTE), __FILE__, __LINE__, LIBREQ);
		if (threadPTE == NULL){
			my_pthread_mutex_unlock(&mutexMalloc);
			return NULL;
		}
		//check if first index of pageTable has been created
		if(tid == 0 && !threads[0]){
			if(!firstPTE){
				firstPTE = threadPTE;
			}
			else{
				PTE * tempPTE = firstPTE;
				while (tempPTE->next != NULL){
					tempPTE = tempPTE->next;	
				}
				tempPTE->next = threadPTE;
			}
		}
		else{
			if (threads[tid]->pageTable == 	NULL){
				threads[tid]->pageTable = threadPTE;
			}
			else{//has been previously created, get to end of current table
				PTE * tempPTE = threads[tid]->pageTable;
				while (tempPTE->next != NULL){
					tempPTE = tempPTE->next;	
				}
				tempPTE->next = threadPTE;
			}
		}
		//add page indexes to pageTable for first thread
		int iter = firstPage;	
		while ( iter < (firstPage + pageCount)-1){//while loop for all but last entry
			threadPTE->pageIndex = (iter);
			threadPTE->maxSize = 0;
			threadPTE->offset = -1;
			iter++;
			threadPTE->next = (PTE*)myallocate(sizeof(PTE), __FILE__, __LINE__, LIBREQ);
			if (threadPTE == NULL){
				my_pthread_mutex_unlock(&mutexMalloc);
				return NULL;
			}
			threadPTE = threadPTE->next;
		}
		//last entry in pageTable
		threadPTE->pageIndex = (iter);
		threadPTE->maxSize = PAGESIZE - (size - (PAGESIZE * (pageCount - 1) - sizeof(memStruct)));	//working
		threadPTE->offset = -1;
		threadPTE->next = NULL;
		//returning the front of the string
		void * retStr = (void*)((long)memNew + sizeof(memStruct));
		my_pthread_mutex_unlock(&mutexMalloc);
		return retStr;
	}
	return NULL;
}

//replace free
void mydeallocate(void* ptr, char* file, int line, int threadId){
	if(!ptr){//|| ((metaData*)((long)ptr - sizeof(metaData)))->used == 0){
		return;
	}
	//coming from library
	if(!threadId){
		((metaData*)((long)ptr - sizeof(metaData)))->used = 0;
	}
	//coming from user space
	else{
		int tid = -1;
		if(currentRunning){
			tid = currentRunning->tid;
		}
		else{
			tid = 0;
		}	
		//logic for shalloc
		if(((long)ptr >= (long)memory + OSSIZE + USERSIZE) && ((long)ptr < (long)memory + OSSIZE + USERSIZE + SHAREDSIZE)){
			((metaData*)((long)ptr - sizeof(metaData)))->used = 0;	
			return;
		}
		//userspace free
		if (((memStruct*)((long)ptr - sizeof(memStruct)))->inUse == TRUE ){
			//find out how many pages are being freed so we can deal with page tables
			int pageCount = ((memStruct*)((long)ptr - sizeof(memStruct)))->pageCount; //number of pages
			int indexStart = (((long)ptr - sizeof(memStruct)) - ((long)memory + OSSIZE))/ PAGESIZE;//first page
			int lastPage = indexStart + pageCount - 1; //last page
			//first set to firstPTE, then reset to proper table if thread running
			PTE * freePTE = firstPTE;
			PTE * previous = NULL; //to set pointer if necessary
			PTE * temp = NULL; //to free it
			if(currentRunning){
				freePTE = threads[tid]->pageTable;
			}
			((memStruct*)((long)ptr - sizeof(memStruct)))->inUse = FALSE;		
		 	//Resetting page table index to -1			
			int i = indexStart;
			bool checked = FALSE;
			while (freePTE && i < (long)(pageCount + indexStart)){ //while loop to get to right page index
				if (checked == FALSE){
					while(freePTE && indexStart != freePTE->pageIndex){
						previous = freePTE;
						freePTE = freePTE->next;
					}
					checked = TRUE;
				}
				if(freePTE){
					freePTE->pageIndex = -1;
					if(freePTE->maxSize <= ((memStruct*)((long)ptr - sizeof(memStruct)))->currUsed){
						freePTE->maxSize == ((memStruct*)((long)ptr - sizeof(memStruct)))->currUsed;
					}
					freePTE->offset = -1;
					temp = freePTE;
					freePTE = freePTE->next;
					temp->next = NULL;
					mydeallocate(temp, __FILE__, __LINE__, LIBREQ);
				}
				if (EPT[i][1] > 1){
					EPT[i][1]--;
					
				}
				else{
					EPT[i][0] = -2; //this way we keep it mprotected so that if returning thread wants it, it segfaults and brings it in.
					EPT[i][1] = 0;
				}
				i++;	
				
			}
			if(previous){
				previous->next = freePTE;
			}
		}
	}
	return;
}

//shared malloc
void* shalloc(int size){
	if(!mallocInitialized){
		mallocInit();
		my_pthread_mutex_init(&mutexMalloc, NULL);
		if(!threads[0]){
			tempTCB = (tcb*)myallocate(sizeof(tcb),__FILE__,__LINE__,LIBREQ);
			tempTCB->tid = 0;
		}
	}
	//if not initialized yet, set first metadata
	if(!shallocInit){
		headSh = (metaData*)((long)memory + OSSIZE + USERSIZE);
		headSh->used = 0;
		headSh->bytes = SHAREDSIZE - sizeof(metaData*);
		shallocInit = TRUE;

	}
	//now that it's initialized, start at headstart at head and keep going until space fits
	shcurrMD = headSh;
	while((long)shcurrMD - ((long)memory+ OSSIZE + USERSIZE) < SHAREDSIZE){
		if(!shcurrMD->used && shcurrMD->bytes >= size){
			break;
		}
		else{	
			shcurrMD = (metaData*)((long)shcurrMD + sizeof(metaData) + shcurrMD->bytes);
		}
		if((long)shcurrMD >= (long)memory + OSSIZE + USERSIZE + SHAREDSIZE){
			return NULL;	
		}
	}
	//if there's not enough room for metadata
	if(shcurrMD->bytes < (sizeof(metaData*) + size)){
		shcurrMD->used = 1;		
	}
	//otherwise, put metdata and let that point to new size
	else{
		metaData* shtempMD = (metaData*)((long)shcurrMD + sizeof(metaData) + size); 
		shtempMD->used = 0;

		shtempMD->bytes = shcurrMD->bytes - size - sizeof(metaData);
		shcurrMD->used = 1;
		shcurrMD->bytes = size;
	}
	return (void*)((long)shcurrMD + sizeof(metaData));

}

/**************** Additional Methods ****************/

//initialize the scheduler
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
	my_pthread_mutex_init(&mutexManualExit, NULL);

	
	while(schedInit){
		maintenanceCycle();
		if(schedInit){//make sure it wasn't uninitialized in maintenance loop.
			runThreads();
		}
	} 	
	
	//now that we're out of the scheduler, destroy the mutex.
	my_pthread_mutex_destroy(&mutexManualExit);
	free_things();

}

//do some maintenance between running cycles
void maintenanceCycle(){	
//	printf("maintenanceCycle()\n");
	//this needs to change if more than 4 priority levels
	if(levelCtrs[0] + levelCtrs[1] + levelCtrs[2] + levelCtrs[3] == 0){
//		printf("if\n");
		//once there are no more threads left, stop running scheduler
		schedInit = FALSE;	
	}
	//otherwise, do some maintenance
	else{
//		printf("else\n");
		queueNode* maintRunning = NULL;	//iterate through MPQ
		queueNode* freeRunning = NULL;	//store node to free.
		queueNode* prevMaint = NULL;
		int i = 0;
		//go through each priority level
//		printf("before while\n");
		while(i < PRIORITY_LEVELS){	
//			printf("i: %d\n", i);
			maintRunning = (*(mpqHeads[i]));
//			printf("%p\n", maintRunning);
			//promote priority of those threads that have been waiting too long (but are not already level 0
			while(maintRunning){
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
//					printf("add\n");
					addMPQ(threads[maintRunning->tid], mpqHeads[threads[maintRunning->tid]->priority], mpqTails[threads[maintRunning->tid]->priority]);
				}
				//move forward and free current if necessary.
				maintRunning = maintRunning->next;
//				printf("%p\n", maintRunning);
				if(freeRunning != NULL){
					mydeallocate(freeRunning,__FILE__,__LINE__,LIBREQ);
					freeRunning = NULL;
				}
			}
//			printf("out of inner while\n");
			i++;
		}
//		printf("after while\n");
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
			my_pthread_mutex_lock(&mutexManualExit);
			manualExit = nextRunning;
			exit_thread(manualExit, NULL);   
			manualExit = NULL;
			my_pthread_mutex_unlock(&mutexManualExit);
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
	//unmprotect everything
//	printf("memprotect\n");
	mprotect((void*)((long)memory + OSSIZE), USERSIZE, PROT_READ | PROT_WRITE); //allow read and write to entire array
//	printf("threads current running preempted\n");
	threads[currentRunning->tid]->threadState = PREEMPTED;
	//change priority +1 on tcb node(unless bottom level or holding mutex)
//	printf("mute waiting\n");
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
//	printf("%p\n", newNode);
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
//TODO: free pageTable, reset EPT[] and swapIndex[]
//	printf("in exit method\n");
	//before exiting, check to see if anyone else joined.
	if(threads[node->tid]->waitingThreads != NULL){//there are waiting threads
		queueNode* currentNode = threads[node->tid]->waitingThreads;
		queueNode* tempNode = NULL;
		//go through each node and give back the value_ptr
		while(currentNode){
			if(value_ptr != NULL && (threads[currentNode->tid]->retval) != NULL){
				*(threads[currentNode->tid]->retval) = (char*)value_ptr;//is this right?? test it.
			}	
			addMPQ(threads[currentNode->tid], mpqHeads[threads[currentNode->tid]->priority], mpqTails[threads[currentNode->tid]->priority]);		
			tempNode = currentNode;
			currentNode = currentNode->next;
			mydeallocate(tempNode,__FILE__,__LINE__,LIBREQ);
		}
	}
	if(threads[node->tid]->pageTable){
		PTE* table = threads[node->tid]->pageTable;
		PTE* free = NULL;
		while(table){
			int page = table->pageIndex;
			int offset = table->offset;
			EPT[page][0] = -2;
			EPT[page][1] = 0;
			swapIndex[offset] = -1;
			free = table;
			table = table->next;
			mydeallocate(free, __FILE__, __LINE__, LIBREQ);
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
	
/* create a new thread */
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
//	printf("hi\n");
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
		tcb* newNode = tempTCB;
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
//		printf("tid: %d\n", currentRunning->tid);
		//mprotect only pages that do not == -1 and do not == tid, let -2 be mprotected so it can segfault
		int i = 0;
		while(i < NUMOFPAGES){
			if(EPT[i][0] > 0 && EPT[i][0] != currentRunning->tid){
//				printf("i: %d, fullsize: %d\n", i, (NUMOFPAGES));
				mprotect((void*)((long)memory + OSSIZE + PAGESIZE * i), PAGESIZE, PROT_NONE);
			}
			i++;
		}
		timer(threads[currentRunning->tid]->priority);
		if((swapcontext(&(sched_uctx),&((threads[currentRunning->tid])->context))) != 0){
			return -2;
		}
//		printf("yup\n");
		mprotect((void*)((long)memory + OSSIZE), USERSIZE, PROT_READ | PROT_WRITE); //allow read and write to entire array
	}
	//otherwise the currently running thread requested to yield, was preempted, or was put onto a waiting queue.
	else{
		//if not preempted or put onto a waiting queue, change status to YIELDED.
		mprotect((void*)((long)memory + OSSIZE), USERSIZE, PROT_READ | PROT_WRITE); //allow read and write to entire array
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
//		printf("ok\n");
		return EINVAL; //errno for invalid argument
	}
	//create new my_pthread_mutex_t struct.
	mutex->lockState = FALSE;
	mutex->owner = NULL;
	mutex->waitQueue = (basicQueue*)myallocate(sizeof(basicQueue),__FILE__,__LINE__,LIBREQ);
//	printf("between %p\n", mutex->waitQueue);
	//init the wait queue
	mutex->waitQueue->head = NULL;
	mutex->waitQueue->tail = NULL;
	mutex->waitQueue->queueSize = 0;	
//	printf("after\n");
	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	/* Locks a given mutex, other threads attempting to access this mutex will not run until it is unlocked. */
//	printf("mutex_lock\n");
	
	if(mutex == NULL){
		return EINVAL;  
	}
	int tid = 0;
	if(currentRunning){
		tid = currentRunning->tid;
	}
	
	while (__sync_lock_test_and_set(&(mutex->lockState), TRUE)){
		//if owner enters here, pass it through
		if(mutex->owner->tid == tid){
			return 0;
		}
		//make the thread wait
		threads[tid]->threadState = WAITING;

		//dealing with priority inversion with priority inheritance
		if((threads[tid]->priority) > (mutex->owner->priority)){

			//put new thread at head of wait queue
			waitQueueNode *newHead = (waitQueueNode*)myallocate(sizeof(waitQueueNode),__FILE__,__LINE__,LIBREQ);
			newHead->thread = threads[tid];

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
				firstNode->thread = threads[tid];
				firstNode->next = NULL;
			
				//set to head and tail			
				mutex->waitQueue->head = firstNode;
				mutex->waitQueue->tail = firstNode;

			}else{

				waitQueueNode *newTail = (waitQueueNode*)myallocate(sizeof(waitQueueNode),__FILE__,__LINE__,LIBREQ);
				newTail->thread = threads[tid];
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
//	printf("return\n");	
	//fetch owner of the lock

	
	if(threads[tid]){
		mutex->owner = threads[tid];
	}
	else{
		mutex->owner = tempTCB;
	}
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