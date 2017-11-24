
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

//ptrs for SHALLOC stuff
metaData* shcurrMD = NULL;
metaData* shprevMD = NULL;
metaData* headSh = NULL;
metaData* shiterMD = NULL;

//external page table and initialization
int EPT[(MEMORYSIZE - OSSIZE -(4*4096))/ 4096][2];//need to somehow malloc size
bool eptSet = FALSE;

//boolean for swap file, used vs unused
bool swapIndex[SWAPFILESIZE/4096] = {FALSE};
bool memoryInit = FALSE;

//save in case main method calls malloc before making new thread.
PTE* firstPTE = NULL; 

//global file descriptor
int fd = -1;

my_pthread_mutex_t* mutexMalloc = NULL;//so that two threads don't grab same page
bool mallocInitialized = FALSE;
//boolean for malloc init
/***************** Malloc Stuff *********************/

//This function is purely for testing, leaving in now but not necessary when we submit
void dataEPT(){
	printf("Printing page table\n");
	int i = 0;
	int j = NUMOFPAGES;
	while (i<NUMOFPAGES){
		int currTid = EPT[i][0];
		if(EPT[i][0] >= 0 ){
				printf("Page: %d, TID: %d\n", i, EPT[i][0]);
			//	printf("Inner page table for tid %d\n", EPT[i]);
			//	showPages(EPT[i]);
				j--;
		}
		i++;
	}	

	printf("number of -1: %d\n",j);
	
}

//take number of consecutive pages needed, returns start index from EPT to be used
int findConsecutivePages(int numPages){
	int i=0;
	int j=0;
	//go through EPT
	while(i < NUMOFPAGES){
		//if it's less than 0, we can take it (-1 means never been used, -2 means used previously, so potentially something in swap file
		if(EPT[i][0] < 0){
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
	int i = 0;
	int j = 0;
	//go through entire array
	while(i < NUMOFPAGES){
		if(EPT[i][0] != currentRunning->tid){
			j++;
			//if we found them all consecutively, break
			if(j == numPages){
				break;
			}
		}
		else{
			//otherwise, once we find one that belongs to self, need to reset counter
			j = 0;
		}
		i++;
	}
	//this happens if it gets to end of array without finding room
	if(j==0 || j < numPages){
		return -1;
	}
	//i went too far ahead.  if i = 4 and numPages/j = 3, then indexes 2, 3, 4 are what can be used.
	return (i - (j-1));
}

//takes index and number of pages and evicts them all - returns offset in file on success, -1 on failure
int evictPage(int page){
//	printf("evictPage\n");
	int swapInd = findSwapIndex();
	if(swapInd == -1){
		printf("File is full.\n");
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
	//first evict
	int status = evictPage(page);
	if(status == -1){
		return -1;
	}
	//then read into memory
	lseek(fd, offset * PAGESIZE, SEEK_SET);
	char buffer[4096];
	read(fd, (void*)buffer + status, PAGESIZE - status); 
	memcpy((void*)((long)memory + OSSIZE + PAGESIZE*page), buffer, PAGESIZE);
	swapIndex[page] = FALSE;
	printf("return restore page\n");
	return status;
}

//for when file is full but need to bring something else back in.
//something is definitely broken here
int evictPageIntoBuffer(int page, int offset){
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
/*void segment_fault_handler(int signum, siginfo_t *si, void* unused){
	printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
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
		PTE* pageTable = threads[currentRunning->tid]->pageTable;
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
		//if restoring fails, shouldn't segfault, but hopefully this never gets called? TODO: figure out what to actually do here
		if(newOffset == -1){
			printf("print statement that I hope never happens... (in seg fault handler)\n");
			exit(EXIT_FAILURE);
		}
		//now update EPT, currentThreads offset and old thread's offset.
		EPT[page][0] = currentRunning->tid;
		//EPT[page][1]++; //incrementing count of threads using this page
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
	}else{
		printf("real external segmentation fault\n");
		exit(EXIT_FAILURE);
	}
}*/
void segment_fault_handler(int signum, siginfo_t *si, void* unused){
//	printf("Got SIGSEGV at address: 0x%lx\n",(long) si->si_addr);
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
		PTE* pageTable = threads[currentRunning->tid]->pageTable;
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
		//if restoring fails, shouldn't segfault, but hopefully this never gets called? TODO: figure out what to actually do here
		if(newOffset == -1){
			//this means file is full
//TODO!!
			//THIS IS THE PROBLEM
			//I THINK I'M SETTING THE OLD OFFSET TO -1 TOO SOON
//			exit(EXIT_FAILURE);			
//			printf("offset: %d\n", offset);
			newOffset = evictPageIntoBuffer(page, offset);
			printf("newOffset: %d\n", newOffset);
		}
		//now update EPT, currentThreads offset and old thread's offset.
		EPT[page][0] = currentRunning->tid;
		//EPT[page][1]++; //incrementing count of threads using this page
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
	}else{
		printf("real external segmentation fault\n");
		exit(EXIT_FAILURE);
	}
}
//If first malloc call, initialize/align memory and set up some important stuff
void mallocInit(){
//	printf("Aligning...\n");
	//initializing mutex to be used in malloc
	my_pthread_mutex_init(mutexMalloc, NULL);
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
	//		printf("in while\n");
			EPT[i][0] = -1;
			i++;
		}
		eptSet = TRUE;				
	}
	//set variable to TRUE so we don't get back into this function again
	mallocInitialized = TRUE;
	return;
}

//take previously freed space, give back what they're asking for and put metadata in for the rest
void spaceBetween(metaData* curr, int size, int bytesFree){
	curr->used = 1;   
	curr->bytes = size;
	curr->ptr = (metaData*)((long)curr + sizeof(metaData) + curr->bytes);
	metaData* iter = NULL;
	iter = curr->ptr;
	iter->used = 0;
	iter->bytes = bytesFree - size - sizeof(metaData); //bytes reamining are total counted above minus the size we allocated and the size of this metadata
	iter->ptr = (metaData*)((long)iter + sizeof(metaData) + iter->bytes);
}

//if previously freed, see if there's more stuff contiguously  
int combineFreeStuff(metaData* iter){
	int bytesFree = iter->bytes;
	while(iter->ptr && (iter->ptr)->used == 0){
		//if the next one, or two, or several are free, combine them.
		iter = iter->ptr;
		bytesFree += sizeof(metaData) + iter->bytes;
	}
	return bytesFree;
}

//replace malloc
void* myallocate(int size, char* file, int line, int threadId){
//	printf("in myallocate\n");
	if(!mallocInitialized){
		mallocInit();
	}

	//error catching in case trying to malloc 0 bytes.
	if(size <= 0){
		return NULL;
	}

	//library request - don't use paging.
	if(!threadId){
	//	TODO: (PUTTING THIS TODO IN TWO PLACES[OTHER IN FREE]) Add changes/fix from old file
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
			}
			
			//if it's at least as big as space, but less than enough space to put metadata
			if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
				currMD->used = 1;
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

						//if there are enough bytes free to squeeze it in, but not enough for metadata, give them all
						if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
							currMD->used = 1;	
							return (void*)((long)currMD + sizeof(metaData));
						}
						//if it's bigger than the space and there's enough room for metadata, put it in and put in metadata for rest
						else if(bytesFree > size){
							spaceBetween(currMD, size, bytesFree);
							return (void*)((long)currMD + sizeof(metaData));
						}
						//else if it won't fit, keep looping through.
					}
				}
				//do it all one more time forward, loop broke when PTR was null, so get to that free space
				prevMD = currMD;
				currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);
				
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
					}				
					if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
						currMD->used = 1;
						return (void*)((long)currMD + sizeof(metaData));
					}
					//if it's bigger than the space and there's enough room for metadata
					else if(bytesFree > size){
						spaceBetween(currMD, size, bytesFree);
						return (void*)((long)currMD + sizeof(metaData));
					}
					//else if it won't fit, keep looping through.
				}
			}
			prevMD = currMD;
			currMD = (metaData*)((long)currMD + sizeof(metaData) + currMD->bytes);

			//Please don't let this happen, 1MB should be plenty
			if(((long)currMD - (long)headOS + sizeof(metaData) + size) > (OSSIZE)){
				printf("Not enough space in OS section.\n");
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

		my_pthread_mutex_lock(mutexMalloc);
		
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
				my_pthread_mutex_unlock(mutexMalloc);
				//EPT[index][0] = tid;
				//EPT[index][1]++;
				//ANNE: at any point are you doing anything with the page table for this? or should it be implemented? if so I think it might just be what I implemented on lines 622 and the two lines above this
				return (void*)((long)newMem + sizeof(memStruct));
			}
			//if it didn't go into if (and therefore didn't return), it needs a page allocated, so continue on
		}

		//determining how many pages are needed to return

		/***MIKE: I think this would work for the note you made below.
		#include <math.h> //add to top though
		int pageCount = ceiling((double)(size + sizeof(memStruct))/(double)PAGESIZE);
		***/
		int pageCount = ((size + sizeof(memStruct))/PAGESIZE) + 1;//ANNE: I may change this later because this adds one if its like 1.4 or 0.5 cause it's an int, so it'd be 1 or 0 respectively, but what if it's exactly 1.0 or 2.0, I think there's code that makes this irrelevant either way but I just wanted to point it out

		int firstPage = findConsecutivePages(pageCount);
		printf("FIRST PAGE: %d\n", firstPage);
//		printf("memory: %p\n", memory);
//		if(firstPage == -1){ //this is exactly where it should be evicting.
//			printf("Today is not your day, hang in there champ\n");
//			return NULL;
//		}
		if(firstPage == -1){
			//this means that the first page found is to far along in the 
			firstPage = findEvictIndex(pageCount);
//			printf("firstpage: %d\n", firstPage);

			int status = -1;
 			if(firstPage == -1){
 				return NULL;
 			}
 			else{
 				int i = 0;
 				int j = firstPage;
 				while(i < pageCount){
// 					printf("%d\n", i);
 					status = evictPage(j);
// 					printf("status: %d\n", status);
 					if(status == -1){
 						my_pthread_mutex_unlock(mutexMalloc);
 						return NULL;
 					}
 					else{
 						PTE* pageTable = threads[EPT[j][0]]->pageTable;
 						while(pageTable){
 							if(pageTable->pageIndex == j){
 								pageTable->offset = status; //stores offset in pageTable
 								break;
 							}
 							pageTable = pageTable->next;
 						}
						EPT[j][0] = -2;	
 					}
 					j++;
 					i++;
 				}
 			}
		}	

//		printf("firstpage again: %d\n", firstPage);			
		memNew = (memStruct*)((long)memory + OSSIZE +(firstPage * PAGESIZE )); //This line is the cause of about an hour worth of segfaulting		
		memNew->pageCount = pageCount;
		memNew->currUsed = size + sizeof(memStruct); //MIKE: why plus sizeof(memStruct)?  What are you doing with it? ANNE, It's just saying how many bytes that this specific malloc has used in the page, if you're not using it in other places I think it'd be best to just leave it, I see what you're saying but I believe I have some logic that uses it and it's working, let me know if you're using it and boy this is long, huh?
		memNew->inUse = TRUE;
		memNew->next = NULL; //i think only important for internal mallocs
		if (memHead == NULL){
			memHead = memNew;
		}
		int i = firstPage;
		while (i < (long)(pageCount + firstPage)){
			EPT[i][0] = tid;
			EPT[i][1]++;
			i++;					
		}
		
		PTE * threadPTE = (PTE*)myallocate(sizeof(PTE), __FILE__, __LINE__, LIBREQ);
		if (threadPTE == NULL){
			my_pthread_mutex_unlock(mutexMalloc);
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
		int iter = firstPage;	//only zero because this is the first creation
		while ( iter < (firstPage + pageCount)-1){//while loop for all but last entry
			threadPTE->pageIndex = (iter);
			threadPTE->maxSize = 0;
			threadPTE->offset = -1;
			iter++;
			threadPTE->next = (PTE*)myallocate(sizeof(PTE), __FILE__, __LINE__, LIBREQ);
			if (threadPTE == NULL){
				my_pthread_mutex_unlock(mutexMalloc);
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
		my_pthread_mutex_unlock(mutexMalloc);
		return retStr;
	}
	return NULL;
}

//replace free
void mydeallocate(void* ptr, char* file, int line, int threadId){
//TODO: change free to -2 so seg fault happens for threads that may have been swapped out.

//	printf("Free stuff: %p\n",ptr);
	if(!ptr ){//|| ((metaData*)((long)ptr - sizeof(metaData)))->used == 0){
		return;
	}
	if(!threadId){//	 TODO: (PUTTING THIS TODO IN TWO PLACES[OTHER IN TOP OF MALLOC]) Add changes/fix from old file
//		printf("Free coming from library. given: %p freed:%p\n", ptr, (metaData*)(((long)ptr)-sizeof(metaData)));
		((metaData*)((long)ptr - sizeof(metaData)))->used = 0;
//		printf("os freed\n");
	}
	else{
		int tid = -1;
		if(currentRunning){
			tid = currentRunning->tid;
		}
		else{
			tid = 0;
		}
		

		//		printf("Free coming from thread number %d\n",tid);
		//add logic for shalloc
		if ((long)ptr - USERSIZE > (long)memory + OSSIZE){
		//	printf("free from shalloc\n");
			((metaData*)((long)ptr - sizeof(metaData)))->used = 0;	
//			printf("shalloc freed\n");
			return;
		}

		//userspace free
		if (((memStruct*)((long)ptr - sizeof(memStruct)))->inUse == TRUE ){
			int pageCount = ((memStruct*)((long)ptr - sizeof(memStruct)))->pageCount;
			int indexStart = ((long) ((memStruct*)((long)ptr - sizeof(memStruct))) - ((long)memory + OSSIZE))/ PAGESIZE;
			int lastPage = indexStart + pageCount;
			//if there are more mallocs being used on the last page, decrement the counter and return;
			if (EPT[lastPage][0] > 1){
				EPT[lastPage][1]--;
				return;
			}
			PTE * freePTE = threads[tid]->pageTable;
			((memStruct*)((long)ptr - sizeof(memStruct)))->inUse = FALSE;		
		 	//Resetting page table index to -1
			
			int i = indexStart;
			bool checked = FALSE;
			//MIKE TODO: shouldn't be freeing last page or resetting EPT of last page if smaller mallocs happened inside of it.
			//TODID: change internal page max size
			while (i < (long)(pageCount + indexStart)){ //while loop to get to right page index
				
				if (checked == FALSE){
					while(indexStart != freePTE->pageIndex){
//						printf("moving");
						freePTE = freePTE->next;
					}
					checked = TRUE;
				}
				freePTE->maxSize = PAGESIZE;
				freePTE = freePTE->next;

				EPT[i][0] = -2; //this way we keep it mprotected so that if returning thread wants it, it segfaults and brings it in.
				EPT[i][1] = 0;
				i++;	
				
			}
//			printf("user section freed\n");
		}
		else{
//			printf("could not find anything to free\n");
		}		
	}
	return;
}

//shared malloc
void* shalloc(int size){
	//if no head is set yet
	if(!headSh){
//		printf("head stuff\n");
		shcurrMD = (metaData*)((long)memory + OSSIZE + USERSIZE);
		shcurrMD->used = 1;
		shcurrMD->bytes = size;
		shcurrMD->ptr = NULL;
		headSh = shcurrMD;
	}
	//if head has been used and then freed.
	else if (headSh->used == 0){
//		printf("head was freed\n");
		shcurrMD = headSh;
		shiterMD = shcurrMD;
		int bytesFree = combineFreeStuff(shiterMD);
		if(bytesFree > 0){
		//	printf("bytes free: %d\n", bytesFree);
		}
		
		//if it's at least as big as space, but less than enough space to put metadata
		if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
			shcurrMD->used = 1;
			//printf("Asked for %d, got %d\n",size,shcurrMD->bytes);
		}
		//if it's bigger than the space and there's enough room for metadata
		else if(bytesFree > size){
			spaceBetween(shcurrMD, size, bytesFree);
		}
		//won't fit
		else{
			//set currentMD to head;
			shprevMD = headSh;
			shcurrMD = headSh;
				//while ptr != NULL, move forward.
			while(shcurrMD->ptr){ 
				shprevMD = shcurrMD;
				shcurrMD = (metaData*)((long)shcurrMD + sizeof(metaData) + shcurrMD->bytes);
				if(shcurrMD->used == 0){
					shiterMD = shcurrMD;
					int bytesFree = combineFreeStuff(shiterMD);
					//if there are enough bytes free to squeeze it in, but not enough for metadata, give them all
					if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
						shcurrMD->used = 1;	
					//	printf("Asked for %d,  got %d\n",size,shcurrMD->bytes);
						return (void*)((long)shcurrMD + sizeof(metaData));
					}
					//if it's bigger than the space and there's enough room for metadata, put it in and put in metadata for rest
					else if(bytesFree > size){
						spaceBetween(shcurrMD, size, bytesFree);
						return (void*)((long)shcurrMD + sizeof(metaData));
					}
					//else if it won't fit, keep looping through.
				}
			}
			//do it all one more time forward, loop broke when PTR was null, so get to that free space
			shprevMD = shcurrMD;
			shcurrMD = (metaData*)((long)shcurrMD + sizeof(metaData) + shcurrMD->bytes);
			
			//Please don't let this happen, 1MB should be plenty
			if(((long)shcurrMD - (long)headSh + sizeof(metaData) + size) > (SHAREDSIZE)){
				printf("Not enough space in shared section.\n");
				return NULL;
			}
			//if it didn't return ^^up there, then use it!
			shcurrMD->used = 1;
			shcurrMD->bytes = size;
			shcurrMD->ptr = NULL;
			shprevMD->ptr = shcurrMD;
		}
		//return the pointer
		return (void*)((long)shcurrMD + sizeof(metaData));
	}
	//head is being used currently
	else{
		//set currentMD to head;
		shprevMD = headSh;
		shcurrMD = headSh;
		//while ptr != NULL, move forward.
		while(shcurrMD->ptr){ 
			shprevMD = shcurrMD;
			shcurrMD = (metaData*)((long)shcurrMD + sizeof(metaData) + shcurrMD->bytes);
			if(shcurrMD->used == 0){
//				printf("found one that was freed\n");
				shiterMD = shcurrMD;
				int bytesFree = combineFreeStuff(shiterMD);
				if(bytesFree > 0){
					//printf("bytes free: %d\n", bytesFree);
				}				
				if(bytesFree >= size && bytesFree < size + sizeof(metaData)){
					shcurrMD->used = 1;
					//printf("Asked for %d, got %d\n",size,shcurrMD->bytes);
					return (void*)((long)shcurrMD + sizeof(metaData));
				}
				//if it's bigger than the space and there's enough room for metadata
				else if(bytesFree > size){
					spaceBetween(shcurrMD, size, bytesFree);
					//printf("ok\n");
					return (void*)((long)shcurrMD + sizeof(metaData));
				}
				//else if it won't fit, keep looping through.
			}
		}
		shprevMD = shcurrMD;
		shcurrMD = (metaData*)((long)shcurrMD + sizeof(metaData) + shcurrMD->bytes);
		//printf("forward one last time, %d/%d bytes used\n", ((long)shcurrMD + sizeof(metaData) + size) - (long)headSh, OSSIZE);
			//Please don't let this happen, 1MB should be plenty
		if(((long)shcurrMD - (long)headSh + sizeof(metaData) + size) > (SHAREDSIZE)){
			printf("Not enough space in shared section.\n");
			return NULL;
		}
			//if it didn't return ^^up there, then use it!
		shcurrMD->used = 1;
		shcurrMD->bytes = size;
		shcurrMD->ptr = NULL;
		shprevMD->ptr = shcurrMD;
	}
// 	printf("memory: %d, %p\n headOS: %d, %p\n memHead: %d, %p\n headSh: %d, %p\n shcurrMD: %d, %p\n", (long)memory, memory, (long)headOS, headOS, (long)memHead, memHead, (long)headSh, headSh, (long)shcurrMD, shcurrMD);
//	printf("OS: %d, %p\nUser: %d, %p\nShared: %d, %p\n", (long)memory, memory, (long)memory + OSSIZE, (void*)((long)memory + OSSIZE), (long)memory + OSSIZE + USERSIZE, (void*)((long)memory + OSSIZE + USERSIZE));
//	printf("about to return %p, %d + %d\n",(void*)((long)shcurrMD + sizeof(metaData)), (long)shcurrMD, sizeof(metaData));
//	printf("----MEMORYsSIZE:\t%d----\n----OSSIZE:\t%d----\n----USERSIZE:\t%d----\n----SHAREDSIZE:\t%d----\n----TOTAL SIZE:\t%d----\n", MEMORYSIZE, OSSIZE, USERSIZE, SHAREDSIZE, OSSIZE + USERSIZE + SHAREDSIZE);
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
			exit_thread(manualExit, NULL);   
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
	//unmprotect everything
	mprotect((void*)((long)memory + OSSIZE), USERSIZE, PROT_READ | PROT_WRITE); //allow read and write to entire array
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
//TODO: free pageTable, reset EPT[] and swapIndex[]
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
		//mprotect only pages that do not == -1 and do not == tid, let -2 be mprotected so it can segfault
		int i = 0;
		while(i < NUMOFPAGES){
			if(EPT[i][0] != -1 && EPT[i][0] != currentRunning->tid){
//				printf("i: %d, fullsize: %d\n", i, (NUMOFPAGES));
				mprotect((void*)((long)memory + OSSIZE + PAGESIZE * i), PAGESIZE, PROT_NONE);
			}
			i++;
		}
//		printf("after while\n");
		timer(threads[currentRunning->tid]->priority);
		if((swapcontext(&(sched_uctx),&((threads[currentRunning->tid])->context))) != 0){
			return -2;
		}
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